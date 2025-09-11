// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "lib/common.h"
#include "lib/library.h"
#include "lib/view_functions.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

view_context_t view_ctx = {0};

/**
 * @brief Wrapper para cleanup del view
 * @details Necesario para usar con atexit
 */
void view_cleanup_wrapper(void) {
	close_up(&view_ctx.sync_fd, &view_ctx.state_fd, &view_ctx.game_state, &view_ctx.game_sync);
}

/**
 * @brief Manejador de señales especifico para view
 * @param sig Numero de señal recibida
 * @details Marca la view para terminacion y permite que termine naturalmente
 */
void view_signal_handler(int sig) {
	printf("View terminated by signal %d\n", sig);

	view_ctx.game_state->game_finished = true;

	// Asegurar que el master sepa que la view termino
	if (view_ctx.game_sync != NULL) {
		if (sem_post(&view_ctx.game_sync->view_done) != 0) {
			perror("Error sending final signal to Master");
		}
	}
}

int main(int argc, char *argv[]) {
	atexit(view_cleanup_wrapper);

	setup_standard_signals(view_signal_handler);

	initialize_view_context(&view_ctx, argc, argv);

	view_main_loop(&view_ctx);

	return EXIT_SUCCESS;
}