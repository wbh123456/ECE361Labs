#include <stdio.h>
#include <string.h> 
#include <stdlib.h>

#define PACKET_DATA_SIZE 1000
#define PACKET_SRING_SIZE 1500

typedef struct Packet {
	unsigned int total_frag;
	unsigned int frag_no;
	unsigned int size;
	char *filename;
	char filedata[1000]; 
} Packet;

void packet2string(Packet *packet, char *packet_string) {
    memset(packet_string, 0, sizeof(char) * PACKET_SRING_SIZE);

    sprintf(packet_string, "%d", packet->total_frag);
    sprintf(packet_string + strlen(packet_string), "%s", ":");
    
    sprintf(packet_string + strlen(packet_string), "%d", packet->frag_no);
    sprintf(packet_string + strlen(packet_string), "%s", ":");

    sprintf(packet_string + strlen(packet_string), "%d", packet->size);
    sprintf(packet_string + strlen(packet_string), "%s", ":");

    sprintf(packet_string + strlen(packet_string), "%s", packet->filename);
    sprintf(packet_string + strlen(packet_string), "%s", ":");

    memcpy(packet_string + strlen(packet_string), packet->filedata, sizeof(char) * PACKET_DATA_SIZE);
}

void string2packet(char* packet_string, Packet *packet) {
    int accumulated_size = 0;
    char* buffer = strtok(packet_string, ":");
    accumulated_size += strlen(buffer);
    packet -> total_frag = atoi(buffer);

    buffer = strtok(NULL, ":");
    accumulated_size += strlen(buffer);
    packet -> frag_no = atoi(buffer);

    buffer = strtok(NULL, ":");
    accumulated_size += strlen(buffer);
    packet -> size = atoi(buffer);

    buffer = strtok(NULL, ":");
    accumulated_size += strlen(buffer);
    strcpy(packet -> filename, buffer);

    memcpy(packet -> filedata, packet_string + accumulated_size + 4, packet -> size);
}