#ifndef PLAYER_FUNCTIONS_H
#define PLAYER_FUNCTIONS_H

#include "common.h"
#include <stdbool.h>

/**
 * @brief Encuentra el ID del jugador actual basado en su PID
 * @return ID del jugador o -1 si no se encuentra
 */
int find_my_player_id(void);

/**
 * @brief Entra en estado de lectura usando patrón readers-writers
 */
void enter_read_state(void);

/**
 * @brief Sale del estado de lectura
 */
void exit_read_state(void);

/**
 * @brief Verifica si un movimiento es válido para un jugador
 * @param player_id ID del jugador
 * @param direction Dirección del movimiento
 * @return true si el movimiento es válido, false en caso contrario
 */
bool is_valid_move(int player_id, direction_t direction);

/**
 * @brief Elige un movimiento estratégico
 * @return Dirección del movimiento elegido
 */
direction_t choose_strategic_move(void);

/**
 * @brief Envía un movimiento al master
 * @param move Movimiento a enviar
 */
void send_move(direction_t move);

#endif // PLAYER_FUNCTIONS_H