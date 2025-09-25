// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#define _GNU_SOURCE
#include "lib/common.h"
#include "lib/library.h"
#include "lib/player_functions.h"
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define tornado_strategic false

player_context_t player_ctx = {0};

/**
 * @brief Wrapper para cleanup del player
 * @details Necesario para usar con atexit
 */
void player_cleanup_wrapper(void) {
	close_up(&player_ctx.sync_fd, &player_ctx.state_fd, &player_ctx.game_state, &player_ctx.game_sync);
}

/**
 * @brief Manejador de señales especifico para player
 * @param sig Numero de señal recibida
 * @details Limpia y termina el player
 */
void player_signal_handler(int sig) {
	printf("Player terminated by signal %d\n", sig);
	generic_signal_handler(sig, "Player", player_ctx.player_id + 1, player_cleanup_wrapper);
}

int main(int argc, char *argv[]) {
	atexit(player_cleanup_wrapper);

	setup_standard_signals(player_signal_handler);

	initialize_player_context(&player_ctx, argc, argv);

	player_main_loop(&player_ctx, tornado_strategic);

	return EXIT_SUCCESS;
}