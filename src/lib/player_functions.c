// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "player_functions.h"
#include "library.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int find_my_player_id(player_context_t *ctx) {
	pid_t my_pid = getpid();
	for (unsigned int i = 0; i < ctx->game_state->player_count; i++) {
		if (ctx->game_state->players[i].pid == my_pid) {
			return (int) i;
		}
	}
	return -1;
}

void enter_read_state(player_context_t *ctx) {
	// lector-escritor para evitar inanicion
	sem_wait(&ctx->game_sync->reader_writer_mutex); // Aseguro que no haya escritores esperando
	sem_wait(&ctx->game_sync->reader_count_mutex);	// Aseguro exclusion mutua para reader_count

	ctx->game_sync->reader_count++;
	if (ctx->game_sync->reader_count == 1) {
		sem_wait(&ctx->game_sync->state_mutex); // Espero mi turno y obtengo el estado del juego
	}

	sem_post(&ctx->game_sync->reader_count_mutex);	// Libero exclusion mutua para reader_count
	sem_post(&ctx->game_sync->reader_writer_mutex); // Libero para que otros lectores puedan entrar
}

void exit_read_state(player_context_t *ctx) {
	sem_wait(&ctx->game_sync->reader_count_mutex); // Aseguro exclusion mutua para reader_count

	ctx->game_sync->reader_count--;
	if (ctx->game_sync->reader_count == 0) {
		sem_post(&ctx->game_sync->state_mutex); // Devuelvo el estado del juego al resto
	}

	sem_post(&ctx->game_sync->reader_count_mutex); // Libero exclusion mutua para reader_count
}

direction_t choose_tornado_move(player_context_t *ctx, direction_t last_move, int cant_moves) {
	if (!(is_valid_move(ctx->player_id, last_move, ctx->game_state) && cant_moves < 9)) {
		if (last_move == 0) {
			last_move = 8;
		}
		last_move = choose_tornado_move(ctx, --last_move, ++cant_moves);
	}

	return last_move;
}

direction_t choose_random_move() {
	srand(time(NULL));

	direction_t move = (rand() % 7);
	return move;
}

direction_t select_first_move(int player_id, game_state_t *game_state) {
	int width = game_state->width;
	int height = game_state->height;
	int player_x = game_state->players[player_id].x;
	int player_y = game_state->players[player_id].y;

	direction_t direction = 0;

	if ((player_x) >= (width / 2)) {
		if ((player_y) >= (height / 2)) {
			direction = DIR_UP;
		}
		else {
			direction = DIR_LEFT;
		}
	}
	else {
		if ((player_y) >= (height / 2)) {
			direction = DIR_DOWN;
		}
		else {
			direction = DIR_RIGHT;
		}
	}
	return direction;
}

void send_move(direction_t move) {
	unsigned char move_byte = (unsigned char) move;
	ssize_t bytes_written = write(STDOUT_FILENO, &move_byte, 1);
	if (bytes_written != 1) {
		perror("Error sending movement");
	}
}

void initialize_player_context(player_context_t *ctx, int argc, char *argv[]) {
	check_params(argc, argv);

	int width = atoi(argv[1]);
	int height = atoi(argv[2]);
	size_t game_state_size = calculate_game_state_size(width, height);
	size_t game_sync_size = calculate_game_sync_size();

	// Generador de numeros aleatorios
	srand(time(NULL) + getpid());

	if (connect_shared_memories(game_state_size, game_sync_size, &ctx->sync_fd, &ctx->state_fd, &ctx->game_state,
								&ctx->game_sync) != 0) {
		fprintf(stderr, "Error to initialize shared memory player");
		exit(EXIT_FAILURE);
	}

	// Encontrar ID de jugador
	enter_read_state(ctx);
	ctx->player_id = find_my_player_id(ctx);
	exit_read_state(ctx);

	if (ctx->player_id == -1) {
		fprintf(stderr, "Error: Could not find player ID\n");
		exit(EXIT_FAILURE);
	}
}

void player_main_loop(player_context_t *ctx, bool tornado_strategic) {
	direction_t chosen_move = select_first_move(ctx->player_id, ctx->game_state);

	while (true) {
		if (sem_wait(&ctx->game_sync->player_turn[ctx->player_id]) != 0) {
			perror("Error waiting for player turn");
			break;
		}

		enter_read_state(ctx); // Leer el estado del juego de forma sincronizada

		if (ctx->game_state->game_finished || ctx->game_state->players[ctx->player_id].is_blocked) {
			exit_read_state(ctx);
			break;
		}

		chosen_move = (tornado_strategic ? choose_tornado_move(ctx, chosen_move, 0) : choose_random_move());

		exit_read_state(ctx);

		send_move(chosen_move); // Enviar movimiento al master
	}
}