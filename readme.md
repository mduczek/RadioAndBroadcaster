RadioAndBroadcaster
===================

The project consists of two apps:

###Broadcaster (Sender)
Reads data from file and streams it to IPv4 multicast group. It has an input buffer and is able to retransmit requested packets of data.

Required run parameters:
multicast group address

Optional run params:
packet size


###Radio (Receiver)
Behaves like a radio, listens for 'stations' broadcasting, you can chose which one to listen via telnet. If it misses a packet it asks for retransmission.

There are 6 threads.
- send 'identification request' and listen for response, add to station list
- telnet ui
- receive data sent to multicast group
- ask for retransmission
- output data
