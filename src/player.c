#include <stdlib.h>
#include <semaphore.h>
#include <string.h>
#include <math.h>
#include "player.h"
#include "debug.h"

struct player {
    char* username;
    int rating;
    int ref;
    sem_t mutex;
};

PLAYER* player_create(char* name) {
    PLAYER* player = (PLAYER*)calloc(1, sizeof(PLAYER));
    if (player == NULL) {
        return NULL;
    }
    player->username = (char*)calloc(strlen(name) + 1, sizeof(char));
    strcpy(player->username, name);
    player->rating = PLAYER_INITIAL_RATING;
    player->ref = 1;
    sem_init(&(player->mutex), 0, 1);
    return player;
}

PLAYER* player_ref(PLAYER* player, char* why) {
    sem_wait(&(player->mutex));
    debug("Increased reference count on player %p (%i -> %i), reason: %s", player, player->ref, player->ref + 1, why);
    player->ref++;
    sem_post(&(player->mutex));
    return player;
}

void player_unref(PLAYER* player, char* why) {
    sem_wait(&(player->mutex));
    debug("Increased reference count on player %p (%i -> %i), reason: %s", player, player->ref, player->ref - 1, why);
    player->ref--;
    if (player->ref == 0) {
        if (player->username != NULL) {
            free(player->username);
            sem_post(&(player->mutex));
            sem_destroy(&(player->mutex));
            free(player);
            player = NULL;
            return;
        }
    }
    sem_post(&(player->mutex));
}

char* player_get_name(PLAYER* player) {
    sem_wait(&(player->mutex));
    char* n = player->username;
    sem_post(&(player->mutex));
    return n;
}

int player_get_rating(PLAYER* player) {
    sem_wait(&(player->mutex));
    int r = player->rating;
    sem_post(&(player->mutex));
    return r;
}

void player_post_result(PLAYER* player1, PLAYER* player2, int result) {
    // need to make sure that it is always acquired in the same order to avoid deadlocks
    // use their memory addresses to make a consistent order as said in class
    PLAYER* lower_player;
    PLAYER* higher_player;
    if (player1 > player2) {
        lower_player = player2;
        higher_player = player1;
    } else if (player2 > player1) {
        lower_player = player1;
        higher_player = player2;
    } else {  // the case that somehow both players are the same
        return;
    }

    sem_wait(&(lower_player->mutex));
    sem_wait(&(higher_player->mutex));
    double s1 = 0;
    double s2 = 0;
    int r1 = player1->rating;
    int r2 = player2->rating;
    if (result == 1) {
        s1 = 1;
    } else if (result == 2) {
        s2 = 1;
    } else {
        s1 = 0.5;
        s2 = 0.5;
    }
    double e1 = 1/(1 + pow(10, (r2-r1)/400));
    double e2 = 1/(1 + pow(10, (r1-r2)/400));
    player1->rating = (int)(r1+32*(s1-e1));
    player2->rating = (int)(r2+32*(s2-e2));
    sem_post(&(lower_player->mutex));
    sem_post(&(higher_player->mutex));
}
