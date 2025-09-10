// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#define _GNU_SOURCE
#include "master_functions.h"
#include "library.h"
#include <errno.h>
#include <time.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>



extern game_state_t *game_state;
extern game_sync_t *game_sync;
extern int state_fd, sync_fd;
extern pid_t *player_pids;
extern pid_t view_pid;
extern int *player_pipes;
extern master_config_t config;
extern bool cleanup_done;
extern bool view_active;

bool is_process_alive(pid_t pid) {
	if (pid <= 0)
		return false;
	return kill(pid, 0) == 0;
}

int sem_wait_with_timeout(sem_t *sem, int timeout_sec) {
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += timeout_sec;

	int result = sem_timedwait(sem, &timeout);
	if (result == -1 && errno == ETIMEDOUT) {
		printf("Warning: Semaphore timeout after %d seconds\n", timeout_sec);
		return -1;
	}
	return result;
}

void cleanup_view_process(void) {
	if (view_pid <= 0)
		return;

	printf("Cleaning up view process (PID: %d)\n", view_pid);

	// 1. Verificar si ya terminó
	int status;
	pid_t result = waitpid(view_pid, &status, WNOHANG);
	if (result > 0) {
		printf("View already terminated\n");
		view_pid = -1;
		return;
	}

	// 2. Terminación con SIGTERM
	if (is_process_alive(view_pid)) {
		kill(view_pid, SIGTERM);

		// Esperar con timeout
		for (int i = 0; i < 10 && is_process_alive(view_pid); i++) {
			usleep(100000); // 100ms
		}
	}

	// 3. Si aún está vivo, terminación forzada
	if (is_process_alive(view_pid)) {
		printf("View not responding to SIGTERM, using SIGKILL\n");
		kill(view_pid, SIGKILL);
		waitpid(view_pid, &status, 0);
	}
	else {
		// Recoger el proceso si terminó
		waitpid(view_pid, &status, WNOHANG);
	}

	if (WIFEXITED(status)) {
		printf("View exited (%d)\n", WEXITSTATUS(status));
	}
	else if (WIFSIGNALED(status)) {
		printf("View terminated by signal %d\n", WTERMSIG(status));
	}

	view_pid = -1;
	view_active = false;
}

void master_cleanup(void) {
	if (player_pids != NULL) {
		for (int i = 0; i < config.player_count; i++) {
			if (player_pids[i] > 0) {
				kill(player_pids[i], SIGTERM);
				waitpid(player_pids[i], NULL, 0);
			}
		}
		free(player_pids);
		player_pids = NULL;
	}

	if (player_pipes != NULL) {
		for (int i = 0; i < config.player_count; i++) {
			if (player_pipes[i] >= 0) {
				close(player_pipes[i]);
				player_pipes[i] = -1; // Marcar como cerrado
			}
		}
		free(player_pipes);
		player_pipes = NULL;
	}

	if (sync_fd >= 0) {
		close(sync_fd);
		sync_fd = -1;
	}

	if (state_fd >= 0) {
		close(state_fd);
		state_fd = -1;
	}

	if (game_state != NULL) {
		munmap(game_state, sizeof(game_state_t) + config.width * config.height * sizeof(int));
		game_state = NULL;
	}

	if (game_sync != NULL) {
		munmap(game_sync, sizeof(game_sync_t));
		game_sync = NULL;
	}

	shm_unlink(GAME_STATE_SHM);
	shm_unlink(GAME_SYNC_SHM);

	if (config.player_paths != NULL) {
		// NO liberar config.player_paths[i] porque apuntan a argv (no son malloc'd)
		// Solo liberar el array de punteros
		free(config.player_paths);
		config.player_paths = NULL;
	}

	printf("Master cleanup completed\n");
}

