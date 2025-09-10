#ifndef VIEW_FUNCTIONS_H
#define VIEW_FUNCTIONS_H

#include "common.h"

// Colores para los 9 jugadores
#define COLOR_PLAYER_1 "\033[1;31m"		 // Rojo brillante
#define COLOR_PLAYER_2 "\033[1;32m"		 // Verde brillante
#define COLOR_PLAYER_3 "\033[1;34m"		 // Azul brillante
#define COLOR_PLAYER_4 "\033[1;35m"		 // Magenta brillante
#define COLOR_PLAYER_5 "\033[1;36m"		 // Cian brillante
#define COLOR_PLAYER_6 "\033[1;33m"		 // Amarillo brillante
#define COLOR_PLAYER_7 "\033[1;37m"		 // Blanco brillante
#define COLOR_PLAYER_8 "\033[1;90m"		 // Gris brillante
#define COLOR_PLAYER_9 "\033[1;38;5;94m" // Marrón brillante
#define COLOR_RESET "\033[0m"

/**
 * @brief Obtiene el color correspondiente a un jugador
 * @param player_id ID del jugador (0-8)
 * @return Código de color ANSI para el jugador
 */
const char *get_player_color(int player_id);

/**
 * @brief Imprime el encabezado del juego
 */
void print_header(void);

/**
 * @brief Imprime la información de los jugadores
 */
void print_players_info(void);

/**
 * @brief Imprime el tablero del juego
 */
void print_board(void);

/**
 * @brief Imprime la leyenda del juego
 */
void print_legend(void);

/**
 * @brief Imprime la información del ganador
 */
void print_winner(void);

/**
 * @brief Imprime el estado completo del juego
 */
void print_game_state(void);

#endif // VIEW_FUNCTIONS_H