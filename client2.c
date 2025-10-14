/* client2.c - fixed version for macOS / POSIX */
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

int create_tcp_socket();
char *get_ip(char *host);
char *build_get_query(char *host, char *page);
void usage();
void *client(void *arg);
int timeval_subtract(struct timeval *result, struct timeval *t2, struct timeval *t1);

#define HOST "coding.debuntu.org"
#define PAGE "/"
#define USERAGENT "HTMLGET 1.0"

#define MAX_THREAD 100

char *host;
char *page;
int port;

int main(int argc, char **argv)
{
	if(argc < 3){
		usage();
		exit(2);
	}

	struct timeval tvBegin, tvEnd, tvDiff;
	int nthread, failed = 0;
	int *tret = NULL;
				
	/* usage check (kept similar to original) */
	if(argc < 3) {
		printf("usage: ./client [server ip or dns] [port] <# thread>\n");
		return 0;
	}
	host = argv[1];
	port = atoi(argv[2]);
		
	if(argc < 4) nthread  = 10;
	else nthread = atoi(argv[3]);
	if(nthread > MAX_THREAD) nthread = MAX_THREAD;
	if(argc > 4){
		page = argv[4];
	}else{
		page = PAGE;
	}

	gettimeofday(&tvBegin, NULL);
	printf("Request: GET %s:%d/%s, # of client: %d\n", host, port, page, nthread);

	pthread_t *p = (pthread_t*)malloc(nthread * sizeof(pthread_t));
	int i;
	for(i = 0; i < nthread; i++)
	{
		pthread_create(&(p[i]), NULL, client, NULL);
	}

	for(i = 0; i < nthread; i++)
	{
		/* join returns pointer in tret */
		pthread_join(p[i], (void**)&tret);
		if(tret == NULL || (*tret) <= 0) failed++;
		/* free the heap-allocated return value from thread */
		if(tret) free(tret);
		tret = NULL;
	}
	free(p);

	gettimeofday(&tvEnd, NULL);
	timeval_subtract(&tvDiff, &tvEnd, &tvBegin);
	printf("Time to handle %d requests (%d failed): %ld.%06ld sec\n",
	       nthread, failed, (long)tvDiff.tv_sec, (long)tvDiff.tv_usec);
	return 0;
}

void *client(void *arg)
{
	struct sockaddr_in *remote;
	int sock;
	int tmpres;
	char *get;
	char buf[BUFSIZ+1];
	char *ip;
	struct timeval tvBegin, tvEnd, tvDiff;

	gettimeofday(&tvBegin, NULL);
	sock = create_tcp_socket();
	ip = get_ip(host);
	/* allocate the size of struct sockaddr_in, not size of pointer */
	remote = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	if (!remote) {
		perror("malloc");
		free(ip);
		close(sock);
		/* return via heap allocated int as usual */
		int *ret = malloc(sizeof(int));
		if (ret) *ret = -1;
		pthread_exit(ret);
	}
	memset(remote, 0, sizeof(struct sockaddr_in));

	remote->sin_family = AF_INET;
	tmpres = inet_pton(AF_INET, ip, (void *)(&(remote->sin_addr.s_addr)));
	if( tmpres < 0)  
	{
		perror("Can't set remote->sin_addr.s_addr");
		free(remote);
		free(ip);
		close(sock);
		int *ret = malloc(sizeof(int)); if(ret) *ret = -1;
		pthread_exit(ret);
	}else if(tmpres == 0)
	{
		fprintf(stderr, "%s is not a valid IP address\n", ip);
		free(remote);
		free(ip);
		close(sock);
		int *ret = malloc(sizeof(int)); if(ret) *ret = -1;
		pthread_exit(ret);
	}
	remote->sin_port = htons(port);

	if(connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr_in)) < 0){
		perror("Could not connect");
		free(remote);
		free(ip);
		close(sock);
		int *ret = malloc(sizeof(int)); if(ret) *ret = -1;
		pthread_exit(ret);
	}

	get = build_get_query(host, page);

	/* Send the query to the server */
	int sent = 0;
	size_t getlen = strlen(get);
	while(sent < (int)getlen)
	{ 
		tmpres = send(sock, get+sent, getlen-sent, 0);
		if(tmpres == -1){
			perror("Can't send query");
			free(get);
			free(remote);
			free(ip);
			close(sock);
			int *ret = malloc(sizeof(int)); if(ret) *ret = -1;
			pthread_exit(ret);
		}
		sent += tmpres;
	}
	/* now receive */
	memset(buf, 0, sizeof(buf));
	int rbyte = 0;
	while((tmpres = read(sock, buf, BUFSIZ)) > 0) {
		rbyte += tmpres;
	}
	if(tmpres < 0) {
		perror("Error receiving data");
	}

	gettimeofday(&tvEnd, NULL);
	timeval_subtract(&tvDiff, &tvEnd, &tvBegin);

	/* portable thread id print */
	unsigned long tid = (unsigned long)pthread_self();
	printf("[tid %lu] received %d bytes (%ld.%06ld sec).\n",
	       tid, rbyte, (long)tvDiff.tv_sec, (long)tvDiff.tv_usec);

	free(get);
	free(remote);
	free(ip);
	close(sock);

	/* return bytes via heap-allocated pointer so main can safely access it */
	int *ret = malloc(sizeof(int));
	if(ret) {
		*ret = rbyte;
	} else {
		/* allocation failed: still provide something */
		/* caller will treat <=0 as failure */
	}
	pthread_exit(ret);
	/* unreachable */
	return NULL;
}

void usage()
{
	fprintf(stderr, "USAGE: ./client <host> <port> <# thread> [page]\n\
\t\thost: IP or hostname. ex: 127.0.0.1\n\
\t\tpage: the page to retrieve. ex: index.html, default: /\n");
}

int create_tcp_socket()
{
	int sock;
	if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
		perror("Can't create TCP socket");
		exit(1);
	}
	return sock;
}

char *get_ip(char *host)
{
	struct hostent *hent;
	int iplen = 15; // XXX.XXX.XXX.XXX
	char *ip = (char *)malloc(iplen+1);
	if (!ip) {
		perror("malloc");
		exit(1);
	}
	memset(ip, 0, iplen+1);
	if((hent = gethostbyname(host)) == NULL)
	{
		herror("Can't get IP");
		free(ip);
		exit(1);
	}
	if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip, iplen) == NULL)
	{
		perror("Can't resolve host");
		free(ip);
		exit(1);
	}
	return ip;
}

char *build_get_query(char *host, char *page)
{
	char *getpage = page;
	char *tpl = "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\n\r\n";
	if(getpage[0] == '/'){
		getpage = getpage + 1;
	}
	/* compute needed length safely */
	int needed = snprintf(NULL, 0, tpl, getpage, host, USERAGENT) + 1;
	char *query = malloc(needed);
	if (!query) return NULL;
	snprintf(query, needed, tpl, getpage, host, USERAGENT);
	return query;
}

int timeval_subtract(struct timeval *result, struct timeval *t2, struct timeval *t1)
{
	long int diff = (t2->tv_usec + 1000000 * t2->tv_sec) - (t1->tv_usec + 1000000 * t1->tv_sec);
	result->tv_sec = diff / 1000000;
	result->tv_usec = diff % 1000000;
	return (diff<0);
}
