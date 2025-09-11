#ifndef LIBRARY_H
#define LIBRARY_H

#include "common.h"

// Constantes para colores de jugadores formato ANSI
#define MAX_PLAYER_COLORS 9
#define COLOR_PLAYER_1 "\033[1;31m"		 // Rojo brillante
#define COLOR_PLAYER_2 "\033[1;32m"		 // Verde brillante
#define COLOR_PLAYER_3 "\033[1;34m"		 // Azul brillante
#define COLOR_PLAYER_4 "\033[1;35m"		 // Magenta brillante
#define COLOR_PLAYER_5 "\033[1;36m"		 // Cian brillante
#define COLOR_PLAYER_6 "\033[1;33m"		 // Amarillo brillante
#define COLOR_PLAYER_7 "\033[1;37m"		 // Blanco brillante
#define COLOR_PLAYER_8 "\033[1;90m"		 // Gris brillante
#define COLOR_PLAYER_9 "\033[1;38;5;94m" // Marron brillante
#define COLOR_RESET "\033[0m"

/**
 * @brief Obtiene el color correspondiente a un jugador
 * @param player_id ID del jugador (0-8)
 * @return Codigo de color ANSI para el jugador
 */
const char *get_player_color(int player_id);

/**
 * @brief Calcula el tamaño necesario para la memoria compartida del estado del juego
 * @param width Ancho del tablero
 * @param height Alto del tablero
 * @return Tamaño en bytes
 */
size_t calculate_game_state_size(int width, int height);

/**
 * @brief Calcula el tamaño necesario para la memoria compartida de sincronizacion
 * @return Tamaño en bytes
 */
size_t calculate_game_sync_size(void);

/**
 * @brief Configura las señales estandar (SIGINT, SIGTERM)
 * @param signal_handler Funcion manejadora de señales
 */
void setup_standard_signals(void (*signal_handler)(int));

/**
 * @brief Valida los parametros de linea de comandos del programa.
 * @param argc Numero de argumentos.
 * @param argv Array de cadenas que contiene los argumentos.
 */
void check_params(int argc, char *argv[]);

/**
 * @brief Cierra y desmapea las memorias compartidas.
 * @param sync_fd Puntero al descriptor de archivo de la memoria compartida de sincronizacion.
 * @param state_fd Puntero al descriptor de archivo de la memoria compartida del estado del juego.
 * @param game_state Puntero doble al estado del juego mapeado.
 * @param game_sync Puntero doble a la estructura de sincronizacion mapeada.
 */
void close_up(int *sync_fd, int *state_fd, game_state_t **game_state, game_sync_t **game_sync);

/**
 * @brief Conecta y mapea las memorias compartidas para el estado del juego y la sincronizacion.
 * @param game_state_size Tamaño de la memoria compartida del estado del juego.
 * @param game_sync_size Tamaño de la memoria compartida de sincronizacion.
 * @param sync_fd Puntero al descriptor de archivo de la memoria compartida de sincronizacion.
 * @param state_fd Puntero al descriptor de archivo de la memoria compartida del estado del juego.
 * @param game_state Puntero doble al estado del juego mapeado.
 * @param game_sync Puntero doble a la estructura de sincronizacion mapeada.
 * @return 0 si la conexion y el mapeo fueron exitosos, -1 en caso de error.
 */
int connect_shared_memories(int game_state_size, int game_sync_size, int *sync_fd, int *state_fd,
							game_state_t **game_state, game_sync_t **game_sync);

/**
 * @brief Manejador generico de señales
 * @param sig Numero de señal recibida
 * @param process_name Nombre del proceso (para logging)
 * @param process_id ID del proceso (opcional, -1 si no aplica)
 * @param cleanup_fn Funcion de limpieza especifica
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
int *get_cell(game_state_t *state, int x, int y);

/**
 * @brief Verifica si un movimiento es valido para un jugador
 * @param player_id ID del jugador
 * @param direction Direccion del movimiento
 * @param game_state Estado del juego
 * @return true si el movimiento es valido, false en caso contrario
 */
bool is_valid_move(int player_id, direction_t direction, game_state_t *game_state);
/**
 * @brief Obtiene el desplazamiento en x e y segun la direccion
 * @param dir Direccion del movimiento
 * @param dx Puntero para almacenar el desplazamiento en x
 * @param dy Puntero para almacenar el desplazamiento en y
 */
void get_direction_offset(direction_t dir, int *dx, int *dy);

#endif // LIBRARY_H