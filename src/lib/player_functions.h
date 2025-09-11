#ifndef PLAYER_FUNCTIONS_H
#define PLAYER_FUNCTIONS_H

#include "common.h"
#include <stdbool.h>

/**
 * @brief Encuentra el ID del jugador actual basado en su PID
 * @param ctx Puntero al contexto del player
 * @return ID del jugador o -1 si no se encuentra
 */
int find_my_player_id(player_context_t *ctx);

/**
 * @brief Entra en estado de lectura usando patron readers-writers
 * @param ctx Puntero al contexto del player
 */
void enter_read_state(player_context_t *ctx);

/**
 * @brief Sale del estado de lectura
 * @param ctx Puntero al contexto del player
 */
void exit_read_state(player_context_t *ctx);

/**
 * @brief Envia un movimiento al master
 * @param move Movimiento a enviar
 */
void send_move(direction_t move);

/**
 * @brief Inicializa el contexto del player
 * @param ctx Puntero al contexto del player
 * @param argc Numero de argumentos
 * @param argv Array de argumentos
 */
void initialize_player_context(player_context_t *ctx, int argc, char *argv[]);

/**
 * @brief Bucle principal del player
 * @param ctx Puntero al contexto del player
 */
void player_main_loop(player_context_t *ctx);

#endif // PLAYER_FUNCTIONS_H