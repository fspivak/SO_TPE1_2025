#ifndef PROCESS_MANAGEMENT_H
#define PROCESS_MANAGEMENT_H

#include "common.h"

/**
 * @brief Verifica si un proceso esta vivo
 * @param pid ID del proceso a verificar
 * @return true si el proceso esta vivo, false en caso contrario
 */
bool is_process_alive(pid_t pid);

/**
 * @brief Limpia el proceso de view de forma segura
 * @param ctx Puntero al contexto del master
 * @details Verifica si ya termino, envia SIGTERM y finalmente SIGKILL si es necesario. Previene procesos zombie.
 */
void cleanup_view_process(master_context_t *ctx);

/**
 * @brief Limpia todos los recursos del master
 * @param ctx Puntero al contexto del master
 */
void master_cleanup(master_context_t *ctx);

/**
 * @brief Crea los procesos de los jugadores y la vista
 * @param ctx Puntero al contexto del master
 * @return 0 si la creacion fue exitosa, -1 en caso de error
 */
int create_processes(master_context_t *ctx);

#endif // PROCESS_MANAGEMENT_H
