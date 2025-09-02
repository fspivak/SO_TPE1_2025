#include "library.h"
#include "common.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

void generic_signal_handler(int sig, const char *process_name, int process_id, void (*cleanup_fn)(void)) {
    if (process_id >= 0) {
        fprintf(stderr, "%s %d received signal %d, cleaning up...\n", process_name, process_id, sig);
    } else {
        fprintf(stderr, "%s received signal %d, cleaning up...\n", process_name, sig);
    }

    if (cleanup_fn != NULL) {
        cleanup_fn();
    }

    exit(EXIT_FAILURE);
}

void clean_screen(void) {
    printf("\033c"); // Clear terminal -> (Unix/Linux)
}

void clean_buffer(void) {
    fflush(stdout); // limpia el buffer de salida por si quedo algo
}

void check_params(int argc, char *argv[]) {
    if (argc != 3 || (atoi(argv[1]) <= 0 || atoi(argv[2]) <= 0)) {
        fprintf(stderr, "Error with params");
        exit(EXIT_FAILURE);
    }
}

void close_up(int *sync_fd, int *state_fd, game_state_t **game_state, game_sync_t **game_sync) {

    if (*game_state != NULL && *game_state != MAP_FAILED) {
        munmap(*game_state, sizeof(game_state_t) + (*game_state)->width * (*game_state)->height * sizeof(int));
    }
    if (*game_sync != NULL && *game_sync != MAP_FAILED) {
        munmap(*game_sync, sizeof(game_sync_t));
    }

    if (*state_fd != -1) close(*state_fd);
    if (*sync_fd != -1) close(*sync_fd);

    // Limpia punteros evitando Segmentation Fault
    *game_state = NULL;
    *game_sync = NULL;
}

int connect_shared_memories(int game_state_size, int game_sync_size, int *sync_fd, int *state_fd,
                            game_state_t **game_state, game_sync_t **game_sync) {
    // open the shared memory for game state
    *state_fd = shm_open(GAME_STATE_SHM, O_RDONLY, 0);
    if (*state_fd == -1) {
        perror("Error oppening shared memory (state)");
        return -1;
    }

    // Mapping the shared memory
    *game_state = mmap(NULL, game_state_size, PROT_READ, MAP_SHARED, *state_fd, 0);

    if (*game_state == MAP_FAILED) {
        perror("Error mapping the shared memory (state)");
        return -1;
    }

    *sync_fd = shm_open(GAME_SYNC_SHM, O_RDWR, 0);
    if (*sync_fd == -1) {
        perror("Error oppening shared memory (sync)");
        return -1;
    }

    *game_sync = mmap(NULL, game_sync_size, PROT_WRITE | PROT_READ, MAP_SHARED, *sync_fd, 0);

    if (*game_sync == MAP_FAILED) {
        perror("Error mapping the shared memory (sync)");
        return -1;
    }

    return 0;
}
