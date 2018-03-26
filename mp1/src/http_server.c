/*
** server.c -- a stream socket server demo
*/

// how does the client know how long the content is?
// (w/o Content-Length header)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

// #define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold

#define REQUEST_SIZE 4096
#define HEADER_SIZE 32
#define BUFFER_SIZE 4096

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void handle_connection(int sockfd);

int main(int argc, char *argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	if(argc != 2){
		fprintf(stderr, "Usage: ./server <port>\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener

			/*if (send(new_fd, "Hello, world!", 13, 0) == -1)
				perror("send");*/

			handle_connection(new_fd);

			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

void handle_connection(int sockfd){
	// receive and parse request

	char request[REQUEST_SIZE];
	char method[REQUEST_SIZE];
	char location[REQUEST_SIZE];
	char version[REQUEST_SIZE];
	char header[HEADER_SIZE];
	char file_buf[BUFFER_SIZE];

	int rv = recv(sockfd, request, REQUEST_SIZE, 0);
	if(rv == -1){
		perror("recv");
		exit(1);
	}else if(rv == 0){
		fprintf(stderr, "socket closed");
		exit(1);
	}


	int matched = sscanf(request, "%s %s %s\n", method, location, version);

	if(matched != 3 || strcmp(method, "GET") || (strcmp(version, "HTTP/1.1") && strcmp(version, "HTTP/1.0")) || strncmp(location, "/", 1)){
		
		// 400 MALFORMED REQUEST

		// send header

		strcpy(header, "400 Bad Request\n\r\n");

		if((rv = send(sockfd, header, strlen(header), 0)) == -1){
			perror("send");
			exit(1);
		}

		return;
	}

	char *filepath = location + 1;

	if(!strcmp(filepath, "")){
		strcpy(filepath, "index.html");
	}

	// open file

	FILE *fin = fopen(filepath, "rb");
	if(fin == NULL){

		// 404 FILE NOT FOUND

		// send header + file

		strcpy(header, "HTTP/1.1 404 Not Found\n\r\n");

		if((rv = send(sockfd, header, strlen(header), 0)) == -1){
			perror("send");
			exit(1);
		}

		char *file_404 = "404.html";

		fin = fopen(file_404, "rb");

		while(1){
			size_t num_read = fread(file_buf, 1, BUFFER_SIZE, fin);
			if(num_read == 0) break;

			void *p = file_buf;
			while(num_read > 0){
				ssize_t num_sent = send(sockfd, p, num_read, 0);
				if(num_sent == -1){
					perror("send");
					exit(1);
				}

				num_read -= num_sent;
				p += num_sent;
			}
		}

		return;
	}

	// send header + file

	strcpy(header, "HTTP/1.1 200 OK\n\r\n");

	if((rv = send(sockfd, header, strlen(header), 0)) == -1){
		perror("send");
		exit(1);
	}

	while(1){
		size_t num_read = fread(file_buf, 1, BUFFER_SIZE, fin);
		if(num_read == 0) break;

		void *p = file_buf;
		while(num_read > 0){
			ssize_t num_sent = send(sockfd, p, num_read, 0);
			if(num_sent == -1){
				perror("send");
				exit(1);
			}

			num_read -= num_sent;
			p += num_sent;
		}
	}
}