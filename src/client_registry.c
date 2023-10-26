#include "client_registry.h"
#include "debug.h"
#include "jeux_globals.h"
#include <stdlib.h>
#include <semaphore.h>
#include <string.h>

struct client_registry {
    // only one client can make use of the registry as the time
    CLIENT* clients[MAX_CLIENTS];
    int client_index;  // the next index that will be used when a client is registered
    int num_of_clients;
    int waiting_threads;
    sem_t mutex;  // main semaphore for the client registry
    sem_t empty;  // = 0 if clients has anything in it

    // option1: make another binary semaphore that will keep track of if num_of_clients = 0
    // option2: change from tracking number of available slots to tracking number of occupied slots
};

CLIENT_REGISTRY* creg_init() {
    CLIENT_REGISTRY* registry = (CLIENT_REGISTRY*)calloc(1, sizeof(CLIENT_REGISTRY));  // calloc to initialize client registry to 0
    if (registry == NULL) {
        return NULL;
    }
    sem_init(&(registry->mutex), 0, 1);
    sem_init(&(registry->empty), 0, 1);  // the registry is empty to start
    //sem_init(&(registry->waiting_threads), 0, 0);
    registry->client_index = 0;  // next client goes to index 0 of the array
    registry->num_of_clients = 0;
    registry->waiting_threads = 0;
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        registry->clients[i] = NULL;
    }
    debug("Initialize client registry");
    return registry;
}

void creg_fini(CLIENT_REGISTRY* cr) {
    // ignore checking
    sem_destroy(&(cr->mutex));
    sem_destroy(&(cr->empty));
    free(cr);

    // if clients need to be checked
    /*sem_wait(&(cr->mutex));  // block so that the number of clients can safely be checked
    if (cr->num_of_clients == 0) {  // check that no clients are remaining in the registry
        sem_destroy(&(cr->occupied));
        sem_destroy(&(cr->mutex));
        free(cr);
    } else {
        sem_post(&(cr->mutex));
    }*/
}

CLIENT* creg_register(CLIENT_REGISTRY *cr, int fd) {
    sem_wait(&(cr->mutex));  // block
    int index = cr->client_index;  // get next free index 
    CLIENT* c = client_create(cr, fd);  // make a client from fd
    cr->clients[index] = c;
    if (cr->clients[index] == NULL) {  // check that a client was actually created
        sem_post(&(cr->mutex));
        return NULL;
    }
    while (cr->clients[index] != NULL) {  // find the next available slot in the array
        index++;
    }
    cr->client_index = index;  // save next available slot in the registry
    cr->num_of_clients++;  // track client count
    if (cr->num_of_clients == 1) {  // first client added
        sem_wait(&(cr->empty));
    }
    sem_post(&(cr->mutex));  // unblock
    return c;
}

int creg_unregister(CLIENT_REGISTRY* cr, CLIENT* client) {
    sem_wait(&(cr->mutex));
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {  // search for the client in the array
        if (cr->clients[i] == client) { 
            client_unref(client, "client registry (unregister) call");
            cr->clients[i] = NULL;
            if (i < cr->client_index) {
                cr->client_index = i;
            }
            cr->num_of_clients--;
            if (cr->num_of_clients == 0) {
                sem_post(&(cr->empty));
            }
            sem_post(&(cr->mutex));
            return 0;  // successful removal
        }
    }
    sem_post(&(cr->mutex));
    return -1;  // client not found
}

CLIENT* creg_lookup(CLIENT_REGISTRY* cr, char* user) {
    sem_wait(&(cr->mutex));
    int i;
    CLIENT* c = NULL;  // for returning client
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (cr->clients[i]) {
            char* name = player_get_name(client_get_player(cr->clients[i]));
            debug("name: %s, len = %lu", name, strlen(name));
            if (strcmp(user, name) == 0) {  // name found
                debug("name found!");
                c = cr->clients[i];
                client_ref(c, "client registry (lookup) call");
                break;
            }
        }
    }
    sem_post(&(cr->mutex));
    return c;
}

PLAYER** creg_all_players(CLIENT_REGISTRY* cr) {
    sem_wait(&(cr->mutex));
    int players_index = 0;
    int total_players = cr->num_of_clients;
    PLAYER** players = (PLAYER**)malloc(sizeof(PLAYER*) * total_players + 1);  // allocate space for all players and NULL
    // guaranteed to be big enough
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (cr->clients[i]) {
            PLAYER* player = NULL;
            if ((player = client_get_player(cr->clients[i]))) {
                players[players_index++] = player;
                player_ref(player, "client registry (all players) call");  // increase player reference for each added
            }
        }
    }
    players[players_index] = NULL;  // null terminate the end (could be very end of malloced memory or in the middle)
    sem_post(&(cr->mutex));
    return players;
}

void creg_wait_for_empty(CLIENT_REGISTRY* cr) {
    sem_wait(&(cr->mutex));
    cr->waiting_threads++;  // the number of threads waiting is incremented every call
    sem_post(&(cr->mutex));
    sem_wait(&(cr->empty));  // wait for the registry to be empty
    int i;
    for (i = 0; i < cr->waiting_threads; i++) {
        sem_post(&(cr->empty));  // allow other threads that called wait_for_empty() to use the sem_t
    }
    // at the end: empty should be 1 because waiting_threads is a minimum of 1
}

void creg_shutdown_all(CLIENT_REGISTRY* cr) {
    sem_wait(&(cr->mutex));
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (cr->clients[i]) {
            shutdown(client_get_fd(cr->clients[i]), SHUT_RD);
        }
    }
    sem_post(&(cr->mutex));
}