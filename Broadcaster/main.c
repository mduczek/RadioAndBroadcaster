//
//  main.c
//  Broadcaster
//
//  Created by Duczi on 03.07.2013.
//  Copyright (c) 2013 Michal Duczynski. All rights reserved.
//

#include "../basic_libs.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>

#include "../utils.h"
#include "../packet.h"
#include "../cyclic_buff.h"
#include "../rdbuff.h"


//=================ID============================
app_id app_identifier = { "broadcaster" , '1' };
packet_id br_id_packet;

//================Configuration===================
char br_mcast_str [INET_ADDRSTRLEN] = "224.0.0.1";
uint16_t br_data_port = 20321;
uint16_t br_ctrl_port = 30321;
uint16_t br_psize = 512;
uint32_t br_fsize = 128000;
uint16_t br_rtime = 250;
char br_name [packet_max_broadcaster_name] = "Unnamed";


//===================Threads=======================
pthread_t th_mode_changer;
pthread_t th_ctrl_port;
//main thread is th_data_port
//===================Globals=======================
enum Mode {
    TRANSMIT = 0,
    RETRANSMIT = 1
};

enum Mode br_mode = TRANSMIT;

int br_ctrl_udp_socket;

int br_data_udp_socket;
struct sockaddr_in br_data_sockaddr;
struct sockaddr_in br_data_mcast_addr;

rdbuff br_fifo;

typedef struct _br_fifo_item {
    char retransmit;
    char data_first_byte;
} br_fifo_item;

pthread_mutex_t br_fifo_mutex;

bool br_ending;

//======================CODE========================

void catch_int (int sig) {
    br_ending = true;
    debug("Signal %d catched.", sig);
    shutdown(STDIN_FILENO, SHUT_RDWR);
    close(STDIN_FILENO);
}

void cleanup(){
    shutdown(br_ctrl_udp_socket, SHUT_RDWR);
    close(br_ctrl_udp_socket);
    pthread_join(th_ctrl_port, NULL);
    
    pthread_join(th_mode_changer, NULL);
    
    pthread_mutex_destroy(&br_fifo_mutex);
    
    rdbuff_destroy(&br_fifo);
}


void* thread_mode_changer(void* _param){
    struct timespec tspec;
    tspec.tv_nsec = br_rtime * 1000;
    tspec.tv_sec = 0;
    enum Mode last = br_mode;
    while (!br_ending) {
        nanosleep(&tspec, NULL);
        last = (last == TRANSMIT) ? RETRANSMIT : TRANSMIT ;
        br_mode = last;
    }
    debug("th_mode_changer end");
    return 0;
}

void* thread_ctrl_port(void* _param){
    packet_id_init(&br_id_packet);
    strncpy(br_id_packet.broadcaster_name, br_name, sizeof(br_id_packet.broadcaster_name)-1);
    br_id_packet.app = app_identifier;
    strncpy(br_id_packet.multicast, br_mcast_str, sizeof(br_id_packet.multicast));
    br_id_packet.max_packet_size = br_psize;
    br_id_packet.data_port = br_data_sockaddr.sin_port;
    packet_id_hton(&br_id_packet);

    br_ctrl_udp_socket = udp_socket_and_bind(br_ctrl_port);
    
    ssize_t resplen;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    ssize_t snd_len;
    
    cbuff_item_container* container = cbuff_item_container_new(br_fifo.buff);
    packet_data_header pdata;
    packet_id_request preq;
    packet_header_buff buffer;
    do {
        resplen = recvfrom(br_ctrl_udp_socket, buffer, sizeof(buffer), 0,
                           (struct sockaddr *) &client_addr, &client_addr_len);
        if (resplen < 0 && !br_ending){
            perror("error on datagram from client socket");
        } else if(resplen>0){
            
            hexdump(buffer, (size_t) resplen);
            
            if (buffer[0] == packet_type_id_request) {
                if (resplen != sizeof(packet_id_request)) {
                    debug("invalid size");
                }else{
                    memcpy(&preq, &buffer[0], sizeof(preq));
                    packet_id_request_ntoh(&preq);
                    //hexdump(&preq, sizeof(preq));
                    snd_len = sendto(br_ctrl_udp_socket, &br_id_packet, sizeof(br_id_packet), 0,
                                             (struct sockaddr *) &client_addr, client_addr_len);
                    if (snd_len != sizeof(br_id_packet)){
                        perror("error on sending datagram to aether");
                    }
                    
                }
            }else if(buffer[0] == packet_type_retransmit){
                if (resplen != sizeof(packet_data_header)) {
                    debug("invalid size");
                }else{
                    memcpy(&pdata, buffer, sizeof(pdata));
                    packet_data_header_ntoh(&pdata);
                    
                    pthread_mutex_lock(&br_fifo_mutex);
                    if(rdbuff_item_get(&br_fifo, pdata.serial, container)){
                        br_fifo_item* it = (br_fifo_item*) container->data;
                        it->retransmit = 'Y';
                        rdbuff_item_set(&br_fifo, pdata.serial, container->data, container->size);               
                    } else debug("invalid serial");
                    pthread_mutex_unlock(&br_fifo_mutex);
                    
                }
            }else debug("dropping packet");
        }
    } while (!br_ending);
    
    cbuff_item_container_del(container);
    debug("th_ctrl_port end");
    return 0;
}

