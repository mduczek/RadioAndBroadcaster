README:

Format nagłówków zdefiniowany w 'packet.h'

struct app_id{
    char app_name [12]; //nazwa aplikacji
    char version; //wersja
}

//prośba o identyfikację wysłana przez aplikację app
struct packet_id_request{
    char type = 'w';
    app_id app;
};

//odpowiedź nadajnika na prośbę o identyfikację
struct packet_id {
    char type = 'i';
    app_id app;
    char broadcaster_name[packet_max_broadcaster_name];
    uint16_t max_packet_size;
    char multicast[INET_ADDRSTRLEN];
    in_port_t data_port;
};

//nagłówek z jakim są przesyłane dane
struct packet_data_header {
    char type = 'd';
    uint16_t serial;//unikalny numer paczki
};

//prośba o retransmisję
struct packet_data_header {
    char type = 't'; //zamiast 'd'
    uint16_t serial;//unikalny numer paczki
};



Nadajnik:
Wątek th_data czyta dane z stdin wkłada je do fifo i wysyła na adres
MULTICAST. 
Dokładnie co RTIME przechodzi w tryb RETRANSMIT i przegląda fifo
w poszukiwaniu paczek oznaczonych do retransmisji, po zakończeniu 
przełącza się w tryb TRANSMIT.

Wątek th_control odbiera prośby o identyfikację lub o retransmisję,
wysyła swoje ID na adres nadawcy UNICAST.

Wątek th_mode_changer dokładnie co RTIME przełącza nadajnik na tryb
retransmisji.


Odbiornik:
Bufor jest zaimplementowany jako fifo. Jest to dokładnie ta sama fifo,
co w nadajniku.

Wątek th_discovery_sender dokładnie co 5s (na początku co 0.5s)
wysyła prośbę o identyfikację na BROADCAST adres.

Wątek th_discovery_listener odbiera ID nadajników i uaktualnia 
listę stacji. Jeśli jest to pierwsza stacja, którą odkryliśmy 
rozpoczynamy odtwarzanie.

Wątek th_data_listener odbiera dane sprawdza czy nadawca paczki zgadza
się z tym, którego obecnie słuchamy, jeśli tak to wrzuca dane do bufora
i sygnalizuje, że są nowe dane. Jeśli nie ma miejsca w buforze, 
usuwa najstarszą paczkę z początku i wrzuca nową na koniec.

Wątek th_data_output pracuje w dwóch trybach: buforowania i wypisywania.
W trybie wypisywania usuwa paczkę z bufora i wypisuje na stdout.
Po natrafieniu na lukę przechodzi w tryb buforowania, czeka na zmiennej
warunkowej na sygnał, że pojawiły się nowe dane, sprawdza, czy jest
jednolite 75% pojemności bufora i przechodzi w tryb wypisywania.

Wątek th_retransmission_requester co RTIME przegląda bufor, oznacza
brakujące paczki, a o te, które zostały oznaczone przy poprzednim
sprawdzaniu, prosi. (dzięki temu o paczkę prosi dopiero po t > RTIME)

Wątek th_ui prezentuje interfejs użytkownikom, przy zmianie
listy stacji, nowy widok jest wysyłany do wszystkich użytkowników.
Należy użyć polecenia:
telnet -4 [host] 10321
//wymagane ustawienie: mode char

