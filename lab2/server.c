#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include <pthread.h>

#include "packet.h"
#include "session.h"

#define MAX_SESSION 10

#define HERE(X) (printf("HERE %d\n", X));

// Sessions
Session session_list[MAX_SESSION];
int session_in_use[MAX_SESSION];
pthread_mutex_t session_lock;

// User Database
int N_USERS = 5;
char* USER_ID_LIST[] = {"aaron1", "aaron2", "aaron3", "user1", "user2"};
char* USER_PWD_LIST[] = {"123", "234", "345", "1", "2"};
int connected[] = {-1, -1, -1, -1, -1}; // user connection socket
pthread_mutex_t connect_lock;

// For thread
void *establish_connection(void *arg);

// Control Routines
int login(Packet* packet, int socket);
int logout(Packet* packet, int socket);
int query(Packet* packet, int socket);
int create_session(Packet* packet, int socket);
int join_session(Packet* packet, int socket);
int leave_session(Packet* packet, int socket);
int broadcast(Packet* packet, int socket);
int invite(Packet* packet, int socket);
int accept_invite(Packet* packet, int socket);
int decline_invite(Packet* packet, int socket);

// Helper Functions
int authenticate_login(const char* user_id, const char* pwd, int socket);
int user_id2index(char* user_id);
int session_id2index(char* session_id);
void destroy_session(int session_index);


int listen_at_port(int port, struct sockaddr_in *server_addr){
    // Create socket descriptor
    int sock_descriptor;
    sock_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_descriptor == -1){
        perror("Failure in socket creation"); 
        exit(0); 
    }

    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr->sin_port = htons(port);

    // bind to socket
    if (bind(sock_descriptor, (struct sockaddr *)server_addr, sizeof(*server_addr))<0){
        printf("Error in binding\n");
        exit(0); 
    } 
    // Listen from the client
    if (listen(sock_descriptor, 10) < 0){ 
        printf("Error in listening...\n"); 
        exit(0); 
    }
    printf("Listening at port %d ...\n", port); 
    return sock_descriptor;
}


int main(int argc, char const *argv[]){
    // check argument
    if (argc != 2) { 
        printf("usage: server -<TCP port number to listen on>\n");
        exit(0);
    }

    // Initialize variables
    if (pthread_mutex_init(&connect_lock, NULL) != 0){
        printf("\n mutex connect_lock init failed\n");
        exit(0);
    }
    if (pthread_mutex_init(&session_lock, NULL) != 0){
        printf("\n mutex session_lock init failed\n");
        exit(0);
    }
    memset(session_list, 0, MAX_SESSION * sizeof(Session));
    for(int i =0; i!= MAX_SESSION; i++) session_in_use[i] = 0;

    // Listen to port
    int port = atoi(argv[1]);
    struct sockaddr_in server_addr;
    int sock_descriptor = listen_at_port(port, &server_addr);
    while(1){
        int *new_socket = malloc(sizeof(int));
        struct sockaddr client_addr;
        int addrlen = sizeof(client_addr);
        // Create a new TCP connection upon a connection request from a client
        if ((*new_socket = accept(sock_descriptor, (struct sockaddr *)&client_addr, (socklen_t*)&addrlen))<0) 
            printf("server doesn't accept the client\n"); 
        else{
            // 1 thread to 1 client
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, establish_connection, (void*)new_socket);
        }
    }
    return 0; 
}


void *establish_connection(void *arg){
    // function in each thread
    int new_socket = *(int *)arg;

    char packet_str[PACKET_SRING_SIZE];
    read(new_socket, packet_str, PACKET_SRING_SIZE); 
    
    // Process Msg
    Packet packet;
    string2packet(packet_str, &packet);

    //Login
    if (packet.type == LOGIN){
        login(&packet, new_socket);
    }
    else{
        // Need to login first
        printf("Need to login first\n");
        return NULL;
    }

    // Keep listening from this client
    while(1){
        char packet_str[PACKET_SRING_SIZE];
        read(new_socket, packet_str, PACKET_SRING_SIZE); 

        Packet packet;
        string2packet(packet_str, &packet);
        printf(" - Received request %d from Client [%s]...\n", packet.type, packet.source);

        // ************************Process Request************************
        if (packet.type == LOGIN){
            printf("Error: Client [%s] already logged in\n", packet.source);
            continue;
        }
        else if(packet.type == EXIT){
            logout(&packet, new_socket);
            return NULL;
        }
        else if(packet.type == QUERY){
            query(&packet, new_socket);
        }
        else if(packet.type == NEW_SESS){
            create_session(&packet, new_socket);
        }
        else if(packet.type == JOIN){
            join_session(&packet, new_socket);
        }
        else if(packet.type == LEAVE_SESS){
            leave_session(&packet, new_socket);
        }
        else if(packet.type == MESSAGE){
            broadcast(&packet, new_socket);
        }
        else if(packet.type == INVITE){
            invite(&packet, new_socket);
        }
        else if(packet.type == ACP){
            accept_invite(&packet, new_socket);
        }
        else if(packet.type == DEC){
            decline_invite(&packet, new_socket);
        }
        else{
            printf("Unrecognized packet_str: %s\n", packet_str);
        }
        // ****************************************************************
    }
}


