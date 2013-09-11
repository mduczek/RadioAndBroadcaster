//
//  main.c
//  Radio
//
//  Created by Duczi on 12.07.2013.
//  Copyright (c) 2013 Michal Duczynski. All rights reserved.
//

#include "../basic_libs.h"
#include <netdb.h>
#include <time.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

#include "../utils.h"
#include "../packet.h"
#include "../cyclic_buff.h"
#include "../rdbuff.h"

#include "stations.h"

//===============Const============================

const char arrow_up [4] = {'\x1B','\x5B','\x41','\xFF'};
const char arrow_down [4] = {'\x1B','\x5B','\x42','\xFF'};


app_id app_identifier = { "radio" , '1' };

//================Configuration===================

#define UI_CONN_MAX 10

char rd_discover_str [INET_ADDRSTRLEN] = "255.255.255.255";
uint16_t rd_data_port = 20321;
uint16_t rd_ctrl_port = 30321;
uint16_t rd_ui_port = 10321;
uint32_t rd_bsize = 128000;
uint16_t rd_rtime = 250;
char rd_waiting_for [packet_max_broadcaster_name] = "";

//===================Threads=======================
pthread_t th_discover_sender;
pthread_t th_discover_listener;
pthread_t th_data_listener;
pthread_t th_data_output;
pthread_t th_retransmission_requester;
//the main thread is the ui thread

//===================Globals=======================

bool rd_ending = false;

int rd_ctrl_udp_socket;
struct sockaddr_in rd_ctrl_dscvr_addr;

int rd_data_udp_socket;
struct ip_mreqn rd_data_multicast_membership;

stations rd_stations;

rdbuff rd_buff;
pthread_mutex_t rd_buff_mutex;
pthread_cond_t rd_buff_cv;

pthread_mutex_t rd_retransmit_mutex;
pthread_cond_t rd_retransmit_cv;

//======================CODE========================

void catch_int (int sig) {
    rd_ending = true;
    debug("Signal %d catched. No new connections will be accepted.\n", sig);
}

void cleanup(){
    
    shutdown(rd_ctrl_udp_socket, SHUT_RDWR);
    close(rd_ctrl_udp_socket);
    pthread_join(th_discover_sender, NULL);
    pthread_join(th_discover_listener, NULL);
    
    shutdown(rd_data_udp_socket, SHUT_RDWR);
    close(rd_data_udp_socket);
    debug("waiting for data");
    pthread_join(th_data_listener, NULL);
    
    pthread_mutex_lock(&rd_buff_mutex);
    pthread_cond_signal(&rd_buff_cv);
    pthread_mutex_unlock(&rd_buff_mutex);
    pthread_join(th_data_output, NULL);
    
    pthread_mutex_lock(&rd_retransmit_mutex);
    pthread_cond_signal(&rd_retransmit_cv);
    pthread_mutex_unlock(&rd_retransmit_mutex);
    pthread_join(th_retransmission_requester, NULL);
    
    sta_destroy(&rd_stations);
    pthread_mutex_destroy(&rd_buff_mutex);
    pthread_cond_destroy(&rd_buff_cv);
    rdbuff_destroy(&rd_buff);
    
    pthread_mutex_destroy(&rd_retransmit_mutex);
    pthread_cond_destroy(&rd_retransmit_cv);
    
}

void* thread_discover_sender(void * _param){
    char initial = 5;
    struct timespec tspec;
    tspec.tv_nsec =  500 * 1000 * 1000; //0.5s
    tspec.tv_sec = 0;
    
    packet_id_request preq;
    packet_id_request_init(&preq);
    preq.app = app_identifier;
    packet_id_request_hton(&preq);
    
    while (!rd_ending) {
        nanosleep(&tspec, NULL);
        if (initial > 0) {
            initial--;
            if (initial==0) {
                tspec.tv_nsec = 0;
                tspec.tv_sec = 5;
            }
        }
        ssize_t snd_len = sendto(rd_ctrl_udp_socket, &preq, sizeof(preq), 0,
                                 (struct sockaddr *) &rd_ctrl_dscvr_addr,
                                 (socklen_t) sizeof(rd_ctrl_dscvr_addr));
        
        debug("sent");
        if (snd_len != sizeof(preq) && !rd_ending){
            perror("error on sending datagram to aether");
        }

        pthread_mutex_lock(&rd_stations.mutex);
        sta_time_unit_passed(&rd_stations);
        pthread_mutex_unlock(&rd_stations.mutex);
    }
    debug("th_discover_sender ended");
    return 0;
}

