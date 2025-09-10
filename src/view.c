// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "lib/common.h"
#include "lib/library.h"
#include "lib/view_functions.h"
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

game_state_t *game_state = NULL;
game_sync_t *game_sync = NULL;
int state_fd = -1, sync_fd = -1;

// wrapper
void view_close_up(void) { close_up(&sync_fd, &state_fd, &game_state, &game_sync); }

// wrapper
void view_signal_wrapper(int sig) { generic_signal_handler(sig, "View", -1, view_close_up); }

int main(int argc, char *argv[]) {
    atexit(view_close_up);

    signal(SIGINT, view_signal_wrapper);  // Ctrl+C
    signal(SIGTERM, view_signal_wrapper); // kill

    check_params(argc, argv);

    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    size_t game_state_size = sizeof(game_state_t) + width * height * sizeof(int);
    size_t game_sync_size = sizeof(game_sync_t);

    if (connect_shared_memories(game_state_size, game_sync_size, &sync_fd, &state_fd, &game_state, &game_sync)) {
        fprintf(stderr, "Error to initialize shared memory view");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Esperar señal del master con manejo de errores
        if (sem_wait(&game_sync->view_ready) != 0) {
            perror("Error receiving signal from Master");
            break;
        }

        // Imprimir estado del juego
        print_game_state();

        // Notificar al master que terminamos de imprimir
        if (sem_post(&game_sync->view_done) != 0) {
            perror("Error sending signal to Master");
            break;
        }

        // Si el juego terminó, salir después de notificar
        if (game_state->game_finished) {
            printf("View: Game over, closing...\n");
            break;
        }
    }

    printf("View finishing... \n");
    return EXIT_SUCCESS; // atexit se encargará del cleanup
}