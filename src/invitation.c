#include <stdlib.h>
#include <semaphore.h>
#include "debug.h"
#include "player.h"
#include "client_registry.h"
#include "jeux_globals.h"

struct invitation {
    INVITATION_STATE state;
    CLIENT* source;
    CLIENT* target;
    GAME_ROLE source_role;
    GAME_ROLE target_role;
    GAME* game;
    int ref;
    sem_t mutex;
};

INVITATION* inv_create(CLIENT* source, CLIENT* target, GAME_ROLE source_role, GAME_ROLE target_role) {
    INVITATION* invitation = (INVITATION*)calloc(1, sizeof(INVITATION));
    if (invitation == NULL) {
        return NULL;
    }
    invitation->state = INV_OPEN_STATE;  // invitations are open on initialization
    invitation->source = source;
    invitation->target = target;
    invitation->source_role = source_role;
    invitation->target_role = target_role;
    invitation->game = NULL;  // there is no game upon initialization
    invitation->ref = 1;  // this is the only reference on initialization
    sem_init(&(invitation->mutex), 0, 1);
    return invitation;
}

INVITATION* inv_ref(INVITATION* inv, char* why) {
    sem_wait(&(inv->mutex));
    debug("Increased reference count on invitation %p (%i -> %i), reason: %s", inv, inv->ref, inv->ref + 1, why);
    inv->ref++;
    sem_post(&(inv->mutex));
    return inv;
}

void inv_unref(INVITATION* inv, char* why) {
    sem_wait(&(inv->mutex));
    debug("Decreased reference count on invitation %p (%i -> %i), reason: %s", inv, inv->ref, inv->ref - 1, why);
    inv->ref--;
    if (inv->ref == 0) {
        if (inv->game != NULL) {
            free(inv->game);
            inv->game = NULL;
        }
        sem_post(&(inv->mutex));
        sem_destroy(&(inv->mutex));
        free(inv);
        inv = NULL;
    } else {
        sem_post(&(inv->mutex));
    }
}

CLIENT* inv_get_source(INVITATION* inv) {
    sem_wait(&(inv->mutex));
    CLIENT* c = inv->source;
    sem_post(&(inv->mutex));
    return c;
}

CLIENT* inv_get_target(INVITATION* inv) {
    sem_wait(&(inv->mutex));
    CLIENT* c = inv->target;
    sem_post(&(inv->mutex));
    return c;
}

GAME_ROLE inv_get_source_role(INVITATION* inv) {
    sem_wait(&(inv->mutex));
    GAME_ROLE r = inv->source_role;
    sem_post(&(inv->mutex));
    return r;
}

GAME_ROLE inv_get_target_role(INVITATION* inv) {
    sem_wait(&(inv->mutex));
    GAME_ROLE r = inv->target_role;
    sem_post(&(inv->mutex));
    return r;
}

GAME* inv_get_game(INVITATION* inv) {
    sem_wait(&(inv->mutex));
    GAME* g = inv->game;
    if (g == NULL) {
        sem_post(&(inv->mutex));
        return NULL;
    } else {
        sem_post(&(inv->mutex));
        return g;
    }
}

int inv_accept(INVITATION* inv) {
    sem_wait(&(inv->mutex));
    if (inv->state != INV_OPEN_STATE) {
        sem_post(&(inv->mutex));
        return -1;
    }
    inv->state = INV_ACCEPTED_STATE;
    inv->game = game_create();
    sem_post(&(inv->mutex));
    return 0;
}

int inv_close(INVITATION* inv, GAME_ROLE role) {
    sem_wait(&(inv->mutex));
    if (role == NULL_ROLE) {
        if (inv->game == NULL) {
            inv->state = INV_CLOSED_STATE;
            sem_post(&(inv->mutex));
            return 0;
        } else {
            sem_post(&(inv->mutex));
            return -1;
        }
    } else {
        game_resign(inv->game, role);
        inv->state = INV_CLOSED_STATE;
        sem_post(&(inv->mutex));
        return 0;
    }
}
