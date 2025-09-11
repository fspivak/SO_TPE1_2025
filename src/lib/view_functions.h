#ifndef VIEW_FUNCTIONS_H
#define VIEW_FUNCTIONS_H

#include "common.h"

/**
 * @brief Imprime el encabezado del juego
 * @param ctx Puntero al contexto del view
 */
void print_header(view_context_t *ctx);

/**
 * @brief Imprime la informacion de los jugadores
 * @param ctx Puntero al contexto del view
 */
void print_players_info(view_context_t *ctx);

// Las funciones auxiliares static estan definidas solo en view_functions.c

/**
 * @brief Imprime el tablero del juego
 * @param ctx Puntero al contexto del view
 */
void print_board(view_context_t *ctx);

/**
 * @brief Imprime la leyenda del juego
 * @param ctx Puntero al contexto del view
 */
void print_legend(view_context_t *ctx);

/**
 * @brief Imprime la informacion del ganador
 * @param ctx Puntero al contexto del view
 */
void print_winner(view_context_t *ctx);

/**
 * @brief Imprime el estado completo del juego
 * @param ctx Puntero al contexto del view
 */
void print_game_state(view_context_t *ctx);

/**
 * @brief Inicializa el contexto del view
 * @param ctx Puntero al contexto del view
 * @param argc Numero de argumentos
 * @param argv Array de argumentos
 */
void initialize_view_context(view_context_t *ctx, int argc, char *argv[]);

/**
 * @brief Bucle principal del view
 * @param ctx Puntero al contexto del view
 */
void view_main_loop(view_context_t *ctx);

#endif // VIEW_FUNCTIONS_H