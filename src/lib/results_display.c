// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#define _GNU_SOURCE
#include "results_display.h"
#include "common.h"
#include "library.h"
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

void print_final_results(master_context_t *ctx) {
	printf("\n=== FINAL RESULTS ===\n");

	// Ordenar jugadores por puntuacion
	int sorted_indices[MAX_PLAYERS];
	for (int i = 0; i < ctx->config.player_count; i++) {
		sorted_indices[i] = i;
	}

	// Buble sort
	for (int i = 0; i < ctx->config.player_count - 1; i++) {
		for (int j = 0; j < ctx->config.player_count - i - 1; j++) {
			if (ctx->game_state->players[sorted_indices[j]].score <
					ctx->game_state->players[sorted_indices[j + 1]].score ||
				(ctx->game_state->players[sorted_indices[j]].score ==
					 ctx->game_state->players[sorted_indices[j + 1]].score &&
				 ctx->game_state->players[sorted_indices[j]].invalid_moves >
					 ctx->game_state->players[sorted_indices[j + 1]].invalid_moves)) {
				int temp = sorted_indices[j];
				sorted_indices[j] = sorted_indices[j + 1];
				sorted_indices[j + 1] = temp;
			}
		}
	}

	// Imprimir resultados ordenados
	for (int i = 0; i < ctx->config.player_count; i++) {
		int idx = sorted_indices[i];

		// Estado de salida del procesos
		int exit_status = 0;
		if (ctx->player_pids[idx] > 0) {
			int status;
			pid_t result = waitpid(ctx->player_pids[idx], &status, WNOHANG);
			if (result > 0) {
				if (WIFEXITED(status)) {
					exit_status = WEXITSTATUS(status);
				}
				else if (WIFSIGNALED(status)) {
					exit_status = WTERMSIG(status);
				}
			}
		}

		printf("%d. %s (%sP%d\033[0m) (%d): %u points (%u V, %u I)\n", i + 1, ctx->game_state->players[idx].name,
			   get_player_color(idx), idx + 1, exit_status, ctx->game_state->players[idx].score,
			   ctx->game_state->players[idx].valid_moves, ctx->game_state->players[idx].invalid_moves);
	}

	printf("=====================\n");
}

void display_game_parameters(const master_config_t *config) {
	printf("\n");
	printf("========================================\n");
	printf("  CHOMPCHAMPS - PARAMETERS OF THE GAME\n");
	printf("========================================\n");
	printf("Table: %dx%d\n", config->width, config->height);
	printf("Delay: %dms\n", config->delay);
	printf("Timeout: %ds\n", config->timeout);
	printf("Seed: %u\n", config->seed);
	printf("Players: %d\n", config->player_count);
}

void display_processes_info(const master_config_t *config, const pid_t *player_pids, pid_t view_pid, bool view_active,
							pid_t master_pid) {
	// Informacion de procesos creados
	for (int i = 0; i < config->player_count; i++) {
		printf("   %d. %s (PID: %d)\n", i + 1, config->player_paths[i], player_pids[i]);
	}

	if (view_active && config->view_path) {
		printf("View: %s (PID: %d)\n", config->view_path, view_pid);
	}
	else {
		printf("View: No view\n");
	}

	printf("Master: (PID: %d)\n", master_pid);
}

void display_game_start(void) {
	printf("========================================\n");
	printf("            STARTING GAME...\n");
	printf("========================================\n\n");

	usleep(START_SLEEP_SEC * 1000000); // para que el usuario pueda leer la informacion
}
