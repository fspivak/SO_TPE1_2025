// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#define _GNU_SOURCE
#include "lib/common.h"
#include "lib/library.h"
#include "lib/player_functions.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// ESTRUCTURA DE LA CATEDRA
// while(1){
//     wait(writer)
//     post(writer)

//     wait(readers_count_mutex);
//     if(readers_count_mutex++ == 0) wait(mutex);
//     post(readers_count_mutex);

//     pedir_movimiento(...)
//     wait(mutex)
//     enviar_movimiento(...)
//     post(mutex)
// }

game_state_t *game_state = NULL;
game_sync_t *game_sync = NULL;
int state_fd = -1, sync_fd = -1, player_id = -1;

// Close up wrapper
void player_close_up(void) { close_up(&sync_fd, &state_fd, &game_state, &game_sync); }

// Signal wrapper
void player_signal_wrapper(int sig) { generic_signal_handler(sig, "Player", player_id + 1, player_close_up); }

int main(int argc, char *argv[]) {
    atexit(player_close_up);

    signal(SIGINT, player_signal_wrapper);  // Ctrl+C
    signal(SIGTERM, player_signal_wrapper); // kill

    check_params(argc, argv);

    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    size_t game_state_size = sizeof(game_state_t) + width * height * sizeof(int);
    size_t game_sync_size = sizeof(game_sync_t);

    // Generador de números aleatorios
    srand(time(NULL) + getpid());

    if (connect_shared_memories(game_state_size, game_sync_size, &sync_fd, &state_fd, &game_state, &game_sync) != 0) {
        fprintf(stderr, "Error to initialize shared memory player");
        exit(EXIT_FAILURE); // atexit se encargará del cleanup
    }

    // Encontrar ID de jugador
    enter_read_state();
    player_id = find_my_player_id();
    exit_read_state();

    if (player_id == -1) {
        fprintf(stderr, "Error: Could not find player ID\n");
        exit(EXIT_FAILURE); // atexit se encargará del cleanup
    }

    fprintf(stderr, "Player %d started (PID: %d)\n", player_id + 1, getpid());

    // Loop principal del jugador
    while (true) {
        // Esperar mi turno
        sem_wait(&game_sync->player_turn[player_id]);

        // Leer el estado del juego de forma sincronizada
        enter_read_state();

        // Verificar si el juego terminó o estoy bloqueado
        if (game_state->game_finished || game_state->players[player_id].is_blocked) {
            exit_read_state();
            break;
        }

        // Elegir movimiento usando estrategia
        direction_t chosen_move = choose_strategic_move();
        exit_read_state();

        // Enviar movimiento al master
        send_move(chosen_move);
    }

    fprintf(stderr, "Player %d finished...\n", player_id + 1);
    return EXIT_SUCCESS; // atexit se encargará del cleanup
}