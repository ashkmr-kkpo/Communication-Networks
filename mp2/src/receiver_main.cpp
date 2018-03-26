/* 
 * File:   receiver_main.c
 * Author: 
 *
 * Created on
 */

// WRITE BUFFER_SIZE SEGMENTS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <map>
#include <assert.h>

#define BLOCK_SIZE 1024
#define NUM_BLOCKS 1024
#define TCP_HEADER_SIZE 20
#define BUFFER_SIZE NUM_BLOCKS * BLOCK_SIZE

using namespace std;

struct sockaddr_in si_me, si_other;
socklen_t addr_len;
int s, slen;

void diep(char *s) {
    perror(s);
    exit(1);
}

typedef struct tcp_header {
    unsigned short source_port;
    unsigned short dest_port;
    unsigned int seq_num;
    unsigned int ack_num;
    unsigned char offset;
    unsigned char flags;
    unsigned short recv_wnd;
    unsigned int length;
} tcp_header;

typedef struct recv_packet {
    unsigned short source_port;
    unsigned short dest_port;
    unsigned int seq_num;
    unsigned int ack_num;
    unsigned char offset;
    unsigned char flags;
    unsigned short recv_wnd;
    unsigned int length;
    char recv_data[BLOCK_SIZE];  //BLOCK OR PAYLOAD SIZE
}   recv_packet;

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);

    /////////////////

    ssize_t recv_bytes;
    unsigned int seq_expect=0;
    unsigned int seq_receive=0;
    std::map <unsigned int, recv_packet *> buffer_recv_map;

    FILE *finish_file = fopen(destinationFile, "w+");  //filepath
    if (finish_file == NULL)
    {
        printf("Error opening file!\n");
        exit(1);
    }

    ////////////////



    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


    /* Now receive data and send acknowledgements */

    unsigned int buffer_length;
    char buf[TCP_HEADER_SIZE + BLOCK_SIZE];

    while(1){
        addr_len = sizeof(si_other);

        recv_bytes = recvfrom(s, buf, TCP_HEADER_SIZE + BLOCK_SIZE, 0, (struct sockaddr*)&si_other, &addr_len);

        if(*(buf + 13) == 'F'){ // FINAL PACKET
                break;
        }

        if(recv_bytes>0)
        {
            //get data from buf
            //All i need is seq number and data
            recv_packet *new_pack = new recv_packet;
           // new_pack->seq_num=  buf[4] ;    //LOOK FOR offsets
            memcpy(&(new_pack->seq_num), buf+4,4);
            memcpy(&buffer_length, buf+16, 4);
            new_pack->length = buffer_length;
            memcpy(new_pack->recv_data, buf+20 ,buffer_length);

            assert(buffer_length + sizeof(tcp_header) == recv_bytes);

            seq_receive= new_pack->seq_num;
            //put into map
            printf("expect, receive %d,%d \n", seq_expect,seq_receive);

            if(seq_expect == seq_receive)
            {

                int i=seq_expect+1 ;
                //write expected pack to file and check for more 
                fwrite(new_pack->recv_data,new_pack->length,1,finish_file);

                while (buffer_recv_map.find(i) != buffer_recv_map.end())
                {
                    //delete from buf and check if more
                    fwrite(buffer_recv_map[i]->recv_data,new_pack->length,1,finish_file);
                    buffer_recv_map.erase(i);
                    i= i+1;
                }
                seq_expect=i;
            }else if(seq_expect < seq_receive)
            {
                //add it to buffer dictionary
                if (buffer_recv_map.find(seq_receive) == buffer_recv_map.end())
                {
                    buffer_recv_map[seq_receive]= new_pack;
                }

            }
            //int sendto(...const struct sockaddr *to, socklen_t tolen);
            if (sendto(s, &seq_expect, sizeof(unsigned int), 0, (struct sockaddr*) &si_other, sizeof(si_other)) == -1)
                diep("sendto");
        }
    }

    fclose(finish_file);
    close(s);
    printf("%s received.\n", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}