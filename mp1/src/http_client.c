#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define MAXDATASIZE 100 // max number of bytes we can get at once 

#define BUFFER_SIZE 1024

typedef struct Url {
    char *scheme;
    char *hostname;
    char *port;
    char *path;

} Url;

int main(int argc, char *argv[])
{
	// char s[INET6_ADDRSTRLEN];

	// char buf[MAXDATASIZE];
	char temp[MAXDATASIZE];
	int sockfd;
	char *url_parse;
	Url request;
	int i; 
	int scheme=0; int host=0; int portget=0;// int hostind;
    if (argc != 2) {
        fprintf(stderr, "Usage: http <url>\n");
        return 1;
    }
    url_parse= (char *)malloc(sizeof(argv[1]));
    request.path = (char *)malloc(100);
    request.path = (char *)malloc(5);
    strcpy(url_parse,argv[1]);
    int j=0;
    //fprintf(stderr, "%s\n",url_parse );
    for(i=0;i<strlen(url_parse);i++,j++)
    {
    	//fprintf(stderr, "%c\n",url_parse[i] );
    	if(url_parse[i]==':' && !scheme)
    	{	
    		scheme=1;
    		 //start of hostname
    		request.scheme = (char *)malloc(i);  //get the scheme
    		strncpy(request.scheme,temp,i)	;
    		i=i+3;
    		j=0;
    	}

    	if((url_parse[i]=='/' || url_parse[i]==':') && scheme && !host)
    	{
    		host=1;
    	
    		request.hostname = (char *)malloc(j);
    		strncpy(request.hostname,temp,j);
    		if(url_parse[i]=='/')
    		{	
    			request.port = (char *)malloc(2);
    			request.port="80";
    			portget=1;
    			i=i-1;
    		}
    		j=0;
    		i=i+1;
    	}
    	if(!portget && host && url_parse[i]=='/')
    	{
    		request.port = (char *)malloc(j);
    		strncpy(request.port,temp,j);
    		portget=1;
    		j=0;
    	}
    	
    	if(portget)
    	{
 
    		request.path[j]=url_parse[i];
    	}
    	temp[j]=url_parse[i];

    }
    request.path[j]='\0';

    //strncpy(request.path,temp,j);  // GET PATH

    printf("%s\n",request.hostname );
    printf("%s\n",(request.port) );
    printf("%s\n",(request.path) );


  	struct addrinfo hints, *servinfo,*p;
    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	int rv;
	if ((rv = getaddrinfo(request.hostname, request.port, &hints, &servinfo)) != 0) 
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	//MAKE CONNECTION
		// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) 
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}
	if (p == NULL) 
	{
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	//BUILD THE REQUEST
	char http_req[1024];
	strcat(http_req, "GET ");
	strcat(http_req, request.path);
	strcat(http_req, " HTTP/1.1\r\n");
	strcat(http_req, "Host: ");
	strcat(http_req, request.hostname);
	strcat(http_req, ":");
	strcat(http_req, request.port);
	strcat(http_req, "\r\n\r\n");
	// strcat(http_req, "\r\nConnection: Keep-Alive\r\n");
	size_t bytes_send=0;
	size_t total_bytes_send=0;
	// size_t bytes_to_send= strlen(http_req);
	printf("%s",http_req);

	while(1)
	{
		if((bytes_send = send(sockfd, http_req, strlen(http_req),0)) == -1)
		{
			perror("send");
			exit(1);
		}

		total_bytes_send+= bytes_send;

		if (total_bytes_send >= bytes_send)
			break;

	}

	char receive[BUFFER_SIZE];
	int num_bytes;

	FILE *f = fopen("output", "w");
	if (f == NULL)
	{
	    printf("Error opening file!\n");
	    exit(1);
	}

	// receive HTTP header

	int header_received = 0;

	while(1)
	{
		if ((num_bytes = recv(sockfd, receive, BUFFER_SIZE, 0)) == -1) {
		    perror("recv");
		    exit(1);
		}

		//	printf("%d\n", num_bytes);
		if(num_bytes == -1 || num_bytes == 0)
			break;

		fprintf(stderr, "%s\n", receive);

		if(!header_received){
			char *buf = receive;
			char *header = strsep(&buf, "\n");
			strsep(&buf, "\n");
			header_received = 1;
			fwrite(buf, num_bytes - strlen(header) - 3, 1, f);
		}else{
			fwrite(receive, num_bytes, 1, f);
		}

	}
	// printf("%s\n", totalreceive);

	// fprintf(f, "%s", totalreceive);
	fclose(f);

}