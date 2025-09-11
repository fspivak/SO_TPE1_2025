// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "config_management.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
