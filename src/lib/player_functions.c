// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#include "player_functions.h"
#include "library.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// Variables externas definidas en player.c
extern game_state_t *game_state;
extern game_sync_t *game_sync;
extern int state_fd, sync_fd, player_id;

int find_my_player_id(void) {
	pid_t my_pid = getpid();
	for (unsigned int i = 0; i < game_state->player_count; i++) {
		if (game_state->players[i].pid == my_pid) {
			return (int) i;
		}
	}
	return -1;
}

void enter_read_state(void) {
	// Readers-Writers para evitar inanición
	sem_wait(&game_sync->reader_writer_mutex); // Aseguro que no haya escritores esperando
	sem_wait(&game_sync->reader_count_mutex);  // Aseguro exclusión mutua para reader_count

	game_sync->reader_count++;
	if (game_sync->reader_count == 1) {
		sem_wait(&game_sync->state_mutex); // Espero mi turno y obtengo el estado del juego
	}

	sem_post(&game_sync->reader_count_mutex);  // Libero exclusión mutua para reader_count
	sem_post(&game_sync->reader_writer_mutex); // Libero para que otros lectores puedan entrar
}

void exit_read_state(void) {
	sem_wait(&game_sync->reader_count_mutex); // Aseguro exclusión mutua para reader_count

	game_sync->reader_count--;
	if (game_sync->reader_count == 0) {
		sem_post(&game_sync->state_mutex); // Devuelvo el estado del juego al resto
	}

	sem_post(&game_sync->reader_count_mutex); // Libero exclusión mutua para reader_count
}

bool is_valid_move(int player_id, direction_t direction) {
	player_t *player = &game_state->players[player_id];
	int dx, dy;

	// Futura posición del player
	get_direction_offset(direction, &dx, &dy);
	int new_x = player->x + dx;
	int new_y = player->y + dy;

	// Validación de que la nueva posición esté dentro del tablero
	if (new_x < 0 || new_y < 0 || new_x >= game_state->width || new_y >= game_state->height) {
		return false;
	}

	// Validar que no esté ocupado
	int *cell = get_cell(game_state, new_x, new_y);
	return (*cell > 0);
}

direction_t choose_strategic_move(void) {
	// Estrategia simple: movimiento aleatorio
	// TODO: Implementar estrategia más inteligente
	return (direction_t) (rand() % 8);
}

void send_move(direction_t move) {
	unsigned char move_byte = (unsigned char) move;
	ssize_t bytes_written = write(STDOUT_FILENO, &move_byte, 1);
	if (bytes_written != 1) {
		perror("Error sending movement");
	}
}