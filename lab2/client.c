#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netdb.h> 

#include <pthread.h>

#include "packet.h"

// The socket the client is connecting to
int sock_descriptor;
char current_client[50];
char current_session[50];
int in_session;

char pending_invitation[50]; // Invitation info. Format: <inviter's id,session id>

// Control Routines
int login(char* client_ID, char* pwd, char* server_IP, char* server_port);
int logout();
int create_session(char* session_id);
int join_session(char* session_id);
int leave_session();
int list();
int send_text(char* text);
int invite(char* user_id, char* session_id);
int decline_invite();
int accept_invite();
void *thread_listen_from_server(); 

// Helper functions
int if_logged_in();

int main(){
    char input_buffer[100];
    sock_descriptor = -1;
    in_session = 0;
    memset(current_client, 0, sizeof(current_client));
    memset(current_session, 0, sizeof(current_session));
    memset(pending_invitation, 0, sizeof(pending_invitation));
    while(1){
        // Clear buffer
        memset(&input_buffer,0,sizeof(input_buffer));

        // printf("What do you want to do?\n>>");
        scanf("%[^\n]%*c", input_buffer);

        if(input_buffer[0] != '/'){
            // send text to conference
            if(in_session) send_text(input_buffer);
            continue;
        }

        // tokenize the input
        char str[100];
        strcpy(str, input_buffer);
        char* tokens[5];
        char* token = strtok(str, " ");
        tokens[0] = token;
        for(int i = 1;token != NULL;i++){
            token = strtok(NULL, " ");
            tokens[i] = token;
        }

        // Recognize Command
        if(strcmp(tokens[0], "/login") == 0){
            char* client_ID = tokens[1];
            char* pwd = tokens[2];
            char* server_IP = tokens[3];
            char* server_port = tokens[4];
            login(client_ID, pwd, server_IP, server_port);
        }
        else if(strcmp(tokens[0], "/logout") == 0){
            if (!if_logged_in()){
                printf("Error: you haven't logged in yet!\n");
                continue;
            }
            else{
                logout();
                return 0;
            }
        }
        else if(strcmp(tokens[0], "/list") == 0){
            list();
        }
        else if(strcmp(tokens[0], "/createsession") == 0){
            char* session_id = tokens[1];
            create_session(session_id);
        }
        else if(strcmp(tokens[0], "/joinsession") == 0){
            char* session_id = tokens[1];
            join_session(session_id);
        }
        else if(strcmp(tokens[0], "/leavesession") == 0){
            leave_session();
        }
        else if(strcmp(tokens[0], "/invite") == 0){
            char* user_id = tokens[1];
            char* session_id = tokens[2];
            invite(user_id, session_id);
        }
        else if(strcmp(tokens[0], "/Y") == 0){
            accept_invite();
        }
        else if(strcmp(tokens[0], "/N") == 0){
            decline_invite();
        }
        else{
            printf("-- Unrecognized command: %s\n", input_buffer);
        }
    }

}



int login(char* client_ID, char* pwd, char* server_IP, char* server_port){
    if (sock_descriptor != -1){
        printf("You are already logged in!\n");
        return -1;
    }
    if (!client_ID||!pwd||!server_IP||!server_port){
        printf("Error: login information not valid!\n");
        return -1;
    }
    //*****************Establish TCP Connection***************
    // Create socket descriptor
    int port = atoi(server_port);
    struct sockaddr_in server_addr;
    sock_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_descriptor == -1){
        printf("Error in socket creation\n"); 
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_IP);
    server_addr.sin_port = htons(port);

    // Connect to server
    if (connect(sock_descriptor, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) { 
        printf("Error in connection\n"); 
        sock_descriptor = -1;
        return -1;
    } 
    // printf("connected to the server@%s, Port# %d\n\n", server_IP, port); 
    //********************************************************

    //****************Log In******************
    unsigned char data[PACKET_SRING_SIZE];
    strcpy(data, client_ID);
    strcat(data, ",");
    strcat(data, pwd);

    Packet login_packet = initialize_packet(LOGIN, strlen(data)+1, client_ID, data);

    char packet_str[PACKET_SRING_SIZE];
    packet2string(&login_packet, packet_str);

    // send packet
    send(sock_descriptor, packet_str, strlen(packet_str)+1 , 0);

    // check whether login successful
    char return_msg[PACKET_SRING_SIZE];
    read(sock_descriptor, return_msg, PACKET_SRING_SIZE);
    Packet reply_packet;
    string2packet(return_msg, &reply_packet);
    if(reply_packet.type == LO_ACK){
        printf("LOGIN successful\n");
        strcpy(current_client, client_ID);
    }
    else if(reply_packet.type == LO_NAK){
        printf("LOGIN unseccessful: %s\n", reply_packet.data);
        sock_descriptor = -1;
        return -1;
    }
    else{
        printf("Error: Unrecognized ACK from server: %s\n", return_msg);
        sock_descriptor = -1;
        return -1;
    }
    //****************************************

    // Thread for listening to server
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, thread_listen_from_server, NULL);

    return 1;
}

