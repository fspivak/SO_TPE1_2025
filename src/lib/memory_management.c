// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "memory_management.h"
#include "common.h"
#include "library.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int create_shared_memories(master_context_t *ctx) {
	size_t state_size = sizeof(game_state_t) + ctx->config.width * ctx->config.height * sizeof(int);
	size_t sync_size = sizeof(game_sync_t);

	// Crear memoria compartida para el estado del juego
	ctx->state_fd = shm_open(GAME_STATE_SHM, O_CREAT | O_RDWR | O_EXCL, 0666);
	if (ctx->state_fd == -1) {
		perror("Error creating game state shared memory");
		return -1;
	}

	if (ftruncate(ctx->state_fd, state_size) == -1) {
		perror("Error setting game state size");
		close(ctx->state_fd);
		shm_unlink(GAME_STATE_SHM);
		return -1;
	}

	ctx->game_state = mmap(NULL, state_size, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->state_fd, 0);
	if (ctx->game_state == MAP_FAILED) {
		perror("Error mapping game state");
		close(ctx->state_fd);
		shm_unlink(GAME_STATE_SHM);
		return -1;
	}

	// Crear memoria compartida para sincronizacion
	ctx->sync_fd = shm_open(GAME_SYNC_SHM, O_CREAT | O_RDWR | O_EXCL, 0666);
	if (ctx->sync_fd == -1) {
		perror("Error creating game sync shared memory");
		munmap(ctx->game_state, state_size);
		close(ctx->state_fd);
		shm_unlink(GAME_STATE_SHM);
		return -1;
	}

	if (ftruncate(ctx->sync_fd, sync_size) == -1) {
		perror("Error setting game sync size");
		munmap(ctx->game_state, state_size);
		close(ctx->state_fd);
		shm_unlink(GAME_STATE_SHM);
		close(ctx->sync_fd);
		shm_unlink(GAME_SYNC_SHM);
		return -1;
	}

	ctx->game_sync = mmap(NULL, sync_size, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->sync_fd, 0);
	if (ctx->game_sync == MAP_FAILED) {
		perror("Error mapping game sync");
		munmap(ctx->game_state, state_size);
		close(ctx->state_fd);
		shm_unlink(GAME_STATE_SHM);
		close(ctx->sync_fd);
		shm_unlink(GAME_SYNC_SHM);
		return -1;
	}

	return 0;
}

void initialize_game_state(master_context_t *ctx) {
	memset(ctx->game_state, 0, sizeof(game_state_t) + ctx->config.width * ctx->config.height * sizeof(int));

	ctx->game_state->width = ctx->config.width;
	ctx->game_state->height = ctx->config.height;
	ctx->game_state->player_count = ctx->config.player_count;
	ctx->game_state->game_finished = false;

	if (ctx->config.player_paths == NULL) {
		fprintf(stderr, "Error: player_paths not initialized\n");
		exit(EXIT_FAILURE);
	}

	srand(ctx->config.seed);

	// Inicializar tablero con recompensas aleatorias (1-9)
	for (int i = 0; i < ctx->config.width * ctx->config.height; i++) {
		ctx->game_state->board[i] = (rand() % 9) + 1;
	}

	// Posicionar jugadores en el tablero
	for (int i = 0; i < ctx->config.player_count; i++) {
		char *player_name = strrchr(ctx->config.player_paths[i], '/');
		if (player_name) {
			player_name++;
		}
		else {
			player_name = ctx->config.player_paths[i];
		}

		snprintf(ctx->game_state->players[i].name, MAX_NAME_LEN, "%s", player_name);
		ctx->game_state->players[i].score = 0;
		ctx->game_state->players[i].valid_moves = 0;
		ctx->game_state->players[i].invalid_moves = 0;
		ctx->game_state->players[i].is_blocked = false;
		ctx->game_state->players[i].pid = 0;

		position_player_at_start(ctx, i);

		// Marcar celda como ocupada
		int *cell = get_cell(ctx->game_state, ctx->game_state->players[i].x, ctx->game_state->players[i].y);
		*cell = -(i);
	}
}

void initialize_synchronization(master_context_t *ctx) {
	if (sem_init(&ctx->game_sync->view_ready, 1, 0) == -1) {
		perror("Error initializing view_ready semaphore");
		exit(EXIT_FAILURE);
	}
	if (sem_init(&ctx->game_sync->view_done, 1, 0) == -1) {
		perror("Error initializing view_done semaphore");
		exit(EXIT_FAILURE);
	}
	if (sem_init(&ctx->game_sync->reader_writer_mutex, 1, 1) == -1) {
		perror("Error initializing reader_writer_mutex semaphore");
		exit(EXIT_FAILURE);
	}
	if (sem_init(&ctx->game_sync->state_mutex, 1, 1) == -1) {
		perror("Error initializing state_mutex semaphore");
		exit(EXIT_FAILURE);
	}
	if (sem_init(&ctx->game_sync->reader_count_mutex, 1, 1) == -1) {
		perror("Error initializing reader_count_mutex semaphore");
		exit(EXIT_FAILURE);
	}
	ctx->game_sync->reader_count = 0;

	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (sem_init(&ctx->game_sync->player_turn[i], 1, 0) == -1) {
			perror("Error initializing player_turn semaphore");
			exit(EXIT_FAILURE);
		}
	}
}

void position_player_at_start(master_context_t *ctx, int player_id) {
	// Distribucion simple: esquinas y bordes
	if (player_id < 4) {
		// Primeros 4 jugadores en las esquinas
		switch (player_id) {
			case 0:
				ctx->game_state->players[player_id].x = 0;
				ctx->game_state->players[player_id].y = 0;
				break;
			case 1:
				ctx->game_state->players[player_id].x = ctx->config.width - 1;
				ctx->game_state->players[player_id].y = 0;
				break;
			case 2:
				ctx->game_state->players[player_id].x = 0;
				ctx->game_state->players[player_id].y = ctx->config.height - 1;
				break;
			case 3:
				ctx->game_state->players[player_id].x = ctx->config.width - 1;
				ctx->game_state->players[player_id].y = ctx->config.height - 1;
				break;
		}
	}
	else {
		// Jugadores adicionales en los bordes
		int side = (player_id - 4) % 4;
		int pos = (player_id - 4) / 4 + 1;

		switch (side) {
			case 0: // Borde superior
				ctx->game_state->players[player_id].x = pos * ctx->config.width / (ctx->config.player_count - 3);
				ctx->game_state->players[player_id].y = 0;
				break;
			case 1: // Borde derecho
				ctx->game_state->players[player_id].x = ctx->config.width - 1;
				ctx->game_state->players[player_id].y = pos * ctx->config.height / (ctx->config.player_count - 3);
				break;
			case 2: // Borde inferior
				ctx->game_state->players[player_id].x =
					ctx->config.width - 1 - pos * ctx->config.width / (ctx->config.player_count - 3);
				ctx->game_state->players[player_id].y = ctx->config.height - 1;
				break;
			case 3: // Borde izquierdo
				ctx->game_state->players[player_id].x = 0;
				ctx->game_state->players[player_id].y =
					ctx->config.height - 1 - pos * ctx->config.height / (ctx->config.player_count - 3);
				break;
		}
	}
}
