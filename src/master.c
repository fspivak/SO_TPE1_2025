// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#define _GNU_SOURCE
#include "lib/common.h"
#include "lib/config_management.h"
#include "lib/game_logic.h"
#include "lib/library.h"
#include "lib/memory_management.h"
#include "lib/process_management.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

master_context_t master_ctx = {0};

/**
 * @brief Wrapper para cleanup del master
 * @details Necesario para usar con atexit
 */
void master_cleanup_wrapper(void) {
	master_cleanup(&master_ctx);
}
/**
 * @brief Manejador de señales especifico para master
 * @param sig Numero de señal recibida
 * @details Limpia y termina el master
 */
void master_signal_handler(int sig) {
	printf("Master terminated by signal %d\n", sig);
	generic_signal_handler(sig, "Master", -1, master_cleanup_wrapper);
}

int main(int argc, char *argv[]) {
	atexit(master_cleanup_wrapper);

	setup_standard_signals(master_signal_handler);
	signal(SIGCHLD, SIG_IGN); // Evita procesos zombie

	parse_arguments(argc, argv, &master_ctx.config);

	if (create_shared_memories(&master_ctx) != 0) {
		fprintf(stderr, "Failed to create shared memories\n");
		exit(EXIT_FAILURE);
	}

	initialize_game_state(&master_ctx);

	initialize_synchronization(&master_ctx);

	display_game_parameters(&master_ctx.config);

	if (create_processes(&master_ctx) != 0) {
		fprintf(stderr, "Failed to create processes\n");
		master_cleanup(&master_ctx);
		exit(EXIT_FAILURE);
	}

	display_processes_info(&master_ctx.config, master_ctx.player_pids, master_ctx.view_pid, master_ctx.view_active);

	display_game_start();

	// Notificar a view que la memoria compartida esta lista
	if (master_ctx.view_active && master_ctx.config.view_path != NULL) {
		sem_post(&master_ctx.game_sync->view_ready);
	}

	// Notificar a todos los jugadores que pueden empezar a jugar
	for (int i = 0; i < master_ctx.config.player_count; i++) {
		sem_post(&master_ctx.game_sync->player_turn[i]);
	}

	// Ejecutar bucle principal del juego
	game_loop(&master_ctx);

	// Verificar el codigo de salida de la view
	if (master_ctx.view_active && master_ctx.view_pid > 0) {
		sem_wait(&master_ctx.game_sync->view_done);

		int view_status;
		pid_t result = waitpid(master_ctx.view_pid, &view_status, 0);
		if (result > 0) {
			int exit_code = 0;
			if (WIFEXITED(view_status)) {
				exit_code = WEXITSTATUS(view_status);
			}
			else if (WIFSIGNALED(view_status)) {
				exit_code = WTERMSIG(view_status);
			}
			printf("View exited (%d)\n", exit_code);
		}
		else {
			printf("View exited (0)\n");
		}
	}

	print_final_results(&master_ctx);

	master_cleanup(&master_ctx);

	return EXIT_SUCCESS;
}