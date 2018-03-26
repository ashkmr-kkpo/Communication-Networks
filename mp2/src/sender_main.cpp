/* 
 * File:   sender_main.c
 * Author: nsm2
 *
 * Created on 
 */

// IMPLEMENT TEARDOWN (FIN RESPONSE)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>
#include <map>

using namespace std;

#define BLOCK_SIZE 1024
#define INIT_SSTHRESH 64
#define RTT 20

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef enum {SLOW_START, CON_AVOIDANCE, FAST_RECOVERY} tcp_congestion_state;
typedef enum {NEW_ACK, DUP_ACK, OLD_ACK, TIMEOUT} tcp_event;
typedef enum {NEW_TRANSMIT, RE_TRANSMIT} trans_state;

typedef struct tcp_header {
    unsigned short source_port;
    unsigned short dest_port;
    unsigned int seq_num;       // used - send
    unsigned int ack_num;       
    unsigned char offset;
    unsigned char flags;        // flags = 'F'
    unsigned short recv_wnd;
    unsigned int length;        // used - send
} tcp_header;

typedef struct send_packet {
    tcp_header header;
    char buf[BLOCK_SIZE];
} send_packet;

void diep(char *s);
void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);
void transfer_file(FILE *fp, unsigned long long int bytesToTransfer);

struct sockaddr_in si_other, si_self;
int s, slen;

int main(int argc, char** argv){
    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

    return (EXIT_SUCCESS);
}

