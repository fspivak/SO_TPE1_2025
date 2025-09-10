#ifndef MASTER_FUNCTIONS_H
#define MASTER_FUNCTIONS_H

#include "common.h"

/**
 * @brief Espera en un semáforo con timeout
 * @param sem Puntero al semáforo
 * @param timeout_sec Tiempo de espera en segundos
 * @return 0 si se obtuvo el semáforo, -1 si hubo timeout o
 */
bool is_process_alive (pid_t pid);

/**
 * @brief Espera en un semáforo con timeout
 * @param sem Puntero al semáforo
 * @param timeout_sec Tiempo de espera en segundos
 * @return 0 si se obtuvo el semáforo, -1 si hubo timeout o error
 */
int sem_wait_with_timeout(sem_t *sem, int timeout_sec);

/**
 * @brief Limpia el proceso de view
 */
void cleanup_view_process(void);

/**
 * @brief Limpia todos los recursos del master
 */
void master_cleanup(void);

/**
 * @brief Parsea los argumentos de línea de comandos y llena la estructura de master_congig_t
 * @param argc Cantidad de argumentos
 * @param argv Array de argumentos
 * @param config Puntero a la estructura de configuración
 */
void parse_arguments(int argc, char *argv[], master_config_t *config);

/**
 * @brief Crea y mapea las memorias compartidas para el estado del juego y la sincronización
 * @return 0 si la creación y el mapeo fueron exitosos, -1 en caso de error
 */
int create_shared_memories(void);

/**
 * @brief Inicializa el estado del juego
 */
void initialize_game_state(void);

/**
 * @brief Inicializa la estructura de sincronización
 */
void initialize_synchronization(void);

/**
 * @brief Crea los procesos de los jugadores y la vista
 * @return 0 si la creación fue exitosa, -1 en caso de error
 */
int create_processes(void);

#endif // MASTER_FUNCTIONS_H