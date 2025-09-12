// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#define _GNU_SOURCE
#include "config_management.h"
#include "common.h"
#include "library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/wait.h>
#include <unistd.h>

void parse_arguments(int argc, char *argv[], master_config_t *config) {
	// Valores por default
	config->width = DEFAULT_BOARD_WIDTH;
	config->height = DEFAULT_BOARD_HEIGHT;
	config->delay = DEFAULT_DELAY_MS;
	config->timeout = DEFAULT_TIMEOUT_SEC;
	config->seed = time(NULL);
	config->view_path = NULL;
	config->player_paths = NULL;
	config->player_count = 0;

	int i = 1;
	while (i < argc) {
		if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
			config->width = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
			config->height = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
			config->delay = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
			config->timeout = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
			config->seed = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) {
			config->view_path = argv[++i];
		}
		else if (strcmp(argv[i], "-p") == 0) {
			int j = i + 1;
			while (j < argc && argv[j][0] != '-') {
				config->player_count++;
				j++;
			}

			if (config->player_count == 0 || config->player_count > MAX_PLAYERS) {
				fprintf(stderr, "Error: Must specify 1-%d players\n", MAX_PLAYERS);
				exit(EXIT_FAILURE);
			}

			config->player_paths = malloc(config->player_count * sizeof(char *));
			if (config->player_paths == NULL) {
				fprintf(stderr, "Error: Failed to allocate memory for player paths\n");
				exit(EXIT_FAILURE);
			}

			for (int k = 0; k < config->player_count; k++) {
				config->player_paths[k] = argv[i + 1 + k];
			}

			i = j - 1; // Avanzar el indice correctamente
		}
		i++;
	}

	// Validacion de datos
	if (config->player_count == 0) {
		fprintf(stderr, "Error: At least one player must be specified with -p\n");
		exit(EXIT_FAILURE);
	}
	if (config->width < 10 || config->height < 10) {
		fprintf(stderr, "Error: Width and height must be at least 10\n");
		exit(EXIT_FAILURE);
	}
	if (config->delay < 0) {
		fprintf(stderr, "Error: Delay must be non-negative\n");
		exit(EXIT_FAILURE);
	}
	if (config->timeout < 10) {
		fprintf(stderr, "Error: Timeout must be at least 10 second\n");
		exit(EXIT_FAILURE);
	}
	if (config->player_count <= 0 || config->player_count > MAX_PLAYERS) {
		fprintf(stderr, "Error: Invalid number of players (1-%d allowed)\n", MAX_PLAYERS);
		exit(EXIT_FAILURE);
	}
}

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

void display_processes_info(const master_config_t *config, const pid_t *player_pids, pid_t view_pid, bool view_active) {
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
}

void display_game_start(void) {
	printf("========================================\n");
	printf("            STARTING GAME...\n");
	printf("========================================\n\n");

	usleep(START_SLEEP_SEC * 1000000); // para que el usuario pueda leer la informacion
}