void diep(char *s){
    perror(s);
    exit(1);
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer){
    //Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if(fp == NULL){
        printf("Could not open file to send.");
        exit(1);
    }

    /* Determine how many bytes to transfer */

    // assert bytesToTransfer is smaller than file size

    slen = sizeof (si_other);

    if((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    int enable = 1;
    if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        diep("setsockopt");

    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    // bind to local address (port = 4546)

    /* unsigned short int localUDPport = 4546;

    memset((char *) &si_self, 0, sizeof(si_self));
    si_self.sin_family = AF_INET;
    si_self.sin_port = htons(4546);
    si_self.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(s, (struct sockaddr *) &si_self, sizeof(si_self)) == -1)
        diep("bind");*/

    /* Send data and receive acknowledgements on s*/

    transfer_file(fp, bytesToTransfer);

    printf("Closing the socket\n");
    close(s);
    fclose(fp);

    return;
}

void transfer_file(FILE *fp, unsigned long long int bytesToTransfer){
    map <unsigned int, send_packet *> send_map;     // seq_number -> send_packet

    tcp_congestion_state con_state = SLOW_START;
    tcp_event event = NEW_ACK;
    trans_state t_state = NEW_TRANSMIT;

    // don't worry about sequence number overflow (4 GB * BLOCK_SIZE) = 4 TB

    unsigned int cwnd = 1; // start at 1
    unsigned int ssthresh = 64;
    unsigned int dup_ack_count = 0;
    unsigned int first_unacked_block = 0;
    unsigned int next_send_block = 0;

    unsigned long long int blocksToTransfer = (bytesToTransfer + BLOCK_SIZE - 1) / BLOCK_SIZE; // ceiling division (bytesToTransfer / BLOCK_SIZE)

    while(1){   // break when FIN_ACK recv'd

        /*************************/
        // TRANSMIT
        /*************************/

        if(t_state == NEW_TRANSMIT){
            while(next_send_block < first_unacked_block + cwnd && next_send_block < blocksToTransfer){ // still space in cwnd
                send_packet *pkt = new send_packet;

                pkt->header.seq_num = next_send_block;
                pkt->header.length = MIN(BLOCK_SIZE, bytesToTransfer);

                size_t n_read;
                if((n_read = fread(pkt->buf, 1, pkt->header.length, fp)) == 0)
                    diep("fread");

                send_map[next_send_block] = pkt;

                ssize_t num_sent;
                if((num_sent = sendto(s, pkt, sizeof(tcp_header) + pkt->header.length, 0, (struct sockaddr *) &si_other, sizeof(si_other))) == -1)
                    diep("sendto");

                bytesToTransfer -= pkt->header.length;
                next_send_block++;
            }
        }else if(t_state == RE_TRANSMIT){
            send_packet *pkt = send_map[first_unacked_block];

            ssize_t num_sent;
            if((num_sent = sendto(s, pkt, sizeof(tcp_header) + pkt->header.length, 0, (struct sockaddr *) &si_other, sizeof(si_other))) == -1)
                diep("sendto");
        }

        /*************************/
        // WAIT_ACK
        // restarts timer after any event
        /*************************/

        struct timeval tv; // overwritten on linux
        fd_set read_fds;

        tv.tv_sec = 0;
        tv.tv_usec = RTT * 1000 * 3 / 2; // timeout = 1.5 * RTT

        FD_ZERO(&read_fds);
        FD_SET(s, &read_fds);

        select(s + 1, &read_fds, NULL, NULL, &tv);

        if(FD_ISSET(s, &read_fds)){
            ssize_t num_recv;
            unsigned int ack_num;   // Need to receive larger structure for FIN
            struct sockaddr_in their_addr;
            socklen_t addr_len;

            addr_len = sizeof(si_other);

            if((num_recv = recvfrom(s, &ack_num, sizeof(unsigned int), 0, (struct sockaddr *) &their_addr, &addr_len)) == -1)
                diep("recvfrom");
            printf("ACK: %d \n",ack_num);
            assert(num_recv == sizeof(unsigned int));
            
            if(ack_num == blocksToTransfer){ // FIN_ACK
                break;
            }else if(ack_num > first_unacked_block){ // new
                event = NEW_ACK;

                for(unsigned int i = first_unacked_block; i < ack_num; i++)
                    send_map.erase(i);

                first_unacked_block = ack_num;
            }else if(ack_num == first_unacked_block){ // dup
                event = DUP_ACK;
            }else{ // old
                event = OLD_ACK;
            }
        }else{
            event = TIMEOUT;
        }

        /*************************/
        // PROCESS
        // sets cwnd and t_state
        /*************************/

        // complete rest of FSM

        /*if(con_state == SLOW_START){
            if(event == NEW_ACK){
                cwnd = cwnd + 1;
                t_state = NEW_TRANSMIT;
            }else if(event == TIMEOUT){
                cwnd = 1;
                t_state = RE_TRANSMIT;
            }
        }*/

        if(con_state == SLOW_START){
            if(event == NEW_ACK){
                cwnd = cwnd + 1;
                dup_ack_count = 0;

                if(cwnd >= ssthresh){
                    con_state = CON_AVOIDANCE;
                }
                
                t_state = NEW_TRANSMIT;
            }else if(event == DUP_ACK){
                dup_ack_count++;

                if(dup_ack_count == 3){
                    con_state = FAST_RECOVERY;

                    ssthresh = cwnd / 2;
                    cwnd = ssthresh + 3;

                    dup_ack_count = 0;

                    t_state = RE_TRANSMIT;
                }else{
                    t_state = NEW_TRANSMIT;
                }
            }else if(event == TIMEOUT){
                ssthresh = cwnd / 2;
                cwnd = 1;
                dup_ack_count = 0;

                t_state = RE_TRANSMIT;
            }
        }else if(con_state == CON_AVOIDANCE){
            if(event == NEW_ACK){
                if(rand() % cwnd == 0) cwnd = cwnd + 1; // cwnd + (1 / cwnd)
                dup_ack_count = 0;

                t_state = NEW_TRANSMIT;
            }else if(event == DUP_ACK){
                dup_ack_count++;

                if(dup_ack_count == 3){
                    con_state = FAST_RECOVERY;

                    ssthresh = cwnd / 2;
                    cwnd = ssthresh + 3;

                    dup_ack_count = 0;

                    t_state = RE_TRANSMIT;
                }else{
                    t_state = NEW_TRANSMIT;
                }
            }else if(event == TIMEOUT){
                con_state = SLOW_START;

                ssthresh = cwnd / 2;
                cwnd = 1;
                dup_ack_count = 0;

                t_state = RE_TRANSMIT;
            }
        }else if(con_state == FAST_RECOVERY){
                if(event == NEW_ACK){
                    con_state = CON_AVOIDANCE;

                    cwnd = ssthresh + 1; // so cwnd > 0
                    dup_ack_count = 0;

                    t_state = NEW_TRANSMIT;
            }else if(event == DUP_ACK){
                cwnd = cwnd + 1;

                t_state = NEW_TRANSMIT;
            }else if(event == TIMEOUT){
                con_state = SLOW_START;

                ssthresh = cwnd / 2;
                cwnd = 1;
                dup_ack_count = 0;

                t_state = RE_TRANSMIT;      
            }
        }
        
    }

    // send FIN_ACK until timeout
    tcp_header fin_pkt;

    fin_pkt.flags = 'F';
    fin_pkt.length = 0;

    for(int i = 0; i < 3; i++){
        ssize_t num_sent;
        if((num_sent = sendto(s, &fin_pkt, sizeof(tcp_header), 0, (struct sockaddr *) &si_other, sizeof(si_other))) == -1)
            diep("sendto");
    }
}