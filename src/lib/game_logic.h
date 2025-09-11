#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "common.h"

/**
 * @brief Espera en un semaforo con timeout
 * @param sem Puntero al semaforo
 * @param timeout_sec Tiempo de espera en segundos
 * @return 0 si se obtuvo el semaforo, -1 si hubo timeout o error
 */
int sem_wait_with_timeout(sem_t *sem, int timeout_sec);

/**
 * @brief Ejecuta un movimiento de un jugador
 * @param ctx Puntero al contexto del master
 * @param player_id ID del jugador
 * @param direction Direccion del movimiento
 */
void execute_player_move(master_context_t *ctx, int player_id, unsigned char direction);

/**
 * @brief Verifica si el juego ha terminado
 * @param ctx Puntero al contexto del master
 * @return true si el juego termino, false en caso contrario
 */
bool check_game_end(master_context_t *ctx);

/**
 * @brief Implementa el patron lectores-escritores sin inanicion para sincronizacion con view
 * @param ctx Puntero al contexto del master
 */
void sync_with_view(master_context_t *ctx);

#endif // GAME_LOGIC_H
