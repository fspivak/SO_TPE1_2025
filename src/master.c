// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#define _GNU_SOURCE
#include "lib/common.h"
#include "lib/library.h"
#include "lib/master_functions.h"
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

// Variables globales
game_state_t *game_state = NULL;
game_sync_t *game_sync = NULL;
int state_fd = -1, sync_fd = -1;
pid_t *player_pids = NULL;
pid_t view_pid = -1;
int *player_pipes = NULL;
master_config_t config = {0};
bool cleanup_done = false;
bool view_active = true;

// TODO: SE PUEDE ELIMINAR EL WRAPPER Y USAR DIRECTAMENTE master_cleanup???
// wrapper
void master_close_up(void) {
	master_cleanup();
}

// TODO: SE PUEDE ELIMINAR EL WRAPPER Y USAR DIRECTAMENTE generic_signal_handler con master_cleanup???
// wrapper
void master_signal_wrapper(int sig) {
	printf("Master received signal %d\n", sig);
	generic_signal_handler(sig, "Master", -1, master_close_up);
}

void execute_player_move(int player_id, unsigned char direction) {
	player_t *player = &game_state->players[player_id];
	int dx, dy;
	get_direction_offset((direction_t) direction, &dx, &dy);

	int new_x = player->x + dx;
	int new_y = player->y + dy;

	int *cell = get_cell(game_state, new_x, new_y);
	int reward = *cell;

	player->x = new_x;
	player->y = new_y;
	player->score += reward;
	player->valid_moves++;

	*cell = -(player_id);
}

bool check_game_end(void) {
	bool any_active = false;

	for (int player_id = 0; player_id < config.player_count; player_id++) {
		if (game_state->players[player_id].is_blocked)
			continue;

		bool has_valid_moves = false;
		for (unsigned char dir = 0; dir < 8; dir++) {
			if (is_valid_move(player_id, dir, game_state)) {
				has_valid_moves = true;
				any_active = true;
				break;
			}
		}

		if (!has_valid_moves && !game_state->players[player_id].is_blocked) {
			// Jugador se bloqueó, marcarlo pero darle tiempo para que termine naturalmente
			game_state->players[player_id].is_blocked = true;
			// NO cerramos el pipe aquí - el jugador necesita leer el estado is_blocked
		}
	}

	return !any_active;
}

// Función mejorada para sincronizar con view
void sync_with_view(void) {
	if (!view_active || config.view_path == NULL)
		return;

	// Verificar si view sigue viva antes de cualquier operación
	if (!is_process_alive(view_pid)) {
		printf("View process died, disabling view\n");
		view_active = false;
		return;
	}

	// Notificar a view con verificación post-signal
	sem_post(&game_sync->view_ready);

	// Verificación adicional inmediatamente después del post
	if (!is_process_alive(view_pid)) {
		printf("View died immediately after signal, disabling view\n");
		view_active = false;
		// Intentar drenar el semáforo que acabamos de postear
		struct timespec immediate_timeout = {0, 1000000}; // 1ms
		sem_timedwait(&game_sync->view_done, &immediate_timeout);
		return;
	}

	// Esperar respuesta con timeout
	if (sem_wait_with_timeout(&game_sync->view_done, VIEW_TIMEOUT_SEC) == -1) {
		printf("View timeout - disabling view for rest of game\n");
		view_active = false;

		// Verificación final para determinar si murió durante la espera
		if (!is_process_alive(view_pid)) {
			printf("View died during timeout wait\n");
		}
		return;
	}
}

