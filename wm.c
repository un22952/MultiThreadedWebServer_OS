#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "webserver.h"
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>    // For O_CREAT, O_EXCL
#include <sys/stat.h> // For mode constants

#define MAX_REQUEST 100

int port, numThread;
pthread_mutex_t lock;

sem_t *sem_empty;
sem_t *sem_full;

int in = 0, out = 0;
int buf[MAX_REQUEST];

void *listener(void *arg)
{
    int r;
    struct sockaddr_in sin;
    int sock;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);
    r = bind(sock, (struct sockaddr *)&sin, sizeof(sin));
    if (r < 0) {
        perror("Error binding socket");
        exit(1);
    }

    r = listen(sock, 5);
    if (r < 0) {
        perror("Error listening socket");
        exit(1);
    }

    printf("HTTP server listening on port %d\n", port);

    while (1) {
        int s = accept(sock, NULL, NULL);
        if (s < 0) {
            perror("accept");
            continue;
        }
        printf("[listener] accepted fd=%d\n", s);

        sem_wait(sem_empty);
        pthread_mutex_lock(&lock);

        buf[in] = s;
        in = (in + 1) % MAX_REQUEST;
        printf("[listener] added fd=%d at index=%d\n", s, in);

        pthread_mutex_unlock(&lock);
        sem_post(sem_full);
    }

    close(sock);
    return NULL;
}

void *worker(void *arg)
{
    while (1) {
        sem_wait(sem_full);
        pthread_mutex_lock(&lock);

        int s = buf[out];
        out = (out + 1) % MAX_REQUEST;

        printf("[worker %lu] handling socket %d\n", pthread_self(), s);

        pthread_mutex_unlock(&lock);
        sem_post(sem_empty);

        process(s);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 3 || atoi(argv[1]) < 2000 || atoi(argv[1]) > 50000) {
        fprintf(stderr, "./webserver_multi PORT(2001 ~ 49999) #_of_threads\n");
        return 0;
    }

    port = atoi(argv[1]);
    numThread = atoi(argv[2]);
    if (numThread > 100) numThread = 100;

    pthread_mutex_init(&lock, NULL);

    // Unlink first to clean up any previous runs
    sem_unlink("/sem_empty");
    sem_unlink("/sem_full");

    // Create named semaphores
    sem_empty = sem_open("/sem_empty", O_CREAT, 0644, MAX_REQUEST);
    sem_full = sem_open("/sem_full", O_CREAT, 0644, 0);

    if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    pthread_t listener_thread;
    pthread_t workers[numThread];
    
    for (int i = 0; i < numThread; i++) {
        pthread_create(&workers[i], NULL, worker, NULL);
    }
    
    pthread_create(&listener_thread, NULL, listener, NULL);
    
    for (int i = 0; i < numThread; i++) {
        pthread_join(workers[i], NULL);
    }
    pthread_join(listener_thread, NULL);

    // Cleanup
    sem_close(sem_empty);
    sem_close(sem_full);
    sem_unlink("/sem_empty");
    sem_unlink("/sem_full");
    pthread_mutex_destroy(&lock);

    return 0;
}
