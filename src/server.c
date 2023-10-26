#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "debug.h"
#include "server.h"
#include "jeux_globals.h"

void make_invite_ACK(JEUX_PACKET_HEADER*, int);
void debug_string_plus_next_bit(char*);

void* jeux_client_service(void* arg) {
    // get fd from args
    int fd = *((int*)arg);  // get fd
    free(arg);  // free arg
    pthread_detach(pthread_self());  // detach this thread
    CLIENT* client = creg_register(client_registry, fd);  // register client
    int login = 0;  // user not logged in by default

    // service loop
    while (1) {
        // current packet info
        JEUX_PACKET_HEADER hdr;
        void* payload = NULL;
        void** payloadp = &payload;
        proto_recv_packet(fd, &hdr, payloadp);  // get packet
        debug("%ld: received packet (type = %i, size = %i, time_sec = %u, time_nsec = %u)", pthread_self(), 
            hdr.type, ntohs(hdr.size), ntohl(hdr.timestamp_sec), ntohl(hdr.timestamp_nsec));

        // packet is received in network byte order
        uint8_t recv_packet_type = hdr.type;
        if (!login) {
            //JEUX_PACKET_HEADER temp_hdr;
            if (recv_packet_type != JEUX_LOGIN_PKT) {  // ignore packets until login
                //make_NACK(&temp_hdr, 0);  // format the header into a NACK header
                //proto_send_packet(fd, &temp_hdr, NULL);
                //debug("%lu: sent packet (type = %i, size = %i, time_sec = %lu, time_nsec = %lu)", pthread_self(), 
                    //temp_hdr.type, ntohs(temp_hdr.size), ntohl(temp_hdr.timestamp_sec), ntohl(temp_hdr.timestamp_nsec));
                client_send_nack(client);
                continue;
            } else {  // received packet is a login packet (name in payload)
                PLAYER* login_player = player_create(*payloadp);
                int login_success = -1;
                if (login_player) {
                    login_success = client_login(client, login_player);
                }
                if (login_success == 0) {
                    login = 1;
                    //make_ACK(&temp_hdr, 0);
                    client_send_ack(client, NULL, 0);
                    continue;
                } else {
                    client_send_nack(client);
                    continue;
                }
                //proto_send_packet(fd, &temp_hdr, NULL);
                //debug("%lu: sent packet (type = %i, size = %i, time_sec = %lu, time_nsec = %lu)", pthread_self(), 
                    //temp_hdr.type, ntohs(temp_hdr.size), ntohl(temp_hdr.timestamp_sec), ntohl(temp_hdr.timestamp_nsec));
            }
        }
        
        // at this point: the client is logged in and can send other packets
        JEUX_PACKET_HEADER response_header;
        switch(hdr.type) {
            case JEUX_USERS_PKT:
                PLAYER** players = creg_all_players(client_registry);
                char* users_info = calloc(1, 1);  // creates a string with \0 needed for concatenation
                int current_player = 0;
                while (players[current_player]) {  // players* can be larger than needed, so count until the NULL
                    char* username = player_get_name(players[current_player]);  // assume this is null terminated
                    int int_rating = player_get_rating(players[current_player++]);  // get the rating of current player
                    size_t rating_string_length = log10(int_rating) + 2;  // +1 for correctness, +1 for \0
                    char rating_string[rating_string_length];  // the string that will hold the rating of the current player
                    sprintf(rating_string, "%d", int_rating);
                    size_t new_length = strlen(users_info) + strlen(username) + strlen(rating_string) + 3;  // old size + new name size + \t + rating + \n + null terminator byte
                    users_info = realloc(users_info, new_length);  
                    strcat(users_info, username);  // null terminator is part of username
                    strcat(users_info, "\t");  // concatenate \t between usernames
                    strcat(users_info, rating_string);
                    strcat(users_info, "\n");
                }
                client_send_ack(client, (void*)users_info, strlen(users_info));
                free(players);
                //free(users_info);  // commented because it might be done in client.c in send_ack()
                break;
            case JEUX_INVITE_PKT:
                //payload = username, hdr role = 1 or 2
                // check if the player getting the invite exists
                if (!payloadp) {  // if no user is specified
                    client_send_nack(client);
                    break;
                }
                debug("this reached");
                //char* invited_username = *payloadp;
                //debug("invited username: %s, len = %lu", invited_username, strlen(invited_username));
                //free(*payloadp);

                debug("payload = %s, len = %lu", (char*)(*payloadp), strlen((char*)(*payloadp)));
                //char* invited_username = (char*)calloc(1, (sizeof(char) * (strlen((char*)(*payloadp)) + 1)));
                //memcpy(invited_username, (char*)(*payloadp), strlen((char*)(*payloadp)));
                char* invited_username = (char*)calloc(ntohs(hdr.size) + 1, sizeof(char));
                char* pp = *payloadp;
                int i;
                for (i = 0; i < ntohs(hdr.size); i++) {
                    invited_username[i] = *pp;
                    pp++;
                }
                free(*payloadp);
                //debug("invited username: %s, len = %lu", invited_username, strlen(invited_username));

                if (!strcmp(invited_username, player_get_name(client_get_player(client)))) {  // check the client is not inviting themselves
                    client_send_nack(client);
                    free(invited_username);
                    break;
                }
                debug("this reached 2");
                int source_role = 0;
                int target_role = 0;
                if (hdr.role == 1) {
                    target_role = 1;
                    source_role = 2;
                } else if (hdr.role == 2) {
                    target_role = 2;
                    source_role = 1;
                } else {
                    client_send_nack(client);
                    break;
                }
                debug("this reached 3");
                debug("username = %s", invited_username);
                debug_string_plus_next_bit(invited_username);
                CLIENT* target_client = creg_lookup(client_registry, invited_username);
                free(invited_username);
                if (!target_client) {  // check that a client was found (!= NULL)
                    client_send_nack(client); 
                    break;
                }
                debug("this reached 4");
                // at this point: it is a valid packet

                int id = client_make_invitation(client, target_client, source_role, target_role);
                if (id != -1) {  // success
                    make_invite_ACK(&response_header, id);
                    client_send_packet(client, &response_header, NULL);
                } else {
                    client_send_nack(client);
                }
                break;
            case JEUX_REVOKE_PKT:
                int revoke = client_revoke_invitation(client, hdr.id);
                if (revoke == 0) {  // success
                    client_send_ack(client, NULL, 0);
                } else {
                    client_send_nack(client);
                }
                break;
            case JEUX_DECLINE_PKT:
                int decline = client_decline_invitation(client, hdr.id);
                if (decline == 0) {  // success
                    client_send_ack(client, NULL, 0);
                }
                else {
                    client_send_nack(client);
                }
                break;
            case JEUX_ACCEPT_PKT:
                char* str = NULL;
                char** strp = &str;
                int accept = client_accept_invitation(client, hdr.id, strp);
                if (accept == 0) {
                    if (strp == NULL) {
                        client_send_ack(client, NULL, 0);
                    } else {
                        client_send_ack(client, *strp, strlen(*strp));
                    }
                } else {
                    client_send_nack(client);
                }
                break;
            case JEUX_MOVE_PKT:
                if (payloadp == NULL) {
                    client_send_nack(client);
                    break;
                }
                char* mov_str = (char*)calloc(ntohs(hdr.size) + 1, sizeof(char));
                char* pp_move = *payloadp;
                int a;
                for (a = 0; a < ntohs(hdr.size); a++) {
                    mov_str[a] = *pp_move;
                    pp_move++;
                }
                free(*payloadp);
                int move = client_make_move(client, hdr.id, mov_str);
                free(mov_str);
                if (move == 0) {
                    client_send_ack(client, NULL, 0);
                } else {
                    client_send_nack(client);
                }
                break;
            case JEUX_RESIGN_PKT:
                int resign = client_resign_game(client, hdr.id);
                if (resign == 0) {
                    client_send_ack(client, NULL, 0);
                } else {
                    client_send_nack(client);
                }
                break;
            default:
                //make_NACK(&response_header, 0);
                //proto_send_packet(fd, &response_header, NULL);
                //debug("%lu: sent packet (type = %i, size = %i, time_sec = %lu, time_nsec = %lu)", pthread_self(), 
                    //response_header.type, ntohs(response_header.size), ntohl(response_header.timestamp_sec), ntohl(response_header.timestamp_nsec));
                client_send_nack(client);
                break;
        }
    }
}

void make_invite_ACK(JEUX_PACKET_HEADER* hdr, int invite_id) {
    hdr->type = JEUX_ACK_PKT;
    hdr->size = htons(0);
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    hdr->timestamp_nsec = htonl(tp.tv_nsec);
    hdr->timestamp_sec = htonl(tp.tv_sec);
    hdr->id = invite_id;

    // unused fields
    hdr->role = 0;
}

void debug_string_plus_next_bit(char* str) {
    int len = strlen(str);
    char* stringp = str;
    int i;
    for (i = 0; i <= len; i++) {
        if (*stringp == '\0') {
            debug("0");
        } else if (*stringp == ' ') {
            debug("space");
        } else {
            debug("%c", *stringp);
        }
        stringp++;
    }
}
