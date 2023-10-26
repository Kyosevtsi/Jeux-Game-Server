#include <stdlib.h>
#include <semaphore.h>
#include <string.h>
#include "player_registry.h"
#include "debug.h"

struct player_registry {
    PLAYER** players;
    int player_count;
    sem_t mutex;
};

PLAYER_REGISTRY* preg_init() {
    PLAYER_REGISTRY* preg = (PLAYER_REGISTRY*)calloc(1, sizeof(PLAYER_REGISTRY));
    if (preg == NULL) {
        return NULL;
    }
    preg->players = (PLAYER**)calloc(1, sizeof(PLAYER*));
    preg->player_count = 0;
    sem_init(&(preg->mutex), 0, 1);
    return preg;
}

void preg_fini(PLAYER_REGISTRY* preg) {
    int i;
    for (i = 0; i < preg->player_count; i++) {
        free(preg->players[i]);
    }
    sem_destroy(&(preg->mutex));
    free(preg);
}

PLAYER* preg_register(PLAYER_REGISTRY* preg, char* name) {
    sem_wait(&(preg->mutex));
    int i;
    for (i = 0; i < preg->player_count; i++) {
        PLAYER* player = preg->players[i];
        char* player_name = player_get_name(player);
        if (strcmp(player_name, name) == 0) {
            player_ref(player, "player registry (register) call");
            sem_post(&(preg->mutex));
            return player;
        }
    }
    // at this point: existing player name was not found
    preg->player_count++;  // increment player count to add another player
    preg->players = (PLAYER**)realloc(preg->players, (preg->player_count + 1) * sizeof(PLAYER*));
    PLAYER* player = player_ref(player_create(name), "player registry (register) call");
    preg->players[preg->player_count - 1] = player;
    preg->players[preg->player_count] = NULL;  // end the registry with a null pointer
    sem_post(&(preg->mutex));
    return player;
}