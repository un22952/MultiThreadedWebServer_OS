CC = gcc
CFLAGS = -g -lpthread

all: webserver webserver_multi client wm

webserver: webserver.c net.c webserver.h
	$(CC) $(CFLAGS) -o $@ webserver.c net.c

webserver_multi: webserver_multi.c net.c webserver.h
	$(CC) $(CFLAGS) -o $@ webserver_multi.c net.c

client: client2.c
	$(CC) $(CFLAGS) -o $@ client2.c

wm: wm.c net.c webserver.h
	$(CC) $(CFLAGS) -o $@ wm.c net.c

clean:
	rm -f webserver webserver_multi client wm

