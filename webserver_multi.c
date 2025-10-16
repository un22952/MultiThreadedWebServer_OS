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
#include <errno.h>


#define MAX_REQUEST 100

int port, numThread;
pthread_mutex_t lock;

sem_t *sem_empty;
sem_t *sem_full;

int in = 0, out = 0, ind = 0;
int buf[MAX_REQUEST];
int failed[MAX_REQUEST];


// producer
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
    // keeps accepting connections and adding to the buffer
    while (1) {
        int s = accept(sock, NULL, NULL);
        if (s < 0) {
            perror("accept");
            continue;
        }
        // tell consumers to wait if full
        sem_wait(sem_empty);
        // lock the buffer
        pthread_mutex_lock(&lock);
        // add to buffer
        buf[in] = s;
        in = (in + 1) % MAX_REQUEST;
        // unlock the buffer
        pthread_mutex_unlock(&lock);
        // tell consumers items are available
        sem_post(sem_full);
    }

    close(sock);
    return NULL;
}

// consumer
void *worker(void *arg)
{
        // keep processing requests
    while (1) {
        //tell producer to wait if empty
        sem_wait(sem_full);
        // lock the buffer
        pthread_mutex_lock(&lock);
        // remove from buffer
        int s = buf[out];
        out = (out + 1) % MAX_REQUEST;
        // unlock the buffer
        pthread_mutex_unlock(&lock);
        //tell producer an empty slot is available
        sem_post(sem_empty);
        // process the request
        process(s);
        
    }
    return NULL;
}

// monitor and recreate dead threads
void threadControl(pthread_t *workers) {
    // keep monitoring
    while (1) {
        
        for (int i = 0; i < numThread; ++i) {
            // identify dead threads
            int rc = pthread_kill(workers[i], 0);
            if (rc == ESRCH) {     
                // recreate dead threads    
                printf("Worker[%d] is dead. Recreating...\n", i);       
                int r = pthread_create(&workers[i], NULL, worker, NULL);
                if (r == 0) {
                    printf("Recreated worker[%d]\n", i);
                    
                }
                else
                    fprintf(stderr, "Recreate failed: %d\n", r);
            }
        }
        sleep(1);  // avoid spin
        
    }
}

// main
int main(int argc, char *argv[])
{
    if (argc != 3 || atoi(argv[1]) < 2000 || atoi(argv[1]) > 50000) {
        fprintf(stderr, "./webserver_multi PORT(2001 ~ 49999) #_of_threads\n");
        return 0;
    }
  
    port = atoi(argv[1]);
    numThread = atoi(argv[2]);
    if (numThread > 100) numThread = 100;
    // initialize mutex and semaphores
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
    // create worker threads
    for (int i = 0; i < numThread; i++) {
        int r = pthread_create(&workers[i], NULL, worker, NULL);
        if (r == 0)
            printf("Created worker[%d]\n", i);
        else
            fprintf(stderr, "worker thread failed: %d\n", r);
    }
    // create listner thread
    int l = pthread_create(&listener_thread, NULL, listener, NULL);
    if (l == 0)
        printf("Created listener thread\n");
    else
        fprintf(stderr, "listener thread failed: %d\n", l);

    // monitor and recreate dead threads
    threadControl(workers);
    // join threads
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
