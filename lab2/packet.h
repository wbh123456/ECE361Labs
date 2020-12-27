#include <stdio.h>
#include <string.h> 
#include <stdlib.h>

#define PACKET_SRING_SIZE 1500
#define MAX_NAME 50
#define MAX_DATA 1000

enum packet_type{
    LOGIN, //0
    LO_ACK, //1
    LO_NAK, //2
    EXIT, //3
    JOIN, //4
    JN_ACK, //5
    JN_NAK, //6
    LEAVE_SESS, //7
    NEW_SESS, //8
    NS_ACK, //9
    MESSAGE, //10
    QUERY, //11
    QU_ACK, //12
    // Additional Feature: Inivite
    INVITE,         // Invite a user to a session. packet.data = <user id>,<session id>
    INVITE_FAILED,  // Failed to invite. packet.data = reason
    ACP,            // Accept invitation, packet.data = <inviter's id>,<session id>
    DEC             // Decline invitation, packet.data = <inviter's id>,<session id>
};

typedef struct Packet {
	unsigned int type;
	unsigned int size; //size of data
	unsigned char source[MAX_NAME]; //ID of client
	unsigned char data[MAX_DATA]; 
} Packet;


Packet initialize_packet(unsigned int type, unsigned int size, unsigned char* source, unsigned char* data){
    Packet packet;
    packet.type = type;
    packet.size = size;
    strcpy(packet.source, source);
    memcpy(packet.data, data, size);
    return packet;
}

void packet2string(Packet *packet, char *packet_string) {
    memset(packet_string, 0, sizeof(char) * PACKET_SRING_SIZE);

    sprintf(packet_string, "%d", packet->type);
    sprintf(packet_string + strlen(packet_string), "%s", ":");
    
    sprintf(packet_string + strlen(packet_string), "%d", packet->size);
    sprintf(packet_string + strlen(packet_string), "%s", ":");

    sprintf(packet_string + strlen(packet_string), "%s", packet->source);
    sprintf(packet_string + strlen(packet_string), "%s", ":");

    memcpy(packet_string + strlen(packet_string), packet->data,  packet->size);
}

void string2packet(char* packet_string, Packet *packet) {
    int accumulated_size = 0;
    char* buffer = strtok(packet_string, ":");
    accumulated_size += strlen(buffer);
    packet -> type = atoi(buffer);

    buffer = strtok(NULL, ":");
    accumulated_size += strlen(buffer);
    packet -> size = atoi(buffer);

    buffer = strtok(NULL, ":");
    if(!buffer) buffer = "";
    accumulated_size += strlen(buffer);
    strcpy(packet -> source, buffer);

    if(packet -> size) memcpy(packet -> data, packet_string + accumulated_size + 3, packet -> size);
    else strcpy(packet -> data, "");
}