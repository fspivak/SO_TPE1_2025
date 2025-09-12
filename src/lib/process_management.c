// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#define _GNU_SOURCE
#include "process_management.h"
#include "common.h"
#include "library.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

bool is_process_alive(pid_t pid) {
	if (pid <= 0)
		return false;
	return kill(pid, 0) == 0;
}

void cleanup_view_process(master_context_t *ctx) {
	if (ctx->view_pid <= 0)
		return;

	int status;
	pid_t result;

	// 1. Verificar si ya termino
	result = waitpid(ctx->view_pid, &status, WNOHANG);
	if (result > 0) { // View termino
		ctx->view_pid = -1;
		ctx->view_active = false;
		return;
	}

	// 2. Verificar si el proceso aun existe
	if (!is_process_alive(ctx->view_pid)) {
		ctx->view_pid = -1;
		ctx->view_active = false;
		return;
	}

	// 3. Terminacion con SIGTERM
	if (kill(ctx->view_pid, SIGTERM) == -1) {
		ctx->view_pid = -1;
		ctx->view_active = false;
		return;
	}

	// 4. Esperar terminacion con timeout
	for (int i = 0; i < MAX_CLEANUP_ATTEMPTS; i++) {
		usleep(CLEANUP_SLEEP_MS * 1000);

		result = waitpid(ctx->view_pid, &status, WNOHANG);
		if (result > 0) { // View termino
			ctx->view_pid = -1;
			ctx->view_active = false;
			return;
		}
	}

	// 5. Terminacion forzada con SIGKILL (solo si aun esta vivo)
	if (is_process_alive(ctx->view_pid)) {
		if (kill(ctx->view_pid, SIGKILL) == -1) {
			perror("Error: sending SIGKILL to view");
		}
		else {
			// Esperar terminacion forzada
			waitpid(ctx->view_pid, &status, 0);
		}
	}

	ctx->view_pid = -1;
	ctx->view_active = false;
}

void master_cleanup(master_context_t *ctx) {
	// 1. Limpiar view
	cleanup_view_process(ctx);

	// 2. Limpiar players
	if (ctx->player_pids != NULL) {
		for (int i = 0; i < ctx->config.player_count; i++) {
			if (ctx->player_pids[i] > 0) {
				// Verificar si el proceso aun esta vivo antes de enviar SIGTERM
				if (is_process_alive(ctx->player_pids[i])) {
					kill(ctx->player_pids[i], SIGTERM);
					waitpid(ctx->player_pids[i], NULL, 0);
				}
				else {
					// Proceso ya termino, recoger su estado
					waitpid(ctx->player_pids[i], NULL, WNOHANG);
				}
			}
		}
		free(ctx->player_pids);
		ctx->player_pids = NULL;
	}

	// 3. Limpiar pipes
	if (ctx->player_pipes != NULL) {
		for (int i = 0; i < ctx->config.player_count; i++) {
			if (ctx->player_pipes[i] >= 0) {
				close(ctx->player_pipes[i]);
				ctx->player_pipes[i] = -1;
			}
		}
		free(ctx->player_pipes);
		ctx->player_pipes = NULL;
	}

	// 4. Limpiar file descriptors
	if (ctx->sync_fd >= 0) {
		close(ctx->sync_fd);
		ctx->sync_fd = -1;
	}

	if (ctx->state_fd >= 0) {
		close(ctx->state_fd);
		ctx->state_fd = -1;
	}

	// 5. Limpiar memoria compartida
	if (ctx->game_state != NULL) {
		munmap(ctx->game_state, sizeof(game_state_t) + ctx->config.width * ctx->config.height * sizeof(int));
		ctx->game_state = NULL;
	}

	if (ctx->game_sync != NULL) {
		munmap(ctx->game_sync, sizeof(game_sync_t));
		ctx->game_sync = NULL;
	}

	shm_unlink(GAME_STATE_SHM);
	shm_unlink(GAME_SYNC_SHM);

	// 6. Limpiar configuracion
	if (ctx->config.player_paths != NULL) {
		free(ctx->config.player_paths);
		ctx->config.player_paths = NULL;
	}
}