void thread_data_port(){
    
    ssize_t res;
    char read_buffer[br_psize + sizeof(br_fifo_item)-1];
    char send_buffer[br_psize + sizeof(packet_data_header)];
    packet_data_header dh;
    cbuff_item_container* container = cbuff_item_container_new(br_fifo.buff);

    do {
        char* ptr = &read_buffer[ offsetof(br_fifo_item, data_first_byte) ];
        size_t space_left = br_psize;
        
        if (br_mode == TRANSMIT) {
            while (space_left != 0) {
                res = read(STDIN_FILENO, ptr, space_left);
                if (res < 0 ){
                    if(br_ending) break;
                    else syserr("reading");
                }
                if (res == 0 ) break;//EOF
                space_left -= (size_t) res;
                ptr += (size_t) res;
            }
            if (res == 0 || br_ending) break;//EOF
            //add to fifo
            uint16_t serial;
            pthread_mutex_lock(&br_fifo_mutex);
            if (rdbuff_full(&br_fifo)) {
                rdbuff_pop_front(&br_fifo, NULL);
            }
            br_fifo_item* item = (br_fifo_item*) read_buffer;
            item->retransmit = 'N';
            rdbuff_push_back(&br_fifo, read_buffer, sizeof(read_buffer), &serial);

            pthread_mutex_unlock(&br_fifo_mutex);
            
            packet_data_init(&dh);
            dh.serial = serial;
            packet_data_header_hton(&dh);
            memcpy(send_buffer, &dh, sizeof(dh));
            memcpy(send_buffer+sizeof(dh),&item->data_first_byte, br_psize);
        }else{
            //br_mode == RETRANSMIT
            bool foundForRetransmit = false;
            pthread_mutex_lock(&br_fifo_mutex);
            for (uint16_t sr = br_fifo.begin_serial; sr < rdbuff_end_serial(&br_fifo); ++sr) {
                if(rdbuff_item_get(&br_fifo, sr, container)){
                    br_fifo_item* it = (br_fifo_item*) container->data;
                    if(it->retransmit == 'Y'){
                        foundForRetransmit = true;
                        it->retransmit = 'N';
                        rdbuff_item_set(&br_fifo, sr, container->data, container->size);
                        packet_data_init(&dh);
                        dh.serial = sr;
                        packet_data_header_hton(&dh);
                        memcpy(send_buffer, &dh, sizeof(dh));
                        memcpy(send_buffer+sizeof(dh), &it->data_first_byte, br_psize);
                        break;
                    }
                }else debug("oops");
            }
            pthread_mutex_unlock(&br_fifo_mutex);
            if(!foundForRetransmit) {br_mode = TRANSMIT; continue;}
        }
        //FIXME:del
        hexdump(send_buffer, sizeof(send_buffer));
        //send
        ssize_t snd_len = sendto(br_data_udp_socket, send_buffer, sizeof(send_buffer), 0,
                                 (struct sockaddr *) &br_data_mcast_addr,
                                 (socklen_t) sizeof(br_data_mcast_addr));
        if ((size_t) snd_len != sizeof(send_buffer)){
            perror("error on sending datagram to client socket");
        }
        
    } while (!br_ending);
    debug("th_data_port end");
    br_ending = true;
    cleanup();
    
}

