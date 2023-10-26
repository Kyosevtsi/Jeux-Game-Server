#include <stdlib.h>
#include <semaphore.h>
#include <string.h>
#include "game.h"
#include "debug.h"

struct game {
    char game_info[9];
    int ref;
    int is_over;
    GAME_ROLE winner;
    GAME_ROLE turn;
    sem_t mutex;
};

struct game_move {
    GAME_ROLE role;
    int move;  // 1 - 9
};

GAME* game_create() {
    GAME* game = (GAME*)calloc(1, sizeof(GAME));
    if (game == NULL) {
        return NULL;
    }
    int i;
    for (i = 0; i < 9; i++) {
        game->game_info[i] = ' ';
    }
    game->ref = 1;
    game->is_over = 0;
    game->winner = NULL_ROLE;
    game->turn = FIRST_PLAYER_ROLE;
    sem_init(&(game->mutex), 0, 1);
    return game;
}

GAME* game_ref(GAME* game, char* why) {
    sem_wait(&(game->mutex));
    debug("Increased reference count on invitation %p (%i -> %i), reason: %s", game, game->ref, game->ref + 1, why);
    game->ref++;
    sem_post(&(game->mutex));
    return game;
}

void game_unref(GAME* game, char* why) {
    sem_wait(&(game->mutex));
    debug("Decreased reference count on invitation %p (%i -> %i), reason: %s", game, game->ref, game->ref - 1, why);
    game->ref--;
    if (game->ref == 0) {
        sem_post(&(game->mutex));
        sem_destroy(&(game->mutex));
        free(game);
    }
    sem_post(&(game->mutex));
}

int game_apply_move(GAME* game, GAME_MOVE* game_move) {
    sem_wait(&(game->mutex));
    // check if a move is legal
    if (game->game_info[game_move->move - 1] != ' ') {  // slot must be empty
        debug("illegal move");
        sem_post(&(game->mutex));
        return -1;
    }
    if (game->is_over) {  // game must not be over
        debug("cannot move on completed game");
        sem_post(&(game->mutex));
        return -1;
    }
    char symbol = ' ';
    if (game_move->role == FIRST_PLAYER_ROLE) {
        symbol = 'X';
    } else {
        symbol = 'O';
    }
    game->game_info[game_move->move - 1] = symbol;  // apply the move
    
    // update turn
    if (game->turn == FIRST_PLAYER_ROLE) {
        game->turn = SECOND_PLAYER_ROLE;
    } else {
        game->turn = FIRST_PLAYER_ROLE;
    }

    // check for a winner
    if (game->winner == NULL_ROLE) {
        // check rows
        if ((game->game_info[0] == symbol) && (game->game_info[0] == game->game_info[1]) && (game->game_info[1] == game->game_info[2])) {
            game->winner = game_move->role;
            game->is_over = 1;
        }
        if ((game->game_info[3] == symbol) && (game->game_info[3] == game->game_info[4]) && (game->game_info[4] == game->game_info[5])) {
            game->winner = game_move->role;
            game->is_over = 1;
        }
        if ((game->game_info[6] == symbol) && (game->game_info[6] == game->game_info[7]) && (game->game_info[7] == game->game_info[8])) {
            game->winner = game_move->role;
            game->is_over = 1;
        }

        // check cols
        if ((game->game_info[0] == symbol) && (game->game_info[0] == game->game_info[3]) && (game->game_info[3] == game->game_info[6])) {
            game->winner = game_move->role;
            game->is_over = 1;
        }
        if ((game->game_info[1] == symbol) && (game->game_info[1] == game->game_info[4]) && (game->game_info[4] == game->game_info[7])) {
            game->winner = game_move->role;
            game->is_over = 1;
        }
        if ((game->game_info[2] == symbol) && (game->game_info[2] == game->game_info[5]) && (game->game_info[5] == game->game_info[8])) {
            game->winner = game_move->role;
            game->is_over = 1;
        }

        // check diags
        if ((game->game_info[0] == symbol) && (game->game_info[0] == game->game_info[4]) && (game->game_info[4] == game->game_info[8])) {
            game->winner = game_move->role;
            game->is_over = 1;
        }
        if ((game->game_info[2] == symbol) && (game->game_info[2] == game->game_info[4]) && (game->game_info[4] == game->game_info[6])) {
            game->winner = game_move->role;
            game->is_over = 1;
        }
    }
    sem_post(&(game->mutex));
    return 0;
}

