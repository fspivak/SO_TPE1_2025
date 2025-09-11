// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "library.h"
#include "common.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int *get_cell(game_state_t *state, int x, int y) {
	return &state->board[y * state->width + x];
}

void get_direction_offset(direction_t dir, int *dx, int *dy) {
	static const int offsets[][2] = {
		{0, -1}, // UP
		{1, -1}, // UP_RIGHT
		{1, 0},	 // RIGHT
		{1, 1},	 // DOWN_RIGHT
		{0, 1},	 // DOWN
		{-1, 1}, // DOWN_LEFT
		{-1, 0}, // LEFT
		{-1, -1} // UP_LEFT
	};
	*dx = offsets[dir][0];
	*dy = offsets[dir][1];
}

const char *get_player_color(int player_id) {
	static const char *player_colors[MAX_PLAYER_COLORS] = {COLOR_PLAYER_1, COLOR_PLAYER_2, COLOR_PLAYER_3,
														   COLOR_PLAYER_4, COLOR_PLAYER_5, COLOR_PLAYER_6,
														   COLOR_PLAYER_7, COLOR_PLAYER_8, COLOR_PLAYER_9};

	if (player_id >= 0 && player_id < MAX_PLAYER_COLORS) {
		return player_colors[player_id];
	}
	return COLOR_RESET;
}

// Funcion utilitaria para calcular tamaños de memoria compartida
size_t calculate_game_state_size(int width, int height) {
	return sizeof(game_state_t) + width * height * sizeof(int);
}

size_t calculate_game_sync_size(void) {
	return sizeof(game_sync_t);
}

// Funcion utilitaria para inicializacion estandar de señales
void setup_standard_signals(void (*signal_handler)(int)) {
	signal(SIGINT, signal_handler);	 // Ctrl+C
	signal(SIGTERM, signal_handler); // kill
}

void generic_signal_handler(int sig, const char *process_name, int process_id, void (*cleanup_fn)(void)) {
	if (process_id >= 0) {
		fprintf(stderr, "%s %d received signal %d, cleaning up...\n", process_name, process_id, sig);
	}
	else {
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

	if (*state_fd != -1)
		close(*state_fd);
	if (*sync_fd != -1)
		close(*sync_fd);

	// Limpia punteros evitando Segmentation Fault
	*game_state = NULL;
	*game_sync = NULL;
}

int connect_shared_memories(int game_state_size, int game_sync_size, int *sync_fd, int *state_fd,
							game_state_t **game_state, game_sync_t **game_sync) {
	// Abrir la memoria compartida para el estado del juego
	*state_fd = shm_open(GAME_STATE_SHM, O_RDONLY, 0);
	if (*state_fd == -1) {
		perror("Error opening shared memory (state)");
		return -1;
	}

	// Mapear la memoria compartida
	*game_state = mmap(NULL, game_state_size, PROT_READ, MAP_SHARED, *state_fd, 0);
	if (*game_state == MAP_FAILED) {
		perror("Error mapping shared memory (state)");
		close(*state_fd);
		return -1;
	}

	// Abrir la memoria compartida para sincronizacion
	*sync_fd = shm_open(GAME_SYNC_SHM, O_RDWR, 0);
	if (*sync_fd == -1) {
		perror("Error opening shared memory (sync)");
		munmap(*game_state, game_state_size);
		close(*state_fd);
		return -1;
	}

	// Mapear la memoria compartida
	*game_sync = mmap(NULL, game_sync_size, PROT_WRITE | PROT_READ, MAP_SHARED, *sync_fd, 0);
	if (*game_sync == MAP_FAILED) {
		perror("Error mapping shared memory (sync)");
		munmap(*game_state, game_state_size);
		close(*state_fd);
		close(*sync_fd);
		return -1;
	}

	return 0;
}

bool is_valid_move(int player_id, direction_t direction, game_state_t *game_state) {
	player_t *player = &game_state->players[player_id];
	int dx, dy;

	// Futura posicion del player
	get_direction_offset(direction, &dx, &dy);
	int new_x = player->x + dx;
	int new_y = player->y + dy;

	// Validacion de que la nueva posicion este dentro del tablero
	if (new_x < 0 || new_y < 0 || new_x >= game_state->width || new_y >= game_state->height) {
		return false;
	}

	// Validar que no este ocupado
	int *cell = get_cell(game_state, new_x, new_y);
	return (*cell > 0);
}