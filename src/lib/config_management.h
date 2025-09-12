#ifndef CONFIG_MANAGEMENT_H
#define CONFIG_MANAGEMENT_H

#include "common.h"

/**
 * @brief Parsea los argumentos de linea de comandos y llena la estructura de master_config_t
 * @param argc Cantidad de argumentos
 * @param argv Array de argumentos
 * @param config Puntero a la estructura de configuracion
 */
void parse_arguments(int argc, char *argv[], master_config_t *config);

/**
 * @brief Imprime los resultados finales del juego
 * @param ctx Puntero al contexto del master
 */
void print_final_results(master_context_t *ctx);

/**
 * @brief Muestra los parametros de configuracion del juego
 * @param config Configuracion del juego
 */
void display_game_parameters(const master_config_t *config);

/**
 * @brief Muestra informacion de los procesos creados
 * @param config Configuracion del juego
 * @param player_pids Array de PIDs de jugadores
 * @param view_pid PID del proceso view
 * @param view_active Si la view esta activa
 */
void display_processes_info(const master_config_t *config, const pid_t *player_pids, pid_t view_pid, bool view_active);

/**
 * @brief Muestra el mensaje de inicio del juego
 */
void display_game_start(void);

#endif // CONFIG_MANAGEMENT_H