/**
 * @brief Funcion auxiliar para crear un proceso de jugador
 * @param ctx Puntero al contexto del master
 * @param player_id ID del jugador
 * @param width_str Ancho del tablero como cadena
 * @param height_str Alto del tablero como cadena
 * @return 0 si la creacion fue exitosa, -1 en caso de error
 */
static int create_player_process(master_context_t *ctx, int player_id, const char *width_str, const char *height_str) {
	int pipefd[2];
	if (pipe(pipefd) == -1) {
		perror("Error creating pipe for player");
		return -1;
	}

	pid_t pid = fork();
	if (pid == -1) {
		perror("Error forking player process");
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}
	else if (pid == 0) {
		// Proceso hijo (player)
		close(pipefd[0]);

		if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
			perror("Error redirecting stdout in player");
			close(pipefd[1]);
			exit(EXIT_FAILURE);
		}
		close(pipefd[1]);

		execl(ctx->config.player_paths[player_id], ctx->config.player_paths[player_id], width_str, height_str, NULL);
		perror("Error executing player program");
		exit(EXIT_FAILURE);
	}
	else {
		// Proceso play creado
		close(pipefd[1]);
		ctx->player_pids[player_id] = pid;
		ctx->player_pipes[player_id] = pipefd[0];
		ctx->game_state->players[player_id].pid = pid;
	}

	return 0;
}

/**
 * @brief Funcion auxiliar para crear el proceso de vista
 * @param ctx Puntero al contexto del master
 * @param width_str Ancho del tablero como cadena
 * @param height_str Alto del tablero como cadena
 * @return 0 si la creacion fue exitosa, -1 en caso de error
 */
static int create_view_process(master_context_t *ctx, const char *width_str, const char *height_str) {
	ctx->view_pid = fork();
	if (ctx->view_pid == -1) {
		perror("Error forking view process");
		ctx->config.view_path = NULL;
		ctx->view_active = false;
		return 0;
	}
	else if (ctx->view_pid == 0) {
		execl(ctx->config.view_path, ctx->config.view_path, width_str, height_str, NULL);
		perror("Error executing view program");
		exit(EXIT_FAILURE);
	}
	else {
		// View process created
		ctx->view_active = true;
	}

	return 0;
}

int create_processes(master_context_t *ctx) {
	ctx->player_pids = calloc(ctx->config.player_count, sizeof(pid_t));
	if (ctx->player_pids == NULL) {
		perror("Error allocating memory for player PIDs");
		return -1;
	}

	ctx->player_pipes = malloc(ctx->config.player_count * sizeof(int));
	if (ctx->player_pipes == NULL) {
		perror("Error allocating memory for player pipes");
		free(ctx->player_pids);
		ctx->player_pids = NULL;
		return -1;
	}

	char width_str[16], height_str[16];
	snprintf(width_str, sizeof(width_str), "%d", ctx->config.width);
	snprintf(height_str, sizeof(height_str), "%d", ctx->config.height);

	for (int i = 0; i < ctx->config.player_count; i++) {
		if (create_player_process(ctx, i, width_str, height_str) != 0) {
			// Cleanup: terminar procesos ya creados
			for (int j = 0; j < i; j++) {
				if (ctx->player_pids[j] > 0) {
					kill(ctx->player_pids[j], SIGTERM);
					waitpid(ctx->player_pids[j], NULL, 0);
				}
			}
			free(ctx->player_pids);
			free(ctx->player_pipes);
			ctx->player_pids = NULL;
			ctx->player_pipes = NULL;
			return -1;
		}
	}

	if (ctx->config.view_path != NULL) {
		if (create_view_process(ctx, width_str, height_str) != 0) {
			// Cleanup: terminar todos los procesos creados
			for (int i = 0; i < ctx->config.player_count; i++) {
				if (ctx->player_pids[i] > 0) {
					kill(ctx->player_pids[i], SIGTERM);
					waitpid(ctx->player_pids[i], NULL, 0);
				}
			}
			free(ctx->player_pids);
			free(ctx->player_pipes);
			ctx->player_pids = NULL;
			ctx->player_pipes = NULL;
			return -1;
		}
	}

	return 0;
}