void game_loop(void) {
	fd_set readfds;
	struct timeval timeout_tv;
	time_t last_valid_move = time(NULL);
	int current_player = 0;

	printf("Starting game loop\n");

	// Sincronización inicial con view
	if (view_active && config.view_path != NULL) {
		printf("Initial sync with view\n");
		sync_with_view();
	}

	while (!game_state->game_finished) {
		FD_ZERO(&readfds);
		int max_fd = 0;

		for (int i = 0; i < config.player_count; i++) {
			if (!game_state->players[i].is_blocked && player_pipes[i] != -1) {
				FD_SET(player_pipes[i], &readfds);
				if (player_pipes[i] > max_fd) {
					max_fd = player_pipes[i];
				}
			}
		}

		timeout_tv.tv_sec = 1;
		timeout_tv.tv_usec = 0;

		int ready = select(max_fd + 1, &readfds, NULL, NULL, &timeout_tv);

		if (ready == -1) {
			if (errno == EINTR)
				continue;
			perror("Select error");
			break;
		}

		if (ready == 0) {
			if (difftime(time(NULL), last_valid_move) >= config.timeout) {
				printf("Timeout reached, ending game\n");
				break;
			}
			continue;
		}

		bool movement_processed = false;

		// Procesar movimientos de jugadores
		for (int attempts = 0; attempts < config.player_count && !movement_processed; attempts++) {
			int player_id = (current_player + attempts) % config.player_count;

			if (game_state->players[player_id].is_blocked || !FD_ISSET(player_pipes[player_id], &readfds)) {
				continue;
			}

			unsigned char move;
			ssize_t bytes_read = read(player_pipes[player_id], &move, 1);

			if (bytes_read <= 0) {
				game_state->players[player_id].is_blocked = true;
				if (player_pipes[player_id] != -1) {
					close(player_pipes[player_id]);
					player_pipes[player_id] = -1;
				}
				continue;
			}

			sem_wait(&game_sync->reader_writer_mutex);
			sem_wait(&game_sync->state_mutex);
			sem_post(&game_sync->reader_writer_mutex);

			if (is_valid_move(player_id, move, game_state)) {
				execute_player_move(player_id, move);
				last_valid_move = time(NULL);
			}
			else {
				game_state->players[player_id].invalid_moves++;
			}

			sem_post(&game_sync->state_mutex);
			sem_post(&game_sync->player_turn[player_id]);

			movement_processed = true;
			current_player = (player_id + 1) % config.player_count;
		}

		// Verificar fin de juego después de procesar movimientos
		if (check_game_end()) {
			printf("Game ended - no more valid moves available\n");
			game_state->game_finished = true;
			// Notificar a todos los jugadores para que despierten y salgan
			for (int i = 0; i < config.player_count; i++) {
				sem_post(&game_sync->player_turn[i]);
			}
			break;
		}

		// Sincronizar con view si hubo movimientos
		if (movement_processed) {
			sync_with_view();
			if (config.delay > 0) {
				usleep(config.delay * 1000);
			}
		}

		// Timeout global del juego
		if (time(NULL) - last_valid_move > config.timeout) {
			printf("Game timeout reached\n");
			game_state->game_finished = true;

			// Notificar a todos los jugadores para que despierten y salgan
			for (int i = 0; i < config.player_count; i++) {
				sem_post(&game_sync->player_turn[i]);
			}
			break;
		}
	}

	// Notificación final a view
	if (view_active && config.view_path != NULL) {
		printf("Final sync with view\n");
		sync_with_view();
		usleep(500000); // Dar tiempo para que view procese el estado final
	}

	printf("Game loop ended\n");
}

// TODO: imprimir el valor de la señal de salida de los procesos
void print_final_results(void) {
	printf("\n=== FINAL RESULTS ===\n");

	// Ordenar jugadores por puntuación
	int sorted_indices[MAX_PLAYERS];
	for (int i = 0; i < config.player_count; i++) {
		sorted_indices[i] = i;
	}

	// Ordenamiento burbuja simple
	for (int i = 0; i < config.player_count - 1; i++) {
		for (int j = 0; j < config.player_count - i - 1; j++) {
			if (game_state->players[sorted_indices[j]].score < game_state->players[sorted_indices[j + 1]].score ||
				(game_state->players[sorted_indices[j]].score == game_state->players[sorted_indices[j + 1]].score &&
				 game_state->players[sorted_indices[j]].invalid_moves >
					 game_state->players[sorted_indices[j + 1]].invalid_moves)) {
				int temp = sorted_indices[j];
				sorted_indices[j] = sorted_indices[j + 1];
				sorted_indices[j + 1] = temp;
			}
		}
	}

	// Imprimir resultados ordenados
	for (int i = 0; i < config.player_count; i++) {
		int idx = sorted_indices[i];
		printf("%d. %s: %u points (%u valid, %u invalid moves)\n", i + 1, game_state->players[idx].name,
			   game_state->players[idx].score, game_state->players[idx].valid_moves,
			   game_state->players[idx].invalid_moves);
	}

	printf("=====================\n");
}

int main(int argc, char *argv[]) {
	// Registrar cleanup automático para el master
	atexit(master_cleanup);

	// Instalar manejadores de señales
	signal(SIGINT, master_signal_wrapper);	// Ctrl+C
	signal(SIGTERM, master_signal_wrapper); // kill
	signal(SIGCHLD, SIG_IGN);				// Evitar procesos zombie

	printf("ChompChamps Master starting...\n");

	// Parsear argumentos
	parse_arguments(argc, argv, &config);

	printf("Game configuration:\n");
	printf("  Board: %dx%d\n", config.width, config.height);
	printf("  Delay: %dms\n", config.delay);
	printf("  Timeout: %ds\n", config.timeout);
	printf("  Seed: %u\n", config.seed);
	printf("  Players: %d\n", config.player_count);
	for (int i = 0; i < config.player_count; i++) {
		printf("    %d: %s\n", i, config.player_paths[i]);
	}
	if (config.view_path) {
		printf("  View: %s\n", config.view_path);
	}

	// Crear memoria compartida
	if (create_shared_memories() != 0) {
		fprintf(stderr, "Failed to create shared memories\n");
		exit(EXIT_FAILURE);
	}

	// Inicializar estado del juego
	initialize_game_state();

	// Inicializar sincronización
	initialize_synchronization();

	// Crear procesos
	if (create_processes() != 0) {
		fprintf(stderr, "Failed to create processes\n");
		master_cleanup();
		exit(EXIT_FAILURE);
	}

	printf("All processes created successfully\n");

	// Dar tiempo a los procesos para conectarse a la memoria compartida
	usleep(100000); // 100ms

	// Notificar a view que la memoria compartida está lista
	if (view_active && config.view_path != NULL) {
		sem_post(&game_sync->view_ready);
	}

	// Ejecutar bucle principal del juego
	game_loop();

	// Imprimir resultados finales
	print_final_results();

	// Cleanup
	master_cleanup();

	printf("ChompChamps Master finished\n");
	return EXIT_SUCCESS;
}