void* thread_discover_listener(void * _param){
    ssize_t resplen;
    packet_id id_pack;
    
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    
    char buffer [sizeof(packet_id)+10];
    while (!rd_ending) {
        resplen = recvfrom(rd_ctrl_udp_socket, buffer, sizeof(packet_id), 0,
                           (struct sockaddr *) &server_addr, &server_addr_len);

        if (resplen < 0 && !rd_ending){
            perror("error on datagram from ctrl socket");
        } else if(resplen>0){
            hexdump(buffer, (size_t) resplen);
            
            if (buffer[0] == packet_type_id) {
                if (resplen != sizeof(packet_id)) {
                    debug("invalid size");
                }else{
                    memcpy(&id_pack, buffer, sizeof(id_pack));
                    packet_id_ntoh(&id_pack);
                    
                    bool not_active = rd_stations.list[rd_stations.current_i].expires_in ==0;
                    
					size_t station_i;
                    pthread_mutex_lock(&rd_stations.mutex);
                    station_i = sta_add(&rd_stations, &id_pack, &server_addr);
                    pthread_mutex_unlock(&rd_stations.mutex);
                    
                    bool waiting = (strlen(rd_waiting_for) > 0);
                    if (not_active &&
                        (!waiting ||
                        (waiting && strncmp(rd_waiting_for, id_pack.broadcaster_name, sizeof(rd_waiting_for))==0) )) {
						debug("added mcast");
						rd_stations.current_i = station_i;
                        //we are not waiting or it's the station we've been waiting for
                        strcpy(rd_waiting_for,"");
                        //begin to play===========
                        struct in_addr multiaddr;
                        multiaddr.s_addr = inet_addr(id_pack.multicast);
                        add_multicast_membership(rd_data_udp_socket, &multiaddr, &rd_data_multicast_membership);
                        pthread_mutex_lock(&rd_buff_mutex);
                        rdbuff_format(&rd_buff, id_pack.max_packet_size);
                        pthread_mutex_unlock(&rd_buff_mutex);
                        
                        pthread_mutex_lock(&rd_retransmit_mutex);
                        pthread_cond_signal(&rd_retransmit_cv);
                        pthread_mutex_unlock(&rd_retransmit_mutex);
                    }
                }
            }else{
                debug("dropping packet");
            }
        }

    }
    debug("th_discover_listener ended");
    return 0;
}


void* thread_data_listener(void * _param){
    ssize_t respln;
    char buffer [packet_max_size];

    cbuff_item_container* cont = cbuff_item_container_new(rd_buff.buff);
    
    struct sockaddr_in server_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    
    while (!rd_ending) {
        respln = recvfrom(rd_data_udp_socket, buffer,sizeof(buffer) , 0,
                           (struct sockaddr *) &server_addr, &server_addr_len);
        if (respln < 0 && !rd_ending){
            perror("error on datagram from data socket");
        } else if(respln>0){
            size_t resplen = (size_t) respln;
            hexdump(buffer, resplen);
            
            if (buffer[0] == packet_type_data && resplen > sizeof(packet_data_header)) {
                packet_data_header* dh = (packet_data_header*) buffer;
                packet_data_header_ntoh(dh);
                pthread_mutex_lock(&rd_stations.mutex);
                in_port_t current = sta_current(&rd_stations).data_port;
                pthread_mutex_unlock(&rd_stations.mutex);
                if (current == server_addr.sin_port ) {
                    //put into buffer
                    pthread_mutex_lock(&rd_buff_mutex);
                    while (dh->serial >= rdbuff_end_serial(&rd_buff)) {
                        rdbuff_pop_front(&rd_buff, NULL);
                    }
                    if (rdbuff_item_set(&rd_buff, dh->serial, buffer+sizeof(*dh), resplen-sizeof(*dh))){
                        /*(//print
                        for (uint16_t sr = rd_buff.begin_serial; sr < rdbuff_end_serial(&rd_buff); sr++ ) {
                            rdbuff_item_get(&rd_buff, sr, cont);
                            hexdump(cont->data, cont->size);
                        }*/
                        pthread_cond_signal(&rd_buff_cv);
                    }else debug("serial  %d is to small, begin serial = %d ", dh->serial, rd_buff.begin_serial );
                    pthread_mutex_unlock(&rd_buff_mutex);
                }else debug("wrong sender");
            }else debug("dropping packet, accepting only data packets");
        }
    }
    cbuff_item_container_del(cont);
    debug("th_data_listener ended");
    return 0;
}

