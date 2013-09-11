CC=gcc
CFLAGS=-Wall -std=gnu99
DEPS = packet.h rdbuff.h cyclic_buff.h utils.h
OBJ = packet.o rdbuff.o cyclic_buff.o utils.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

nadajnik: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) ./Broadcaster/main.c -lpthread

odbiornik: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) ./Radio/stations.c ./Radio/main.c -lpthread

.PHONY: clean

clean:
	rm -f *.o *~