#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>

#include "packet.h"

#define BUFFER_SIZE 1000

int main(int argc, char const *argv[])
{
    // check argument
    if (argc != 2) { 
        printf("usage: server -<UDP listen port>\n");
        exit(0);
    }
    int port = atoi(argv[1]);

    int sock_descriptor;
    struct sockaddr_in server_addr;

    // Create socket
    sock_descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_descriptor == -1){
        perror("Failure in socket creation"); 
        exit(0); 
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    // server_addr.sin_addr.s_addr = htonl(INADDR_ANY);	
    // memset(server_addr.sin_zero, 0, sizeof(server_addr.sin_zero));

    // bind to socket
    bind(sock_descriptor, (struct sockaddr *) &server_addr, sizeof(server_addr));

    // Listen from the client
    char buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr; 
    socklen_t l;
    if (recvfrom(sock_descriptor, buffer, sizeof(buffer), 0, (struct sockaddr *) &client_addr, &l) == -1) {
        printf("Error in receiving\n");
        exit(1);
    }

    // send message back to client based on message recevied
    if (strcmp(buffer, "ftp") == 0) {
        if ((sendto(sock_descriptor, "yes", strlen("yes"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr))) == -1) {
            printf("Error in sending yes to client\n");
            exit(1);
        }
    } else {
        if ((sendto(sock_descriptor, "no", strlen("no"), 0, (struct sockaddr *) &client_addr, sizeof(client_addr))) == -1) {
            printf("Error in sending no to client\n");
            exit(1);
        }
    }

    // Transferfile
    FILE *pFile = NULL;
    Packet packet;
    char packet_str[PACKET_SRING_SIZE];

    packet.filename = (char *) malloc(BUFFER_SIZE);
    packet.total_frag = 100;
    int file_size = 0;
    
    for (unsigned int packet_no = 0; packet_no != packet.total_frag;) {
        if (recvfrom(sock_descriptor, packet_str, sizeof(packet_str), 0, (struct sockaddr *) &client_addr, &l) == -1) {
            printf("Error in receiving packet from client\n");
            exit(1);
        }
        string2packet(packet_str, &packet);

        packet_no = packet.frag_no;
        printf("-- Received frag_no: %d\n", packet_no);

        // Create file
        if (packet_no == 1) {
            char filename[BUFFER_SIZE];
            strcpy(filename, packet.filename);		
            pFile = fopen(filename, "w");
        }
    
        // Write to file
        if (fwrite(packet.filedata, sizeof(char), packet.size, pFile) != packet.size) {
            printf("Error in writing the file\n");
            exit(1);
        } 

        // Send ACK back to client
        char ack_mssg[] = "ACK";
        // for(long i=-999999;i!=99999999;) i++;
        if ((sendto(sock_descriptor, ack_mssg, sizeof(ack_mssg), 0, (struct sockaddr *) &client_addr, sizeof(client_addr))) == -1) {
            printf("Error in sending ACK to client\n");
            exit(1);
        }

        file_size += packet.size;
    }

    printf("File %s transfer completed \n", packet.filename);
    printf("Total frag received = %d, file size = %d \n", packet.total_frag, file_size);
    fclose(pFile);
    free(packet.filename);

    close(sock_descriptor);
    return 0;
}