bool rd_buff_uninterrupted_chunk(cbuff_item_container* cont){
    if(rd_buff.buff->max_length == 0) return false;
    uint16_t percent75 = rd_buff.begin_serial + (rd_buff.buff->max_length*3)/4;
    uint16_t sr;
    for (sr = rd_buff.begin_serial; sr < percent75; sr++ ) {
        rdbuff_item_get(&rd_buff, sr, cont);
        if (cont->size != sta_current(&rd_stations).max_packet_size) break;
    }
    return (sr == percent75);
}

void* thread_data_output(void * _param){
    cbuff_item_container* cont = cbuff_item_container_new(rd_buff.buff);
    while (!rd_ending) {
        pthread_mutex_lock(&rd_buff_mutex);
        while (!rd_ending && !rd_buff_uninterrupted_chunk(cont)) {
            pthread_cond_wait(&rd_buff_cv, &rd_buff_mutex);
        }
        pthread_mutex_unlock(&rd_buff_mutex);

        while (!rd_ending) {
            pthread_mutex_lock(&rd_buff_mutex);
            rdbuff_item_get(&rd_buff, rd_buff.begin_serial, cont);
            if (cont->size == sta_current(&rd_stations).max_packet_size){
                rdbuff_pop_front(&rd_buff, NULL);
                pthread_mutex_unlock(&rd_buff_mutex);
                
                ssize_t res = write(STDOUT_FILENO, cont->data, cont->size);
                if (res < 0 && !rd_ending) {
                    syserr("error writing to stdout");
                }
            }else{
                pthread_mutex_unlock(&rd_buff_mutex);
                break;
            }
        }
    }
    cbuff_item_container_del(cont);
    debug("th_data_output ended");
    return 0;
}

void* thread_retransmission_requester(void * _param){
    
    packet_data_header dhead;
    packet_retrasmit_init(&dhead);
    cbuff_item_container* cont = cbuff_item_container_new(rd_buff.buff);
    struct timespec tspec;
    tspec.tv_nsec =  rd_rtime * 1000 * 1000;
    tspec.tv_sec = 0;
    
    while (!rd_ending) {
        pthread_mutex_lock(&rd_retransmit_mutex);
        while (!rd_ending && rd_stations.list[rd_stations.current_i].expires_in==0 ) {
            pthread_cond_wait(&rd_retransmit_cv, &rd_retransmit_mutex);
        }
        pthread_mutex_unlock(&rd_retransmit_mutex);
        if (rd_ending) break;
        
        nanosleep(&tspec, NULL);
        
        //debug("retransmit");
        
        //find the packet with biggest number
        ssize_t sr;
        pthread_mutex_lock(&rd_buff_mutex);
        for (sr = rdbuff_end_serial(&rd_buff)-1; sr >= rd_buff.begin_serial; sr--) {
            rdbuff_item_get(&rd_buff, sr, cont);
            if (cont->size != 0) break;
        }
        pthread_mutex_unlock(&rd_buff_mutex);

        while( sr-- > rd_buff.begin_serial ) {
            pthread_mutex_lock(&rd_buff_mutex);
            rdbuff_item_get(&rd_buff, sr, cont);
            pthread_mutex_unlock(&rd_buff_mutex);
            if (cont->size == 0) {
                //a package is missing
                pthread_mutex_lock(&rd_buff_mutex);
                rdbuff_item_set(&rd_buff, sr, &rdbuff_retransmit_mark, sizeof(rdbuff_retransmit_mark));
                pthread_mutex_unlock(&rd_buff_mutex);
            }else if (*cont->data == rdbuff_retransmit_mark && cont->size == sizeof(rdbuff_retransmit_mark)){
                //retransmit
                dhead.serial = sr;
                packet_data_header_hton(&dhead);
                struct sockaddr_in* current = &rd_stations.list[rd_stations.current_i].ctrl_addr;
                ssize_t snd_len = sendto(rd_ctrl_udp_socket, &dhead, sizeof(dhead), 0,
                                         (struct sockaddr *) current,
                                         (socklen_t) sizeof(*current));
                
                debug("R %u", sr);
                if (snd_len != sizeof(dhead) && !rd_ending){
                    perror("error on sending datagram to aether");
                }
            }
        }
    }
    cbuff_item_container_del(cont);
    debug("th_retransmission_req ended");
    return 0;
}