//********************Control Routines**********************************
int login(Packet* packet, int socket){
    // Check username and password
    char* user_id = strtok(packet->data, ",");
    char* pwd = strtok(NULL, " ");

    pthread_mutex_lock(&connect_lock);
    int authentication = authenticate_login(user_id, pwd, socket);
    pthread_mutex_unlock(&connect_lock);

    if (authentication == 1){
        char* LO_ACK_msg = "1:0:server:"; // LO_ACK
        send(socket, LO_ACK_msg, strlen(LO_ACK_msg)+1 , 0);
        printf("Client: [%s] is connected\n", packet->source);
        return 1;
    }
    else{
        char* NAK_msg;
        if(authentication == 0)
            NAK_msg = "Incorrect Username or Password";
        else
            NAK_msg = "This user is logged in somewehre else";
        Packet reply_packet = initialize_packet(LO_NAK, strlen(NAK_msg)+1, "server", NAK_msg);
        char packet_str[PACKET_SRING_SIZE];
        packet2string(&reply_packet, packet_str);
        send(socket, packet_str, strlen(packet_str)+1 , 0);
        return -1;
    }
}


int logout(Packet* packet, int socket){
    // Return -1 on logout failure, 1 on success
    
    // Logout user
    int user_index = user_id2index(packet->source);
    pthread_mutex_lock(&connect_lock);
    connected[user_index] = -1;
    pthread_mutex_unlock(&connect_lock);
        
    // leave meeting
    leave_session(packet, socket);
    printf("Client [%s] logged out!\n", packet->source);

    // close connection
    if (close(socket)!=0){
        printf("Error in closing socket\n");
        return -1;
    }
    return 1;
}


int create_session(Packet* packet, int socket){
    // Return 1 on session created 
    // Return -1 on session max count reached
    char* session_id = packet->data;
    // add session in session list
    int created = 0; // 1 if session created, 0 if not
    for(int i = 0; i!=MAX_SESSION; i++){
        pthread_mutex_lock(&session_lock);
        if(session_in_use[i] == 0){
            int attendee_index = user_id2index(packet->source);
            initialize_session(&session_list[i], session_id, attendee_index);
            session_in_use[i] = 1;
            created = 1;
            pthread_mutex_unlock(&session_lock);
            break;
        }
        pthread_mutex_unlock(&session_lock);
    }

    if(created == 0){
        // Max session number reached
        printf("Session creation failed: Max session number reached!\n");
        return -1;
    }

    // Reply with ack
    printf("Session %s is created!\n", session_id);
    Packet reply_packet = initialize_packet(NS_ACK, strlen(session_id)+1, "server", session_id);
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&reply_packet, packet_str);
    send(socket, packet_str, strlen(packet_str)+1 , 0);
    return 1;
}


int join_session(Packet* packet, int socket){
    char* session_id = packet->data;
    int attendee_index = user_id2index(packet->source);

    //get session index
    pthread_mutex_lock(&session_lock);
    int session_index = session_id2index(session_id);
    pthread_mutex_unlock(&session_lock);
    if (session_index == -1){
        // Session not found
        printf("Error: Client [%s] joined session failed %s\n", packet->source, session_id);
        char msg[50] = {0};
        strcat(msg, session_id);
        strcat(msg, ",");
        strcat(msg, "Failed! Session not found!");

        Packet reply_packet = initialize_packet(JN_NAK, strlen(msg)+1, "server", msg);
        char packet_str[PACKET_SRING_SIZE];
        packet2string(&reply_packet, packet_str);
        send(socket, packet_str, strlen(packet_str)+1 , 0);
        return -1;
    }
    
    // join session
    pthread_mutex_lock(&session_lock);
    int attended = attendee_join_session(&session_list[session_index], attendee_index);
    pthread_mutex_unlock(&session_lock);

    // reply
    if(attended){
        Packet reply_packet = initialize_packet(JN_ACK, strlen(session_id)+1, "server", session_id);
        char packet_str[PACKET_SRING_SIZE];
        packet2string(&reply_packet, packet_str);
        send(socket, packet_str, strlen(packet_str)+1 , 0);
        printf("Client [%s] joined session %s\n", packet->source, session_id);
        return 1;
    }
    else{
        printf("Error: Client [%s] joined session failed %s\n", packet->source, session_id);
        char msg[50] = {0};
        strcat(msg, session_id);
        strcat(msg, ",");
        strcat(msg, "Failed! Session is full!");

        Packet reply_packet = initialize_packet(JN_NAK, strlen(msg)+1, "server", msg);
        char packet_str[PACKET_SRING_SIZE];
        packet2string(&reply_packet, packet_str);
        send(socket, packet_str, strlen(packet_str)+1 , 0);
        return -1;
    }
}


