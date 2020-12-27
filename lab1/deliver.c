#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h> 
#include <time.h>
#include <ctype.h>

#include "packet.h"

#define BUFFER_SIZE 50

double establish_communication(int sock_descriptor, struct sockaddr_in server_addr){
    // Record RTT
    clock_t start, end; 
    start = clock();

    // send ftp to server
    char send_buffer[] = "ftp";
    sendto(sock_descriptor, (const char *)send_buffer, sizeof(send_buffer), 0, (const struct sockaddr *) &server_addr, sizeof(server_addr)); 

    // Receiving yes from server
    socklen_t len = sizeof(server_addr);
    char buffer[BUFFER_SIZE];
    memset(&buffer, 0, sizeof(buffer));
    if (recvfrom(sock_descriptor, (char *)buffer, sizeof(buffer), 0, (struct sockaddr *)&server_addr, &len) == -1) {
        perror("Failure in receiving server's info"); 
        exit(0); 
    }
    printf("Received Message:%s\n", buffer);

    // Calculate RTT
    end = clock();
    double RTT = ((double) (end - start) / CLOCKS_PER_SEC);
    printf("RTT = %f usec.\n", RTT * 1000000);  

    if (strcmp(buffer, "yes") == 0){
        printf("A file transfer can start.\n");
    }
    else{
        printf("Did not receive yes from server.\n");
        exit(0);
    }

    return RTT;
}

void send_file(char * filename, struct sockaddr_in server_addr, int sock_descriptor, double RTT){
    char c[PACKET_DATA_SIZE];
    // Open file
    FILE *fptr;
    if ((fptr = fopen(filename, "r")) == NULL) {
        printf("Error in opening file");
        exit(0);
    }

    // get number of packets needed
    fseek(fptr, 0, SEEK_END);
    int file_length = ftell(fptr);
    int total_frag = file_length / PACKET_DATA_SIZE + (file_length > (file_length / PACKET_DATA_SIZE * PACKET_DATA_SIZE)) ;
    printf("Total size of file: %d, Total_frag: %d\n",file_length, total_frag);

    // go to beginning of the file
    rewind(fptr);

    //Read file into packets
    char **packet_array = malloc(sizeof(char*) * total_frag);
    for (int frag_no = 1; frag_no <= total_frag; frag_no++){
        Packet packet;

        packet.total_frag = total_frag;
        packet.frag_no = frag_no;
        memset(packet.filedata, 0, sizeof(char) * PACKET_DATA_SIZE);
        int read = fread((void*)packet.filedata, sizeof(char), PACKET_DATA_SIZE, fptr);
        packet.size = read;
        packet.filename = filename;

        // convert structure to packet string
        char *packet_string = malloc(sizeof(char) * PACKET_SRING_SIZE);
        packet2string(&packet, packet_string);
        packet_array[frag_no - 1] = packet_string;
    }

    // send file
    // Set timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = (int)(RTT * 1000000);

    if(setsockopt(sock_descriptor, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        printf("setsockopt failed\n");
        exit(1);
    }
    for (int frag_no = 1; frag_no <= total_frag; ++frag_no) {
        for (int sent_count = 0; ; sent_count ++){
            // Resend if timeout or not ACK
            if (sent_count > 3){
                printf("Too many resends, quit\n");
                exit(1);
            }
            if (sent_count == 0) printf("-- Sending frag_no: %d\n", frag_no);
            else printf("-- Re-sending frag_no: %d\n", frag_no);

            clock_t start, end; 
            start = clock();

            if (sendto(sock_descriptor, packet_array[frag_no - 1], PACKET_SRING_SIZE, 0, (const struct sockaddr *) &server_addr, sizeof(server_addr)) == -1){
                printf("Failure sending packet #%d\n", frag_no);
                exit(1);
            }

            //receive ACK from server
            socklen_t len = sizeof(server_addr);
            char received[BUFFER_SIZE];
            memset(&received, 0, sizeof(received));

            // clock_t here = clock();
            if (recvfrom(sock_descriptor, (char *)received, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &len) == -1) {
                // end = clock();
                // printf("Delay = %f\n", ((double) (end - here) / CLOCKS_PER_SEC * 1000000));
                printf("Timeout in receiving ACK from packet %d\n", frag_no);
                continue;
            }
            else {
                end = clock();
                // printf("Delay = %f\n", ((double) (end - here) / CLOCKS_PER_SEC * 1000000));
                double newRTT = ((double) (end - start) / CLOCKS_PER_SEC);
                printf("-- Received: %s, new RTT = %f usec.\n", received, newRTT * 1000000);
                if (strcmp(received, "ACK") != 0){
                    printf("Did not receive ACK \n");
                    continue;
                }
                break;
            }

        }
    }
}

int main(int argc, char const *argv[]){
    // check argument
    if (argc != 3) { 
        printf("usage: deliver <server address> <server port number>\n");
        exit(0);
    }
    int port = atoi(argv[2]);

    int sock_descriptor;
    struct sockaddr_in server_addr;

    // Create socket descriptor
    sock_descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_descriptor == -1){
        perror("Failure in socket creation"); 
        exit(0); 
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    // server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    struct hostent *server = gethostbyname(argv[1]);
    bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);
    
    // ask for user input
    char filename[BUFFER_SIZE];
    printf("Enter input: ftp <file name>:\n");
    scanf("%s", filename);

    // Check if file exists
    if(access(filename, F_OK) == -1) {
        // file doesn't exist
        printf("File <%s> does not exist, quit program\n", filename);
        exit(0);
    }

    // Establish communication
    double RTT = establish_communication(sock_descriptor, server_addr);


    // Send file to server
    send_file(filename, server_addr, sock_descriptor, RTT);

    close(sock_descriptor);
    return 0;
}