void thread_ui(){
    struct pollfd client[UI_CONN_MAX];
    struct sockaddr_in server;
    ssize_t rval;
    int msgsock, activeClients, i, ret;
    
    /* Inicjujemy tablicę z gniazdkami klientów, client[0] to gniazdko centrali */
    for (i = 0; i < UI_CONN_MAX; ++i) {
        client[i].fd = -1;
        client[i].events = POLLIN;
        client[i].revents = 0;
    }
    activeClients = 0;
    
    /* Tworzymy gniazdko centrali */
    client[0].fd = socket(PF_INET, SOCK_STREAM, 0);
    if (client[0].fd < 0) {
        perror("Opening stream socket");
        exit(EXIT_FAILURE);
    }
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(rd_ui_port);
    
    // bind the socket to a concrete address
    if (bind(client[0].fd, (struct sockaddr*)&server,
             (socklen_t)sizeof(server)) < 0) {
        perror("Binding stream socket");
        exit(EXIT_FAILURE);
    }
    
    debug("Control connection at port %d\n",rd_ui_port);
    
    // switch to listening (passive open)
    if (listen(client[0].fd, 5) == -1) {
        perror("Starting to listen");
        exit(EXIT_FAILURE);
    }
    
    /* Do pracy */
    do {
        for (i = 0; i < UI_CONN_MAX; ++i)
            client[i].revents = 0;
        if ( rd_ending && client[0].fd >= 0) {
            if (close(client[0].fd) < 0)
                perror("close");
            client[0].fd = -1;
        }
        
        /* Wait for 5000 ms */
        ret = poll(client, UI_CONN_MAX, 5000);
        if (ret < 0)
            perror("poll");
        else if (ret > 0) {
            if ( !rd_ending && (client[0].revents & POLLIN)) {
                msgsock =
                accept(client[0].fd, (struct sockaddr*)0, (socklen_t*)0);
                if (msgsock == -1)
                    perror("accept");
                else {
                    for (i = 1; i < UI_CONN_MAX; ++i) {
                        if (client[i].fd == -1) {
                            client[i].fd = msgsock;
                            activeClients += 1;
                            break;
                        }
                    }
                    if (i >= UI_CONN_MAX) {
                        debug( "Too many control connections\n");
                        if (close(msgsock) < 0)
                            perror("close");
                    }
                }
            }
            for (i = 1; i < UI_CONN_MAX; ++i) {
                if (client[i].fd != -1
                    && (client[i].revents & (POLLIN | POLLERR))) {
                    //read char by char
                    char arrow_chars [4];
                    rval = read(client[i].fd, &arrow_chars, sizeof(arrow_chars));
                    if (rval < 0) {
                        perror("Reading stream message");
                        if (close(client[i].fd) < 0)
                            perror("close");
                        client[i].fd = -1;
                        activeClients -= 1;
                    }else if (rval == 0) {
                        debug( "Ending connection");
                        if (close(client[i].fd) < 0)
                            perror("close");
                        client[i].fd = -1;
                        activeClients -= 1;
                    }else{
                        bool changed = false;
                        if (strncmp(arrow_chars, arrow_up, 4)==0) {
                            debug("> up");
                            pthread_mutex_lock(&rd_stations.mutex);
                            changed = sta_previous(&rd_stations);
                            pthread_mutex_unlock(&rd_stations.mutex);
                            
                        }else if (strncmp(arrow_chars, arrow_down, 4)==0){
                            debug("> down");
                            pthread_mutex_lock(&rd_stations.mutex);
                            changed = sta_next(&rd_stations);
                            pthread_mutex_unlock(&rd_stations.mutex);
                        }
                        if (changed) {
                            drop_multicast_membership(rd_data_udp_socket, &rd_data_multicast_membership);
                            struct in_addr multiaddr;
                            multiaddr.s_addr = inet_addr(sta_current(&rd_stations).multicast);
                            add_multicast_membership(rd_data_udp_socket, &multiaddr,
                                                     &rd_data_multicast_membership);
                            pthread_mutex_lock(&rd_buff_mutex);
                            rdbuff_format(&rd_buff, sta_current(&rd_stations).max_packet_size);
                            pthread_mutex_unlock(&rd_buff_mutex);
                            break;
                        }
                    }
                    
                }
            }
        }
        size_t print_size = strlen(rd_stations.print_buff);
        ssize_t write_len;
        for (size_t j=1; j<UI_CONN_MAX; j++) {
            if (client[j].fd != -1) {
                write_len = write(client[j].fd, rd_stations.print_buff, print_size);
                if (write_len<0) perror("write");
                debug("%zd %zd %zd",j, client[j].fd, write_len);
                if ((size_t) write_len != print_size) {
                    debug("err on sending ui update");
                }
            }
            
        }
        
        /*else
         debug( "control: waiting");*/
    } while ( !rd_ending || activeClients > 0);
    
    if (client[0].fd >= 0)
        if (close(client[0].fd) < 0)
            perror("Closing main socket");
    
    cleanup();
    
    exit(EXIT_SUCCESS);

}

