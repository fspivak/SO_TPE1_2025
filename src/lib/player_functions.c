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