void parse_arguments(int argc, char *argv[], master_config_t *config) {
	// Valores por defecto
	config->width = 10;
	config->height = 10;
	config->delay = 200;
	config->timeout = 10;
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
			// Contar jugadores que siguen después de -p
			int j = i + 1;
			while (j < argc && argv[j][0] != '-') {
				config->player_count++;
				j++;
			}

			if (config->player_count == 0 || config->player_count > MAX_PLAYERS) {
				fprintf(stderr, "Error: Must specify 1-%d players\n", MAX_PLAYERS);
				exit(EXIT_FAILURE);
			}

			// Allocar memoria para paths de jugadores
			config->player_paths = malloc(config->player_count * sizeof(char *));
			if (config->player_paths == NULL) {
				fprintf(stderr, "Error: Failed to allocate memory for player paths\n");
				exit(EXIT_FAILURE);
			}
			for (int k = 0; k < config->player_count; k++) {
				config->player_paths[k] = argv[i + 1 + k];
			}

			// Avanzar el índice correctamente
			i = j - 1; // j-1 porque el loop hará i++ al final
		}
		i++;
	}

	if (config->player_count == 0) {
		fprintf(stderr, "Error: At least one player must be specified with -p\n");
		exit(EXIT_FAILURE);
	}

	// Validación de datos
	if (config->width <= 0 || config->height <= 0) {
		fprintf(stderr, "Error: Width and height must be positive\n");
		exit(EXIT_FAILURE);
	}
	if (config->delay < 0) {
		fprintf(stderr, "Error: Delay must be non-negative\n");
		exit(EXIT_FAILURE);
	}
	if (config->player_count <= 0 || config->player_count > MAX_PLAYERS) {
		fprintf(stderr, "Error: Invalid number of players (1-%d allowed)\n", MAX_PLAYERS);
		exit(EXIT_FAILURE);
	}
}




int create_shared_memories(void) {
	size_t state_size = sizeof(game_state_t) + config.width * config.height * sizeof(int);
	size_t sync_size = sizeof(game_sync_t);

	// Crear memoria compartida para el estado del juego
	state_fd = shm_open(GAME_STATE_SHM, O_CREAT | O_RDWR | O_EXCL, 0666);
	if (state_fd == -1) {
		perror("Error creating game state shared memory");
		return -1;
	}

	if (ftruncate(state_fd, state_size) == -1) {
		perror("Error setting game state size");
		close(state_fd);
		shm_unlink(GAME_STATE_SHM);
		return -1;
	}

	game_state = mmap(NULL, state_size, PROT_READ | PROT_WRITE, MAP_SHARED, state_fd, 0);
	if (game_state == MAP_FAILED) {
		perror("Error mapping game state");
		close(state_fd);
		shm_unlink(GAME_STATE_SHM);
		return -1;
	}

	// Crear memoria compartida para sincronización
	sync_fd = shm_open(GAME_SYNC_SHM, O_CREAT | O_RDWR | O_EXCL, 0666);
	if (sync_fd == -1) {
		perror("Error creating game sync shared memory");
		munmap(game_state, state_size);
		close(state_fd);
		shm_unlink(GAME_STATE_SHM);
		return -1;
	}

	if (ftruncate(sync_fd, sync_size) == -1) {
		perror("Error setting game sync size");
		munmap(game_state, state_size);
		close(state_fd);
		shm_unlink(GAME_STATE_SHM);
		close(sync_fd);
		shm_unlink(GAME_SYNC_SHM);
		return -1;
	}

	game_sync = mmap(NULL, sync_size, PROT_READ | PROT_WRITE, MAP_SHARED, sync_fd, 0);
	if (game_sync == MAP_FAILED) {
		perror("Error mapping game sync");
		munmap(game_state, state_size);
		close(state_fd);
		shm_unlink(GAME_STATE_SHM);
		close(sync_fd);
		shm_unlink(GAME_SYNC_SHM);
		return -1;
	}

	return 0;
}