void parse_args(int argc, char ** argv);

int main(int argc, const char * argv[])
{
    if (signal(SIGINT, catch_int) == SIG_ERR) {
        perror("Unable to change signal handler\n");
        exit(EXIT_FAILURE);
    }
    
    parse_args(argc, (char**) argv);
    
    int rc;
    
    //let system assign a port for ctrl socket
    rd_ctrl_udp_socket = udp_socket_and_bind(0);
    addr_struct_from(rd_discover_str, rd_ctrl_port, &rd_ctrl_dscvr_addr);
    int so_broadcast = 1;
    rc = setsockopt(rd_ctrl_udp_socket, SOL_SOCKET, SO_BROADCAST, &so_broadcast, sizeof(so_broadcast));
    if (rc == -1) {
        syserr("setsockopt");
    }
    
    rd_data_udp_socket = udp_socket_and_bind(rd_data_port);
    
    sta_init(&rd_stations);
    rdbuff_init(&rd_buff, rd_bsize);
    
    pthread_mutex_init(&rd_buff_mutex, 0);
    pthread_cond_init (&rd_buff_cv, 0);
    
    pthread_mutex_init(&rd_retransmit_mutex, 0);
    pthread_cond_init (&rd_retransmit_cv, 0);
    
    pthread_attr_t th_attr;
    pthread_attr_init(&th_attr);
    pthread_attr_setdetachstate(&th_attr, PTHREAD_CREATE_JOINABLE);
    
    rc = pthread_create(&th_discover_sender, &th_attr, thread_discover_sender, NULL);
    if (rc==-1) syserr("pthread_create");
    
    rc = pthread_create(&th_discover_listener, &th_attr, thread_discover_listener, NULL);
    if (rc==-1) syserr("pthread_create");
    
    rc = pthread_create(&th_data_listener, &th_attr, thread_data_listener, NULL);
    if (rc==-1) syserr("pthread_create");
    
    rc = pthread_create(&th_data_output, &th_attr, thread_data_output, NULL);
    if (rc==-1) syserr("pthread_create");
    
    rc = pthread_create(&th_retransmission_requester, &th_attr, thread_retransmission_requester, NULL);
    if (rc==-1) syserr("pthread_create");
    
    thread_ui();
    
    return 0;
}

void parse_args(int argc, char ** argv){
    int c;

    while ((c = getopt (argc, argv, "d:P:C:U:b:R:n:")) != -1)
        switch (c)
    {
        case 'd':
            if (inet_addr(optarg) == INADDR_NONE) syserr("invalid discovery addr");
            else strncpy(rd_discover_str, optarg, sizeof(rd_discover_str));
            break;
        case 'P':
            if(sscanf(optarg, "%hu",&rd_data_port)<=0) syserr("invalid data port");
            break;
        case 'C':
            if(sscanf(optarg, "%hu",&rd_ctrl_port)<=0) syserr("invalid ctrl port");
            break;
        case 'U':
            if(sscanf(optarg, "%hu",&rd_ui_port)<=0) syserr("invalid ui port");
            break;
        case 'b':
            if(sscanf(optarg, "%u",&rd_bsize)<=0) syserr("invalid bsize");
            break;
        case 'R':
            if(sscanf(optarg, "%hu",&rd_rtime)<=0) syserr("invalid rtime");
            break;
        case 'n':
            strncpy(rd_waiting_for, optarg, sizeof(rd_waiting_for));
            break;
        case '?':
            syserr("all opts take an argument, following are allowed dPCUbR");
        default:
            abort ();
    }
}