int game_resign(GAME* game, GAME_ROLE role) {
    sem_wait(&(game->mutex));
    if (game->is_over) {
        sem_post(&(game->mutex));
        return -1;
    }
    game->is_over = 1;
    if (role == FIRST_PLAYER_ROLE) {
        game->winner = SECOND_PLAYER_ROLE;
    } else {
        game->winner = FIRST_PLAYER_ROLE;
    }
    sem_post(&(game->mutex));
    return 0;
}

char* game_unparse_state(GAME* game) {
    sem_wait(&(game->mutex));
    char* parsed_game = (char*)calloc(41, sizeof(char));  // ends with null terminator
    int i;
    // manual checking hell
    parsed_game[0] = game->game_info[0];
    parsed_game[1] = '|';
    parsed_game[2] = game->game_info[1];
    parsed_game[3] = '|';
    parsed_game[4] = game->game_info[2];

    parsed_game[5] = '\n';

    for (i = 6; i < 11; i++) {
        parsed_game[i] = '-';
    }

    parsed_game[11] = '\n';

    parsed_game[12] = game->game_info[3];
    parsed_game[13] = '|';
    parsed_game[14] = game->game_info[4];
    parsed_game[15] = '|';
    parsed_game[16] = game->game_info[5];

    parsed_game[17] = '\n';

    for (i = 18; i < 23; i++) {
        parsed_game[i] = '-';
    }

    parsed_game[23] = '\n';

    parsed_game[24] = game->game_info[6];
    parsed_game[25] = '|';
    parsed_game[26] = game->game_info[7];
    parsed_game[27] = '|';
    parsed_game[28] = game->game_info[8];

    parsed_game[29] = '\n';

    if (game->turn == FIRST_PLAYER_ROLE) {
        parsed_game[30] = 'X';
    } else {
        parsed_game[30] = 'O';
    }
    strcat(parsed_game, " to move");
    debug("game: %s", parsed_game);
    sem_post(&(game->mutex));
    return parsed_game;
}

int game_is_over(GAME* game) {
    sem_wait(&(game->mutex));
    int over = game->is_over;
    sem_post(&(game->mutex));
    return over;
}

GAME_ROLE game_get_winner(GAME* game) {
    sem_wait(&(game->mutex));
    GAME_ROLE winner_role = game->winner;
    sem_post(&(game->mutex));
    return winner_role;
}

GAME_MOVE* game_parse_move(GAME* game, GAME_ROLE role, char* str) {
    sem_wait(&(game->mutex));
    // check the string first
    int pos = 0;
    if (strlen(str) == 1) {
        pos = (int)(str[0] - '0');
        if ((pos < 1) || (pos > 9)) {  // invalid
            debug("invalid move");
            sem_post(&(game->mutex));
            return NULL;
        }
    } else if (strlen(str) == 4) {
        if (str[1] != '<') {  // invalid
            debug("invalid move");
            sem_post(&(game->mutex));
            return NULL;
        }
        if (str[2] != '-') {  // invalid
            debug("invalid move");
            sem_post(&(game->mutex));
            return NULL;
        }
        if ((game->turn == FIRST_PLAYER_ROLE) && (str[3] == 'O')) {  // invalid
            debug("invalid move");
            sem_post(&(game->mutex));
            return NULL;
        }
        if ((game->turn == SECOND_PLAYER_ROLE) && (str[3] == 'X')) { // invalid
            debug("invalid move");
            sem_post(&(game->mutex));
            return NULL;
        }
    } else {  // invalid
        debug("invalid str");
        sem_post(&(game->mutex));
        return NULL;
    }
    // at this point: valid string
    if ((role != NULL_ROLE) && (role != game->turn)) {
        debug("invalid role");
        sem_post(&(game->mutex));
        return NULL;
    }
    GAME_MOVE* gamemove = (GAME_MOVE*)malloc(sizeof(GAME_MOVE));
    gamemove->role = role;
    gamemove->move = pos;
    sem_post(&(game->mutex));
    return gamemove;
}

char* game_unparse_move(GAME_MOVE* move) {
    char* move_str;
    if (move->role == NULL_ROLE) {
        move_str = (char*)calloc(2, sizeof(char));
        move_str[0] = (char)(move->move + '0');
        return move_str;
    } else {
        move_str = (char*)calloc(5, sizeof(char));
        move_str[0] = (char)(move->move + '0');
        move_str[1] = '<';
        move_str[2] = '-';
        if (move->role == FIRST_PLAYER_ROLE) {
            move_str[3] = 'X';
        } else {
            move_str[3] = 'O';
        }
        return move_str;
    }
}