#ifndef LIBRARY_H
#define LIBRARY_H

#include "common.h"

/**
 * @brief Valida los parámetros de línea de comandos del programa.
 * @param argc Número de argumentos.
 * @param argv Array de cadenas que contiene los argumentos.
 */
void check_params(int argc, char *argv[]);

/**
 * @brief Cierra y desmapea las memorias compartidas.
 * @param sync_fd Puntero al descriptor de archivo de la memoria compartida de sincronización.
 * @param state_fd Puntero al descriptor de archivo de la memoria compartida del estado del juego.
 * @param game_state Puntero doble al estado del juego mapeado.
 * @param game_sync Puntero doble a la estructura de sincronización mapeada.
 */
void close_up(int *sync_fd, int *state_fd, game_state_t **game_state, game_sync_t **game_sync);

/**
 * @brief Conecta y mapea las memorias compartidas para el estado del juego y la sincronización.
 * @param game_state_size Tamaño de la memoria compartida del estado del juego.
 * @param game_sync_size Tamaño de la memoria compartida de sincronización.
 * @param sync_fd Puntero al descriptor de archivo de la memoria compartida de sincronización.
 * @param state_fd Puntero al descriptor de archivo de la memoria compartida del estado del juego.
 * @param game_state Puntero doble al estado del juego mapeado.
 * @param game_sync Puntero doble a la estructura de sincronización mapeada.
 * @return 0 si la conexión y el mapeo fueron exitosos, -1 en caso de error.
 */
int connect_shared_memories(int game_state_size, int game_sync_size, int *sync_fd, int *state_fd,
							game_state_t **game_state, game_sync_t **game_sync);

/**
 * @brief Manejador genérico de señales
 * @param sig Número de señal recibida
 * @param process_name Nombre del proceso (para logging)
 * @param process_id ID del proceso (opcional, -1 si no aplica)
 * @param cleanup_fn Función de limpieza específica
 */
void generic_signal_handler(int sig, const char *process_name, int process_id, void (*cleanup_fn)(void));

/**
 * @brief Limpia la pantalla del terminal
 */
void clean_screen(void);

/**
 * @brief Limpia el buffer de salida
 */
void clean_buffer(void);

/**
 * @brief Obtiene un puntero a la celda del tablero
 * @param state Estado del juego
 * @param x Coordenada x de la celda
 * @param y Coordenada y de la celda
 * @return Puntero a la celda en el tablero
 */
static inline int *get_cell(game_state_t *state, int x, int y) {
	return &state->board[y * state->width + x];
}

/**
 * @brief Verifica si un movimiento es válido para un jugador
 * @param player_id ID del jugador
 * @param direction Dirección del movimiento
 * @param game_state Estado del juego
 * @return true si el movimiento es válido, false en caso contrario
 */
bool is_valid_move(int player_id, direction_t direction, game_state_t *game_state);
/**
 * @brief Obtiene el desplazamiento en x e y según la dirección
 * @param dir Dirección del movimiento
 * @param dx Puntero para almacenar el desplazamiento en x
 * @param dy Puntero para almacenar el desplazamiento en y
 */
static inline void get_direction_offset(direction_t dir, int *dx, int *dy) {
	static const int offsets[][2] = {
		{0, -1}, // UP
		{1, -1}, // UP_RIGHT
		{1, 0},	 // RIGHT
		{1, 1},	 // DOWN_RIGHT
		{0, 1},	 // DOWN
		{-1, 1}, // DOWN_LEFT
		{-1, 0}, // LEFT
		{-1, -1} // UP_LEFT
	};
	*dx = offsets[dir][0];
	*dy = offsets[dir][1];
}

#endif // LIBRARY_H