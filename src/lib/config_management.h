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

#endif // CONFIG_MANAGEMENT_H
