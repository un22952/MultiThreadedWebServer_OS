//webserver_multi.c
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "webserver.h"
#include <stdlib.h>

#define MAX_REQUEST 100

int port, numThread;

void *listener()
{
	int r;
	struct sockaddr_in sin;
	struct sockaddr_in peer;
	int peer_len = sizeof(peer);
	int sock;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);
	r = bind(sock, (struct sockaddr *) &sin, sizeof(sin));
	if(r < 0) {
		perror("Error binding socket:");
		return;
	}

	r = listen(sock, 5);
	if(r < 0) {
		perror("Error listening socket:");
		return;
	}

	printf("HTTP server listening on port %d\n", port);
	while (1)
	{
		int s;
		s = accept(sock, NULL, NULL);
		if (s < 0) break;

		//process(s);
	}

	close(sock);
}

void thread_control()
{
	/* ----- */
}

int main(int argc, char *argv[])
{
	if(argc != 3 || atoi(argv[1]) < 2000 || atoi(argv[1]) > 50000)
	{
		fprintf(stderr, "./webserver_multi PORT(2001 ~ 49999) #_of_threads\n");
		return 0;
	}

	int i;
	port = atoi(argv[1]);
	numThread = atoi(argv[2]);
	thread_control();
	return 0;
}