int leave_session(Packet* packet, int socket){
    int user_index = user_id2index(packet->source);
    pthread_mutex_lock(&session_lock);
    for (int i = 0; i<MAX_SESSION; i++){
        if(session_in_use[i]){
            int remaining = remove_attendee_from_session(&session_list[i], user_index);
            // Destroy session if no one remains
            if(remaining == 0) destroy_session(i);
        }
    }
    pthread_mutex_unlock(&session_lock);
    printf("Client [%s] left session!\n", packet->source);
}


int query(Packet* packet, int socket){
    // Get current connected clients
    char rpl_msg[512];
    memset(rpl_msg, 0, sizeof(char) * 512);
    strcpy(rpl_msg, "Online clients:\n");
    int first = 1;
    for(int i = 0; i!=N_USERS; i++){
        pthread_mutex_lock(&connect_lock);
        if(connected[i]!=-1){
            if(!first) strcat(rpl_msg, ", ");
            else first = 0;
            strcat(rpl_msg, USER_ID_LIST[i]);
        }
        pthread_mutex_unlock(&connect_lock);
    }
    if(first) strcat(rpl_msg, "<Empty>");
    // Get current sessions
    strcat(rpl_msg, "\nCurrent sessions:\n");
    first = 1;
    for (int i=0; i!=MAX_SESSION; i++){
        pthread_mutex_lock(&session_lock);
        if(session_in_use[i]){
            if(!first) strcat(rpl_msg, "\n");
            else first = 0;
            strcat(rpl_msg, session_list[i].id);
            strcat(rpl_msg, ", attendees:");

            int first_attendee = 1;
            for(int j = 0; j!=SESSION_MAX_ATTENDEE; j++){
                if(session_list[i].attendee[j]!=-1){
                    if(!first_attendee) strcat(rpl_msg, ", ");
                    else first_attendee=0;
                    strcat(rpl_msg, USER_ID_LIST[session_list[i].attendee[j]]);
                }
            }
        }
        pthread_mutex_unlock(&session_lock);
    }
    if(first) strcat(rpl_msg, "<Empty>");

    // reply with query msg
    // printf("query =\n%s\n", rpl_msg);
    Packet reply_packet = initialize_packet(QU_ACK, strlen(rpl_msg)+1, "server", rpl_msg);
    memset(rpl_msg, 0, sizeof(rpl_msg));
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&reply_packet, packet_str);
    send(socket, packet_str, strlen(packet_str)+1 , 0);
    return 1;
}

int broadcast(Packet* packet, int socket){
    // Slow broadcast. big complexity, loop through the session list and find all corresponding users to send msg

    int user_index = user_id2index(packet->source);

    // Broadcast to all client in the same session
    pthread_mutex_lock(&session_lock);
    for(int i =0; i!= MAX_SESSION; i++){
        if(session_in_use[i]){
            int in = 0; // is 1 if the client is in this session
            for(int j = 0; j!= SESSION_MAX_ATTENDEE; j++){
                if (session_list[i].attendee[j] == user_index){
                    in = 1;
                    break;
                }
            }
            if(in){
                // if client in the session, broadcast msg to all other clients 
                printf("Broadcast msg in session %s\n", session_list[i].id);
                for(int j = 0; j!= SESSION_MAX_ATTENDEE; j++){
                    if(session_list[i].attendee[j] != -1){
                        // Indicate both the session and user id in the source
                        char source[500] = {0};
                        strcat(source, packet->source);
                        strcat(source, ",");
                        strcat(source, session_list[i].id);
                        Packet msg_packet = initialize_packet(MESSAGE, strlen(packet->data)+1, source, packet->data);
                        char packet_str[PACKET_SRING_SIZE];
                        packet2string(&msg_packet, packet_str);

                        pthread_mutex_lock(&connect_lock);
                        int client_socket = connected[session_list[i].attendee[j]];
                        pthread_mutex_unlock(&connect_lock);

                        send(client_socket, packet_str, strlen(packet_str)+1 , 0);
                    }
                }
            }
        }
    }
    pthread_mutex_unlock(&session_lock);

    return 1;
}


