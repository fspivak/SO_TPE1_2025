#include "common.h"


void check_params(int argc, char * argv[]);

void close_up(int * sync_fd,int * state_fd, game_state_t **game_state,game_sync_t **game_sync);

int connect_shared_memories(int game_state_size, int game_sync_size, int * sync_fd,int * state_fd, game_state_t **game_state, game_sync_t **game_sync);

/**
 * @brief Obtiene un puntero a la celda del tablero
 */
// Para calcular offsets del tablero
static inline int *get_cell(game_state_t *state, int x, int y)
{
    return &state->board[y * state->width + x];
}

// Calcular desplazamiento según dirección
static inline void get_direction_offset(direction_t dir, int *dx, int *dy)
{
    static const int offsets[][2] = {
        {0, -1}, // UP
        {1, -1}, // UP_RIGHT
        {1, 0},  // RIGHT
        {1, 1},  // DOWN_RIGHT
        {0, 1},  // DOWN
        {-1, 1}, // DOWN_LEFT
        {-1, 0}, // LEFT
        {-1, -1} // UP_LEFT
    };
    *dx = offsets[dir][0];
    *dy = offsets[dir][1];
}