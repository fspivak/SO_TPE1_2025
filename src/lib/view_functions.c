// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "view_functions.h"
#include "library.h"
#include <stdio.h>
#include <stdlib.h>

void print_header(view_context_t *ctx) {
	printf("=== CHOMPCHAMPS ===\n");
	printf("Board: %d x %d | Players: %u\n\n", ctx->game_state->width, ctx->game_state->height,
		   ctx->game_state->player_count);
}

void print_players_info(view_context_t *ctx) {
	printf("PLAYERS:\n");
	const unsigned short players_count = ctx->game_state->player_count;
	const player_t *players_array = ctx->game_state->players;

	for (unsigned short i = 0; i < players_count; i++) {
		const player_t *player = &players_array[i];
		char status = player->is_blocked ? 'X' : 'O';
		const char *color = get_player_color(i);

		printf("  %d. %s%s%s [%c] - Pos: (%2d,%2d) | Score: %3u | V:%2u I:%2u\n", i + 1, color, player->name,
			   COLOR_RESET, status, player->x, player->y, player->score, player->valid_moves, player->invalid_moves);
	}
	printf("\n");
}

/**
 * @brief Funcion auxiliar para imprimir encabezados de columnas
 * @param width Ancho del tablero
 */
static void print_column_headers(unsigned short width) {
	printf("    ");
	for (unsigned short x = 0; x < width; x++) {
		printf("%2d ", x);
	}
	printf("\n");
}

/**
 * @brief Funcion auxiliar para imprimir una linea separadora
 * @param width Ancho del tablero
 */
static void print_separator_line(unsigned short width) {
	printf("   ");
	for (unsigned short x = 0; x < width; x++) {
		printf("---");
	}
	printf("\n");
}

/**
 * @brief Funcion auxiliar para imprimir una fila del tablero
 * @param ctx Puntero al contexto del view
 * @param y indice de la fila a imprimir
 */
static void print_board_row(view_context_t *ctx, unsigned short y) {
	const unsigned short width = ctx->game_state->width;
	const unsigned short players_count = ctx->game_state->player_count;

	printf("%2d |", y);

	for (unsigned short x = 0; x < width; x++) {
		int *cell = get_cell(ctx->game_state, x, y);

		if (*cell > 0) {
			printf("%2d ", *cell);
		}
		else {
			// Celda ocupada por un jugador
			int player_id = -(*cell);
			if ((unsigned short) player_id < players_count) {
				const char *color = get_player_color(player_id);
				printf("%sP%d%s ", color, player_id + 1, COLOR_RESET);
			}
			else {
				printf("?? ");
			}
		}
	}
	printf("\n");
}

void print_board(view_context_t *ctx) {
	printf("BOARD:\n");
	const unsigned short width = ctx->game_state->width;
	const unsigned short height = ctx->game_state->height;

	print_column_headers(width);

	print_separator_line(width);

	for (unsigned short y = 0; y < height; y++) {
		print_board_row(ctx, y);
	}
}

void print_legend(view_context_t *ctx) {
	printf("\nLEGEND:\n");
	printf("  1-9: Available rewards\n");
	printf("  Player colors: ");
	const unsigned short players_count = ctx->game_state->player_count;
	for (unsigned short i = 0; i < players_count; i++) {
		const char *color = get_player_color(i);
		printf("%sP%d%s", color, i + 1, COLOR_RESET);
		if (i < players_count - 1) {
			printf(" ");
		}
	}
	printf("\n");
	printf("  [O]: Active player | [X]: Blocked player\n");
	printf("  V: Valid moves | I: Invalid moves\n");
}

void print_winner(view_context_t *ctx) {
	printf("\n*** GAME OVER ***\n");

	// Ganador
	int winner = -1;
	unsigned int max_score = 0;

	for (unsigned int i = 0; i < ctx->game_state->player_count; i++) {
		if (ctx->game_state->players[i].score > max_score) {
			max_score = ctx->game_state->players[i].score;
			winner = (int) i;
		}
		else if (ctx->game_state->players[i].score == max_score && winner != -1) {
			// Criterios de desempate
			if (ctx->game_state->players[i].valid_moves < ctx->game_state->players[winner].valid_moves) {
				winner = (int) i;
			}
			else if (ctx->game_state->players[i].valid_moves == ctx->game_state->players[winner].valid_moves) {
				if (ctx->game_state->players[i].invalid_moves < ctx->game_state->players[winner].invalid_moves) {
					winner = (int) i;
				}
			}
		}
	}

	if (winner != -1) {
		const char *winner_color = get_player_color(winner);
		printf("¡WINNER: %s%s%s (%sP%d%s) with %u points!\n", winner_color, ctx->game_state->players[winner].name,
			   COLOR_RESET, winner_color, winner + 1, COLOR_RESET, ctx->game_state->players[winner].score);
	}
	else {
		printf("¡DRAW!\n");
	}
}

void print_game_state(view_context_t *ctx) {
	clean_screen();
	print_header(ctx);
	print_players_info(ctx);
	print_board(ctx);
	print_legend(ctx);

	if (ctx->game_state->game_finished) {
		print_winner(ctx);
	}

	printf("\n");
	clean_buffer();
}

void initialize_view_context(view_context_t *ctx, int argc, char *argv[]) {
	check_params(argc, argv);

	int width = atoi(argv[1]);
	int height = atoi(argv[2]);
	size_t game_state_size = calculate_game_state_size(width, height);
	size_t game_sync_size = calculate_game_sync_size();

	if (connect_shared_memories(game_state_size, game_sync_size, &ctx->sync_fd, &ctx->state_fd, &ctx->game_state,
								&ctx->game_sync)) {
		fprintf(stderr, "Error to initialize shared memory view");
		exit(EXIT_FAILURE);
	}
}

void view_main_loop(view_context_t *ctx) {
	while (1) {
		// Esperar señal del master
		if (sem_wait(&ctx->game_sync->view_ready) != 0) {
			perror("Error receiving signal from Master");
			break;
		}

		print_game_state(ctx);

		// Notificar al master
		if (sem_post(&ctx->game_sync->view_done) != 0) {
			perror("Error sending signal to Master");
			break;
		}

		// Si el juego termino, salir despues de notificar
		if (ctx->game_state->game_finished) {
			break;
		}
	}
}