int invite(Packet* packet, int socket){
    char data_cpy[MAX_DATA] = {0};
    strcpy(data_cpy,packet->data);
    char* user_id = strtok(data_cpy, ",");
    char* session_id = strtok(NULL, ",");
    printf("Invite user [%s]  to join session [%s]\n", user_id, session_id);

    int user_index = user_id2index(user_id);

    if (user_index == -1){
        // User not found
        printf("Error: User no found!\n");
        char reason[] = "User no found!";

        Packet rply_packet = initialize_packet(INVITE_FAILED, strlen(reason)+1, packet->source, reason);
        char packet_str[PACKET_SRING_SIZE];
        packet2string(&rply_packet, packet_str);

        send(socket, packet_str, strlen(packet_str)+1 , 0);

        return -1;
    }

    // Get user's acceptance / decline
    Packet ask_packet = initialize_packet(INVITE, strlen(packet->data)+1, packet->source, packet->data);
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&ask_packet, packet_str);

    pthread_mutex_lock(&connect_lock);
    int client_socket = connected[user_index];
    pthread_mutex_unlock(&connect_lock);

    if (client_socket == -1){
        // User is offline
        printf("Error: User not online!\n");
        char reason[] = "User is not online";

        Packet rply_packet = initialize_packet(INVITE_FAILED, strlen(reason)+1, packet->source, reason);
        char packet_str[PACKET_SRING_SIZE];
        packet2string(&rply_packet, packet_str);

        send(socket, packet_str, strlen(packet_str)+1 , 0);

        return -1;
    }

    send(client_socket, packet_str, strlen(packet_str)+1 , 0);
}

int accept_invite(Packet* packet, int socket){
    char data_cpy[MAX_DATA] = {0};
    strcpy(data_cpy,packet->data);
    char* inviter = strtok(data_cpy, ",");
    char* conf_session = strtok(NULL, ",");
    char* invitee = packet->source;

    printf("Client [%s] accepted invitation to join session %s\n", invitee, conf_session);

    // Join session
    Packet join_packet = initialize_packet(JOIN, strlen(conf_session)+1, invitee, conf_session);
    join_session(&join_packet, socket);

    // Send msg to the inviter.
    Packet rply_packet = initialize_packet(ACP, strlen(packet->data)+1, invitee, packet->data);
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&rply_packet, packet_str);

    pthread_mutex_lock(&connect_lock);
    int client_socket = connected[user_id2index(inviter)];
    pthread_mutex_unlock(&connect_lock);

    send(client_socket, packet_str, strlen(packet_str)+1 , 0);

    return 1;
}


int decline_invite(Packet* packet, int socket){
    char data_cpy[MAX_DATA] = {0};
    strcpy(data_cpy,packet->data);
    char* inviter = strtok(data_cpy, ",");
    char* conf_session = strtok(NULL, ",");
    char* invitee = packet->source;

    printf("Client [%s] declined invitation to join session %s\n", invitee, conf_session);

    // Send msg to the inviter.
    Packet rply_packet = initialize_packet(DEC, strlen(packet->data)+1, invitee, packet->data);
    char packet_str[PACKET_SRING_SIZE];
    packet2string(&rply_packet, packet_str);

    pthread_mutex_lock(&connect_lock);
    int client_socket = connected[user_id2index(inviter)];
    pthread_mutex_unlock(&connect_lock);

    send(client_socket, packet_str, strlen(packet_str)+1 , 0);

    return 1;
}




// *********************Helper Functions**********************************
// Helper function cannot require locks

int authenticate_login(const char* user_id, const char* pwd, int socket){
    // Requires Lock from caller
    // Return 1 on successful authentication
    // Return 0 on incorrect Username or Password
    // Return -1 on Client is logged in some where else
    for(int i =0; i!= N_USERS; i++){
        if(strcmp(user_id, USER_ID_LIST[i]) == 0 && strcmp(pwd, USER_PWD_LIST[i]) == 0){
            //check if connected
            
            if(connected[i] != -1) return -1;
            else{
                connected[i] = socket;
                return 1;
            }
        }
    }
    return 0;
}

int user_id2index(char* user_id){
    // Find user id index given user id
    for(int i =0; i!= N_USERS; i++){
        if(strcmp(user_id, USER_ID_LIST[i]) == 0)
        return i;
    }
    printf("ERROR user [%s] not found!\n", user_id);
    return -1;
}

int session_id2index(char* session_id){
    // Need session lock
    for(int i =0; i!= MAX_SESSION; i++){
        if(session_in_use[i] && strcmp(session_id, session_list[i].id) == 0)
            return i;
    }
    printf("ERROR session %s not found!\n", session_id);
    return -1;
}

void destroy_session(int session_index){
    // Need lock on caller
    char sesson_id[SESSION_NAME_SIZE];
    strcpy(sesson_id, session_list[session_index].id);

    memset(session_list + session_index, 0, sizeof(Session));
    session_in_use[session_index] = 0;
    printf("Session %s destroyed!\n", sesson_id);
}