void parse_args(int argc, char * argv[]);

int main(int argc, const char * argv[])
{
    if (signal(SIGINT, catch_int) == SIG_ERR) {
        perror("Unable to change signal handler\n");
        exit(EXIT_FAILURE);
    }
    
    parse_args(argc, (char**) argv);
    
    rdbuff_init(&br_fifo, br_fsize);
    rdbuff_format(&br_fifo, br_psize+sizeof(br_fifo_item)-1);
    
    /*br_data_udp_socket = udp_socket_and_bind(br_data_port);
    
    struct ip_mreqn mreq;
    add_multicast_membership(br_data_udp_socket, &br_data_mcast_addr.sin_addr, &mreq);*/
    //br_data_udp_socket = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
    //if (br_data_udp_socket < 0) syserr("error creating crtl socket");
    br_data_udp_socket = udp_socket_and_bind(0);

    struct in_addr iaddr;
    iaddr.s_addr = INADDR_ANY;
    if(setsockopt(br_data_udp_socket, IPPROTO_IP, IP_MULTICAST_IF, &iaddr,
               sizeof(struct in_addr))==-1) syserr("setsockopt");
    unsigned char one = 1;
    if(setsockopt(br_data_udp_socket, IPPROTO_IP, IP_MULTICAST_LOOP,
                        &one, sizeof(unsigned char))==-1) syserr("setsockopt");
    addr_struct_from(br_mcast_str, br_data_port, &br_data_mcast_addr);
    
    socklen_t sock_len = sizeof(br_data_sockaddr);
    if(getsockname(br_data_udp_socket, (struct sockaddr *) &br_data_sockaddr, &sock_len) !=0)
        syserr("err getting sockaddr");
    
    pthread_mutex_init(&br_fifo_mutex, 0);
    
    pthread_attr_t th_attr;
    pthread_attr_init(&th_attr);
    pthread_attr_setdetachstate(&th_attr, PTHREAD_CREATE_JOINABLE);
    
    int rc;
    
    rc = pthread_create(&th_mode_changer, &th_attr, thread_mode_changer, NULL);
    if (rc==-1) syserr("pthread_create");
    
    
    rc = pthread_create(&th_ctrl_port, &th_attr, thread_ctrl_port, NULL);
    if (rc==-1) syserr("pthread_create");
    
    thread_data_port();

    
    return 0;
}



void parse_args(int argc, char * argv[]){
    int c;
    bool mcast_set = false;
    while ((c = getopt (argc, argv, "a:P:C:p:f:R:n:")) != -1)
        switch (c)
    {
        case 'a':
            if (inet_addr(optarg) == INADDR_NONE) syserr("invalid mcast addr");
            else strncpy(br_mcast_str, optarg, sizeof(br_mcast_str));
            mcast_set = true;
            break;
        case 'P':
             if(sscanf(optarg, "%hu",&br_data_port)<=0) syserr("invalid data port");
            break;
        case 'C':
            if(sscanf(optarg, "%hu",&br_ctrl_port)<=0) syserr("invalid ctrl port");
            break;
        case 'p':
            if(sscanf(optarg, "%hu",&br_psize)<=0) syserr("invalid psize");
            break;
        case 'f':
            if(sscanf(optarg, "%u",&br_fsize)<=0) syserr("invalid fsize");
            break;
        case 'R':
            if(sscanf(optarg, "%hu",&br_rtime)<=0) syserr("invalid rtime");
            break;
        case 'n':
            strncpy(br_name, optarg, sizeof(br_name));
            break;
        case '?':
            syserr("all opts take an argument, following are allowed aPCpfRn");
        default:
            abort ();
    }

    if (!mcast_set) {
        syserr("required arg (mcast_addr) -a was not set");
    }
    
    debug("%s, d %d, c %d, p %d, f %d, r %d, %s",br_mcast_str, br_data_port,br_ctrl_port,br_psize,br_fsize,br_rtime, br_name);
}
