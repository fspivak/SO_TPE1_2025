#ifndef MEMORY_MANAGEMENT_H
#define MEMORY_MANAGEMENT_H

#include "common.h"

/**
 * @brief Crea y mapea las memorias compartidas para el estado del juego y la sincronizacion
 * @param ctx Puntero al contexto del master
 * @return 0 si la creacion y el mapeo fueron exitosos, -1 en caso de error
 */
int create_shared_memories(master_context_t *ctx);

/**
 * @brief Inicializa el estado del juego
 * @param ctx Puntero al contexto del master
 */
void initialize_game_state(master_context_t *ctx);

/**
 * @brief Inicializa la estructura de sincronizacion
 * @param ctx Puntero al contexto del master
 */
void initialize_synchronization(master_context_t *ctx);

/**
 * @brief Posiciona jugadores en el tablero
 * @param ctx Puntero al contexto del master
 * @param player_id ID del jugador a posicionar
 */
void position_player_at_start(master_context_t *ctx, int player_id);

#endif // MEMORY_MANAGEMENT_H