int logout(){
    // Create packet
    Packet logout_packet = initialize_packet(EXIT, 0, current_client, "");
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&logout_packet, packet_str);

    // send packet
    send(sock_descriptor, packet_str, strlen(packet_str)+1 , 0);

    // close connection
    if (close(sock_descriptor)!=0){
        printf("Error in closing socket\n");
        return 0;
    }
    printf("Logged out from server!\n");
    return 1;
}

int list(){
    if (!if_logged_in()){
        printf("Error: You haven't logged in yet!\n");
        return -1;
    }
    // Create packet
    Packet list_packet = initialize_packet(QUERY, 0, current_client, "");
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&list_packet, packet_str);

    // send packet
    send(sock_descriptor, packet_str, strlen(packet_str)+1 , 0);
}


int create_session(char* session_id){
    if (!if_logged_in()){
        printf("Error: You haven't logged in yet!\n");
        return -1;
    }
    if (!session_id){
        printf("Usage: /createsession <session_id>\n");
        return -1;
    }
    if(in_session){
        printf("Error: you are in a session!\n");
        return -1;
    }
    // Create packet
    Packet createsession_packet = initialize_packet(NEW_SESS, strlen(session_id)+1, current_client, session_id);
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&createsession_packet, packet_str);

    // send packet
    send(sock_descriptor, packet_str, strlen(packet_str)+1 , 0);
    return 1;
}

int join_session(char* session_id){
    // Can join multiple sessions
    if (!if_logged_in()){
        printf("Error: You haven't logged in yet!\n");
        return -1;
    }
    if (!session_id){
        printf("Usage: /createsession <session_id>\n");
        return -1;
    }

    // Create packet
    Packet join_packet = initialize_packet(JOIN, strlen(session_id)+1, current_client, session_id);
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&join_packet, packet_str);

    // send packet
    send(sock_descriptor, packet_str, strlen(packet_str)+1 , 0);
    return 1;
}

int leave_session(){
    // To be implemented: Specify which sessio to leave
    if (!if_logged_in()){
        printf("Error: You haven't logged in yet!\n");
        return -1;
    }
    if (!in_session){
        printf("Error: You are not in a session!\n");
        return -1;
    }

    // Create packet
    Packet leave_packet = initialize_packet(LEAVE_SESS, 0, current_client, "");
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&leave_packet, packet_str);

    // send packet
    send(sock_descriptor, packet_str, strlen(packet_str)+1 , 0);

    printf("You left session!\n", current_session);
    in_session = 0;
    return 1;
}

int send_text(char* text){
    if(in_session == 0 || sock_descriptor == -1){
        printf("ERROR\n");
        return -1;
    }

    // Create packet
    Packet text_packet = initialize_packet(MESSAGE, strlen(text)+1, current_client, text);
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&text_packet, packet_str);

    // send packet
    send(sock_descriptor, packet_str, strlen(packet_str)+1 , 0);

    return 1;
}

int invite(char* user_id, char* session_id){
    // Invite a user to a session, Can only keep 1 pending request.
    if (!user_id||!session_id){
        printf("Usage: /invite <user_id> <session_id> \n");
        return -1;
    }
    if (!if_logged_in()){
        printf("Error: You haven't logged in yet!\n");
        return -1;
    }
    if(strcmp(user_id, current_client) == 0){
        printf("Error: You cannot invite yourself!\n");
        return -1;
    }
    
    // Create packet
    char user_session_ids[MAX_DATA] = {0};
    strcat(user_session_ids, user_id);
    strcat(user_session_ids, ",");
    strcat(user_session_ids, session_id);
    Packet invite_packet = initialize_packet(INVITE, strlen(user_session_ids)+1, current_client, user_session_ids);
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&invite_packet, packet_str);

    // send packet
    send(sock_descriptor, packet_str, strlen(packet_str)+1 , 0);

    return 1;
}

