//webserver_multi.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "webserver.h"
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#define MAX_REQUEST 100

int port, numThread;

int in = 0, out = 0, count = 0, ind = 0;
int buf[MAX_REQUEST];

/* synchronization */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER; // buffer has item(s)
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER; // buffer has empty spaces

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
		exit(1);
	}

	r = listen(sock, 5);
	if(r < 0) {
		perror("Error listening socket:");
		exit(1);
	}

	printf("HTTP server listening on port %d\n", port);
	while (1)
	{
		
		int s = accept(sock, NULL, NULL);
		if (s < 0) {
			if (errno == EINTR) continue;
				perror("accept");
				continue;
			}
		if (s == 0) {
			fprintf(stderr, "Warning: accept() returned fd 0; closing and skipping\n");
			close(s);
			continue;
		}

        printf("[listener] accepted fd=%d\n", s);
		pthread_mutex_lock(&lock);

		while (count == MAX_REQUEST) {
			pthread_cond_wait(&not_full, &lock);
		}
		count++;
		// add req to the buffer
		buf[ind++] = s;
		in = (in + 1) % MAX_REQUEST;
		printf("[listener] accepted in buffer value =%d with in =%d\n", buf[ind - 1], ind - 1);
		pthread_cond_signal(&not_empty);
		pthread_mutex_unlock(&lock);
		
		
	}

	close(sock);
	return NULL;
}

void *worker()
{
	while(1) {
		
		pthread_mutex_lock(&lock);
		while (count == 0) {
			pthread_cond_wait(&not_empty, &lock);
		}
		count--;
		int s;
		s = buf[--ind];

		
		out = (out + 1) % MAX_REQUEST;
		//printf("[wk] accepted in buffer value =%d with out =%d\n", buf[out - 1], out - 1);
		printf("[wk] accepted in buffer value =%d with out =%d\n", buf[ind + 1], ind + 1);
		printf("[worker %lu] handling socket %d\n", pthread_self(), s);
		pthread_cond_signal(&not_full);
		pthread_mutex_unlock(&lock);
		
		process(s);
	}
	return NULL;
}

void thread_control()
{
	/* ----- */
}

int main(int argc, char *argv[])
{
	
	
	//buf = (int*)malloc(sizeof(int) * MAX_REQUEST);

	if(argc != 3 || atoi(argv[1]) < 2000 || atoi(argv[1]) > 50000)
	{
		fprintf(stderr, "./webserver_multi PORT(2001 ~ 49999) #_of_threads\n");
		return 0;
	}

	int i;
	port = atoi(argv[1]);
	numThread = atoi(argv[2]);
	if (numThread > 100) numThread = 100;
	pthread_t l;
	pthread_create(&l, NULL, listener, NULL);
	pthread_t * p = (pthread_t *)malloc(sizeof(pthread_t)* numThread);
	for (i = 0; i < numThread; i++) {
		pthread_create(&p[i], NULL, worker, NULL);
		
	} // for
	
	
	for (i = 0; i < numThread; i++) {
		pthread_join(p[i], NULL);
	} // for
	pthread_join(l, NULL);
	//thread_control();
	free(p);
	
	pthread_mutex_destroy(&lock);
	pthread_cond_destroy(&not_empty);
	pthread_cond_destroy(&not_full);
	return 0;
}

