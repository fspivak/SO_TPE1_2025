#include "common.h"


void check_params(int argc, int width, int height);

void * close_up(int * sync_fd,int * state_fd, game_state_t *game_state,game_sync_t *game_sync);

int connect_shared_memories(int game_state_size, int game_sync_size, int * sync_fd,int * state_fd, game_state_t *game_state, game_sync_t *game_sync);