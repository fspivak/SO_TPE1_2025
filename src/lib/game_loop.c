// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#define _GNU_SOURCE
#include "game_loop.h"
#include "common.h"
#include "game_logic.h"
#include "library.h"
#include <errno.h>
#include <stdio.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

// Funcion auxiliar para configurar file descriptors
static void setup_file_descriptors(master_context_t *ctx, fd_set *readfds, int *max_fd) {
	FD_ZERO(readfds);
	*max_fd = 0;

	for (int i = 0; i < ctx->config.player_count; i++) {
		if (!ctx->game_state->players[i].is_blocked && ctx->player_pipes[i] != -1) {
			FD_SET(ctx->player_pipes[i], readfds);
			if (ctx->player_pipes[i] > *max_fd) {
				*max_fd = ctx->player_pipes[i];
			}
		}
	}
}

// Funcion auxiliar para procesar movimientos de jugadores
static bool process_player_moves(master_context_t *ctx, fd_set *readfds, int *current_player, time_t *last_valid_move) {
	bool movement_processed = false;

	for (int attempts = 0; attempts < ctx->config.player_count && !movement_processed; attempts++) {
		int player_id = (*current_player + attempts) % ctx->config.player_count;

		if (ctx->game_state->players[player_id].is_blocked || !FD_ISSET(ctx->player_pipes[player_id], readfds)) {
			continue;
		}

		unsigned char move;
		ssize_t bytes_read = read(ctx->player_pipes[player_id], &move, 1);

		if (bytes_read <= 0) {
			ctx->game_state->players[player_id].is_blocked = true;
			if (ctx->player_pipes[player_id] != -1) {
				close(ctx->player_pipes[player_id]);
				ctx->player_pipes[player_id] = -1;
			}
			continue;
		}

		sem_wait(&ctx->game_sync->reader_writer_mutex);
		sem_wait(&ctx->game_sync->state_mutex);
		sem_post(&ctx->game_sync->reader_writer_mutex);

		if (is_valid_move(player_id, move, ctx->game_state)) {
			execute_player_move(ctx, player_id, move);
			*last_valid_move = time(NULL);
		}
		else {
			ctx->game_state->players[player_id].invalid_moves++;
		}

		sem_post(&ctx->game_sync->state_mutex);
		sem_post(&ctx->game_sync->player_turn[player_id]);

		movement_processed = true;
		*current_player = (player_id + 1) % ctx->config.player_count;
	}

	return movement_processed;
}

// Funcion auxiliar para verificar timeout
static bool check_timeout(master_context_t *ctx, time_t last_valid_move) {
	return difftime(time(NULL), last_valid_move) >= ctx->config.timeout;
}

// Funcion auxiliar para notificar a todos los jugadores
static void notify_all_players(master_context_t *ctx) {
	for (int i = 0; i < ctx->config.player_count; i++) {
		sem_post(&ctx->game_sync->player_turn[i]);
	}
}

// Funcion auxiliar para manejar fin de juego
static void handle_game_end(master_context_t *ctx) {
	// Game ended - no more valid moves available
	ctx->game_state->game_finished = true;
	notify_all_players(ctx);
}

// Funcion auxiliar para sincronizar con view si es necesario
static void sync_with_view_if_needed(master_context_t *ctx, bool movement_processed) {
	if (movement_processed) {
		sync_with_view(ctx);
		if (ctx->config.delay > 0) {
			usleep(ctx->config.delay * 1000);
		}
	}
}

void game_loop(master_context_t *ctx) {
	fd_set readfds;
	struct timeval timeout_tv;
	time_t last_valid_move = time(NULL);
	int current_player = 0;

	// Starting game loop

	// Sincronizacion inicial con view
	if (ctx->view_active && ctx->config.view_path != NULL) {
		// Initial sync with view
		sync_with_view(ctx);
	}

	while (!ctx->game_state->game_finished) {
		int max_fd;
		setup_file_descriptors(ctx, &readfds, &max_fd);

		timeout_tv.tv_sec = 1;
		timeout_tv.tv_usec = 0;

		int ready = select(max_fd + 1, &readfds, NULL, NULL, &timeout_tv);

		if (ready == -1) {
			if (errno == EINTR)
				continue;
			perror("Select error");
			break;
		}

		if (ready == 0) {
			if (check_timeout(ctx, last_valid_move)) {
				// Timeout, finalizando programa
				break;
			}
			continue;
		}

		bool movement_processed = process_player_moves(ctx, &readfds, &current_player, &last_valid_move);

		// Verificar fin de juego despues de procesar movimientos
		if (check_game_end(ctx)) {
			handle_game_end(ctx);
			break;
		}

		// Sincronizar con view si hubo movimientos
		sync_with_view_if_needed(ctx, movement_processed);

		// Timeout global del juego
		if (time(NULL) - last_valid_move > ctx->config.timeout) {
			// Game timeout reached
			ctx->game_state->game_finished = true;
			notify_all_players(ctx);
			break;
		}
	}

	// Notificacion final a view
	if (ctx->view_active && ctx->config.view_path != NULL) {
		// Final sync with view
		sync_with_view(ctx);
	}
}
