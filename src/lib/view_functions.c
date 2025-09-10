// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "view_functions.h"
#include "library.h"
#include <stdio.h>
#include <stdlib.h>

// Variables externas definidas en view.c
extern game_state_t *game_state;
extern game_sync_t *game_sync;
extern int state_fd, sync_fd;

const char *get_player_color(int player_id) {
    switch (player_id) {
    case 0: return COLOR_PLAYER_1;
    case 1: return COLOR_PLAYER_2;
    case 2: return COLOR_PLAYER_3;
    case 3: return COLOR_PLAYER_4;
    case 4: return COLOR_PLAYER_5;
    case 5: return COLOR_PLAYER_6;
    case 6: return COLOR_PLAYER_7;
    case 7: return COLOR_PLAYER_8;
    case 8: return COLOR_PLAYER_9;
    default: return COLOR_RESET;
    }
}

void print_header(void) {
    printf("=== CHOMPCHAMPS ===\n");
    printf("Board: %d x %d | Players: %u\n\n", game_state->width, game_state->height, game_state->player_count);
}

void print_players_info(void) {
    printf("PLAYERS:\n");
    const unsigned short players_count = game_state->player_count;
    const player_t *players_array = game_state->players;

    for (unsigned short i = 0; i < players_count; i++) {
        const player_t *player = &players_array[i];
        char status = player->is_blocked ? 'X' : 'O';
        const char *color = get_player_color(i);

        printf("  %d. %s%s%s [%c] - Pos: (%2d,%2d) | Score: %3u | V:%2u I:%2u\n", i + 1, color, player->name,
               COLOR_RESET, status, player->x, player->y, player->score, player->valid_moves, player->invalid_moves);
    }
    printf("\n");
}

void print_board(void) {
    printf("BOARD:\n");
    const unsigned short width = game_state->width;
    const unsigned short height = game_state->height;
    const unsigned short players_count = game_state->player_count;

    // Imprime columna
    printf("    ");
    for (unsigned short x = 0; x < width; x++) {
        printf("%2d ", x);
    }
    printf("\n");

    // Imprime separadora
    printf("   ");
    for (unsigned short x = 0; x < width; x++) {
        printf("---");
    }
    printf("\n");

    // Imprime filas
    for (unsigned short y = 0; y < height; y++) {
        printf("%2d |", y);

        for (unsigned short x = 0; x < width; x++) {
            int *cell = get_cell(game_state, x, y);

            if (*cell > 0) {
                // Celda libre
                printf("%2d ", *cell);
            } else {
                // Celda ocupada por un jugador
                int player_id = -(*cell);
                if ((unsigned short) player_id < players_count) {
                    // Numero del jugador con su color único
                    const char *color = get_player_color(player_id);
                    printf("%sP%d%s ", color, player_id + 1, COLOR_RESET);
                } else {
                    printf("?? ");
                }
            }
        }
        printf("\n");
    }
}

void print_legend(void) {
    printf("\nLEGEND:\n");
    printf("  1-9: Available rewards\n");
    printf("  Player colors: ");
    const unsigned short players_count = game_state->player_count;
    for (unsigned short i = 0; i < players_count; i++) {
        const char *color = get_player_color(i);
        printf("%sP%d%s", color, i + 1, COLOR_RESET);
        if (i < players_count - 1) {
            printf(" ");
        }
    }
    printf("\n");
    printf("  [O]: Active player | [X]: Blocked player\n");
    printf("  V: Valid moves | I: Invalid moves\n");
}

void print_winner(void) {
    printf("\n*** GAME OVER ***\n");

    // Encontrar y mostrar el ganador
    int winner = -1;
    unsigned int max_score = 0;

    for (unsigned int i = 0; i < game_state->player_count; i++) {
        if (game_state->players[i].score > max_score) {
            max_score = game_state->players[i].score;
            winner = (int) i;
        } else if (game_state->players[i].score == max_score && winner != -1) {
            // Aplicar criterios de desempate
            if (game_state->players[i].valid_moves < game_state->players[winner].valid_moves) {
                winner = (int) i;
            } else if (game_state->players[i].valid_moves == game_state->players[winner].valid_moves) {
                if (game_state->players[i].invalid_moves < game_state->players[winner].invalid_moves) {
                    winner = (int) i;
                }
            }
        }
    }

    if (winner != -1) {
        const char *winner_color = get_player_color(winner);
        printf("¡WINNER: %s%s%s (%sP%d%s) with %u points!\n", winner_color, game_state->players[winner].name,
               COLOR_RESET, winner_color, winner + 1, COLOR_RESET, game_state->players[winner].score);
    } else {
        printf("¡DRAW!\n");
    }
}

void print_game_state(void) {
    clean_screen();
    print_header();
    print_players_info();
    print_board();
    print_legend();

    if (game_state->game_finished) {
        print_winner();
    }

    printf("\n");
    clean_buffer();
}