void initialize_game_state(void) {
	memset(game_state, 0, sizeof(game_state_t) + config.width * config.height * sizeof(int));

	game_state->width = config.width;
	game_state->height = config.height;
	game_state->player_count = config.player_count;
	game_state->game_finished = false;

	// Verificar que player_paths esté inicializado
	if (config.player_paths == NULL) {
		fprintf(stderr, "Error: player_paths not initialized\n");
		exit(EXIT_FAILURE);
	}

	srand(config.seed);

	// Inicializar tablero con recompensas aleatorias (1-9)
	for (int i = 0; i < config.width * config.height; i++) {
		game_state->board[i] = (rand() % 9) + 1;
	}

	// Posicionar jugadores de forma determinística
	for (int i = 0; i < config.player_count; i++) {
		// Extraer el nombre del archivo del path completo
		char *player_name = strrchr(config.player_paths[i], '/');
		if (player_name) {
			player_name++; // Saltar el '/'
		}
		else {
			player_name = config.player_paths[i]; // Si no hay '/', usar todo el path
		}

		snprintf(game_state->players[i].name, MAX_NAME_LEN, "%s", player_name);
		game_state->players[i].score = 0;
		game_state->players[i].valid_moves = 0;
		game_state->players[i].invalid_moves = 0;
		game_state->players[i].is_blocked = false;
		game_state->players[i].pid = 0;

        // TODO: REDUCIR CANTIDAD DE CODIGO
		// Distribución simple: esquinas y bordes
		switch (i) {
			case 0:
				game_state->players[i].x = 0;
				game_state->players[i].y = 0;
				break;
			case 1:
				game_state->players[i].x = config.width - 1;
				game_state->players[i].y = 0;
				break;
			case 2:
				game_state->players[i].x = 0;
				game_state->players[i].y = config.height - 1;
				break;
			case 3:
				game_state->players[i].x = config.width - 1;
				game_state->players[i].y = config.height - 1;
				break;
			default: {
				int side = (i - 4) % 4;
				int pos = (i - 4) / 4 + 1;
				switch (side) {
					case 0:
						game_state->players[i].x = pos * config.width / (config.player_count - 3);
						game_state->players[i].y = 0;
						break;
					case 1:
						game_state->players[i].x = config.width - 1;
						game_state->players[i].y = pos * config.height / (config.player_count - 3);
						break;
					case 2:
						game_state->players[i].x = config.width - 1 - pos * config.width / (config.player_count - 3);
						game_state->players[i].y = config.height - 1;
						break;
					case 3:
						game_state->players[i].x = 0;
						game_state->players[i].y = config.height - 1 - pos * config.height / (config.player_count - 3);
						break;
				}
				break;
			}
		}

		// Marcar celda como ocupada
		int *cell = get_cell(game_state, game_state->players[i].x, game_state->players[i].y);
		*cell = -(i);
		printf("DEBUG: Player %d at (%d,%d), cell value = %d\n", i, game_state->players[i].x, game_state->players[i].y,
			   *cell);
	}
}

void initialize_synchronization(void) {
	sem_init(&game_sync->view_ready, 1, 0);
	sem_init(&game_sync->view_done, 1, 0);
	sem_init(&game_sync->reader_writer_mutex, 1, 1);
	sem_init(&game_sync->state_mutex, 1, 1);
	sem_init(&game_sync->reader_count_mutex, 1, 1);
	game_sync->reader_count = 0;

	for (int i = 0; i < MAX_PLAYERS; i++) {
		sem_init(&game_sync->player_turn[i], 1, 0);
	}

	for (int i = 0; i < config.player_count; i++) {
		sem_post(&game_sync->player_turn[i]);
	}
}

int create_processes(void) {
	player_pids = calloc(config.player_count, sizeof(pid_t));
	player_pipes = malloc(config.player_count * sizeof(int));

	char width_str[16], height_str[16];
	snprintf(width_str, sizeof(width_str), "%d", config.width);
	snprintf(height_str, sizeof(height_str), "%d", config.height);

	// Crear procesos de jugadores
	for (int i = 0; i < config.player_count; i++) {
		int pipefd[2];
		if (pipe(pipefd) == -1) {
			perror("Error creating pipe for player");
			return -1;
		}

		pid_t pid = fork();
		if (pid == -1) {
			perror("Error forking player process");
			return -1;
		}
		else if (pid == 0) {
			// Proceso hijo (jugador)
			close(pipefd[0]);

			if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
				perror("Error redirecting stdout in player");
				exit(EXIT_FAILURE);
			}
			close(pipefd[1]);

			execl(config.player_paths[i], config.player_paths[i], width_str, height_str, NULL);
			perror("Error executing player program");
			exit(EXIT_FAILURE);
		}
		else {
			close(pipefd[1]);
			player_pids[i] = pid;
			player_pipes[i] = pipefd[0];
			game_state->players[i].pid = pid;
		}
	}

	// Crear proceso de vista si se especificó
	if (config.view_path != NULL) {
		view_pid = fork();
		if (view_pid == -1) {
			perror("Error forking view process");
			printf("Warning: Continuing without view\n");
			config.view_path = NULL;
			view_active = false;
			return 0;
		}
		else if (view_pid == 0) {
			execl(config.view_path, config.view_path, width_str, height_str, NULL);
			perror("Error executing view program");
			exit(EXIT_FAILURE);
		}
		else {
			printf("View process created (PID: %d)\n", view_pid);
			view_active = true;
		}
	}

	return 0;
}