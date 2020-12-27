#define SESSION_NAME_SIZE 50
#define SESSION_MAX_ATTENDEE 3

typedef struct Session {
    char id[SESSION_NAME_SIZE];
    int attendee[SESSION_MAX_ATTENDEE];
}Session;

void initialize_session(Session *session, char* session_id, int attendee_index){
    strcpy(session->id, session_id);
    session->attendee[0] = attendee_index;
    for(int i = 1; i < SESSION_MAX_ATTENDEE; i++) 
        session->attendee[i] = -1;
}

int remove_attendee_from_session(Session *session, int attendee_index){
    // Make sure the person is not in the session
    // Return 0 if no more person in this session
    // Return 1 if still has attendee
    int count = 0;
    int found = 0;
    for(int i = 0; i < SESSION_MAX_ATTENDEE; i++){
        if(session->attendee[i] == attendee_index){
            session->attendee[i] = -1;
            if(count) return 1;
            found = 1;
        }

        // If an attendee, then add to count
        if(count == 0) count += session->attendee[i] != -1;
        if(count && found) return 1;
    }
    return count;
}

int attendee_join_session(Session *session, int attendee_index){
    // Assunme the attendee is not attended
    // Return 1 on success
    // Return 0 on max attendee reached
    for(int i = 1; i < SESSION_MAX_ATTENDEE; i++){
        if(session->attendee[i] == -1){
            session->attendee[i] = attendee_index;
            return 1;
        }
    }
    return 0;
}