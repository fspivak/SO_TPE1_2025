// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#define _GNU_SOURCE
#include "game_logic.h"
#include "common.h"
#include "library.h"
#include "process_management.h"
#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <time.h>

int sem_wait_with_timeout(sem_t *sem, int timeout_sec) {
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += timeout_sec;

	int result = sem_timedwait(sem, &timeout);
	if (result == -1 && errno == ETIMEDOUT) {
		return -1;
	}
	return result;
}

void execute_player_move(master_context_t *ctx, int player_id, unsigned char direction) {
	player_t *player = &ctx->game_state->players[player_id];
	int dx, dy;
	get_direction_offset((direction_t) direction, &dx, &dy);

	int new_x = player->x + dx;
	int new_y = player->y + dy;

	int *cell = get_cell(ctx->game_state, new_x, new_y);
	int reward = *cell;

	player->x = new_x;
	player->y = new_y;
	player->score += reward;
	player->valid_moves++;

	*cell = -(player_id);
}

bool check_game_end(master_context_t *ctx) {
	bool any_active = false;

	for (int player_id = 0; player_id < ctx->config.player_count; player_id++) {
		if (ctx->game_state->players[player_id].is_blocked)
			continue;

		bool has_valid_moves = false;
		for (unsigned char dir = 0; dir < 8; dir++) {
			if (is_valid_move(player_id, dir, ctx->game_state)) {
				has_valid_moves = true;
				any_active = true;
				break;
			}
		}

		if (!has_valid_moves && !ctx->game_state->players[player_id].is_blocked) {
			// Marcamos el jugador como bloqueado y esperamos que el jugador lo maneje
			ctx->game_state->players[player_id].is_blocked = true;
		}
	}

	return !any_active;
}

void sync_with_view(master_context_t *ctx) {
	if (!ctx->view_active || ctx->config.view_path == NULL)
		return;

	// Verificar si view sigue viva
	if (!is_process_alive(ctx->view_pid)) {
		ctx->view_active = false;
		return;
	}

	// Notificar a view
	if (sem_post(&ctx->game_sync->view_ready) == -1) {
		perror("Error signaling view");
		ctx->view_active = false;
		return;
	}

	// Esperar respuesta con timeout
	if (sem_wait_with_timeout(&ctx->game_sync->view_done, VIEW_TIMEOUT_SEC) == -1) {
		ctx->view_active = false;
		return;
	}
}