int accept_invite(){
    if (!if_logged_in()){
        printf("Error: You haven't logged in yet!\n");
        return -1;
    }
    if(strlen(pending_invitation) == 0){
        printf("Error: No pending invitation!\n");
        return -1;
    }

    Packet packet = initialize_packet(ACP, strlen(pending_invitation)+1, current_client, pending_invitation);
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&packet, packet_str);

    // send packet
    send(sock_descriptor, packet_str, strlen(packet_str)+1 , 0);

    // Clear pedning invitation
    memset(pending_invitation, 0, sizeof(pending_invitation));
    return 1;
}

int decline_invite(){
    if (!if_logged_in()){
        printf("Error: You haven't logged in yet!\n");
        return -1;
    }
    if(strlen(pending_invitation) == 0){
        printf("Error: No pending invitation!\n");
        return -1;
    }

    Packet packet = initialize_packet(DEC, strlen(pending_invitation)+1, current_client, pending_invitation);
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&packet, packet_str);

    // send packet
    send(sock_descriptor, packet_str, strlen(packet_str)+1 , 0);

    // Clear pedning invitation
    memset(pending_invitation, 0, sizeof(pending_invitation));
    return 1;
}


void *thread_listen_from_server(){
    // function to be called in a new thread
    // Listen from the server

    while(1){
        char msg[PACKET_SRING_SIZE]={0};
        read(sock_descriptor, msg, PACKET_SRING_SIZE);
        Packet server_packet;
        string2packet(msg, &server_packet);

        if (server_packet.type == MESSAGE){
            if(in_session == 0 || sock_descriptor == -1){
                printf("ERROR: received session msg without joining or logging in!\n");
                return NULL;
            }
            char* source_client = strtok(server_packet.source, ",");
            char* conf_session = strtok(NULL, ",");
            printf("\n--------[From conference %s]--------\n", conf_session);
            printf("[--> %s: %s]\n", source_client, server_packet.data);
            printf("---------------------------------\n");
        }
        else if(server_packet.type == QU_ACK){
            printf("- Current online users and available sessions:\n%s\n", server_packet.data);
        }
        else if(server_packet.type == NS_ACK){
            printf("Session %s is created and joined!\n", server_packet.data);
            strcpy(current_session, server_packet.data);
            in_session = 1;
        }
        else if(server_packet.type == JN_ACK){
            printf("Joined session %s!\n", server_packet.data);
            strcpy(current_session, server_packet.data);
            in_session = 1;
        }
        else if(server_packet.type == JN_NAK){
            printf("Joined session failed. Info<session ID, reason>[%s]!\n", server_packet.data);
        }
        else if(server_packet.type == INVITE){
            char* user_id = strtok(server_packet.data, ",");
            char* conf_session = strtok(NULL, ",");
            printf("User %s invites you to join session %s!\n", server_packet.source, conf_session);
            printf("Enter [/Y] to accept, [/N] to decline:\n");

            // Set invitation info
            memset(pending_invitation, 0, sizeof pending_invitation);
            strcat(pending_invitation, server_packet.source);
            strcat(pending_invitation, ",");
            strcat(pending_invitation, conf_session);
        }
        else if(server_packet.type == DEC){
            // The user declined the invitation
            printf("%s declined your invitation!\n", server_packet.source);
        }
        else if(server_packet.type == ACP){
            // The user accepted the invitation
            printf("%s accepted your invitation!\n", server_packet.source);
        }
        else if(server_packet.type == INVITE_FAILED){
            // The user accepted the invitation
            printf("Invitation failed!\n");
            printf("Reason: %s\n", server_packet.data);
        }

    }

    return NULL;
}


// Helper Functions

int if_logged_in(){
    // Return 1 if logged in, 0 otherwise
    if (sock_descriptor == -1)
        return 0;
    else
        return 1;
}
