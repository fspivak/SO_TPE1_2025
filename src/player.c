#include "common.h"
#include "library.h"
#include <stdio.h>




static game_state_t *game_state = NULL;
static game_sync_t *game_sync =NULL;
static int state_fd = -1, sync_fd=-1, g_player_id= -1;




//player:
int main(int argc, char* argv[]){

    int width =atoi(argv[1]);
    int height =atoi(argv[2]);
    size_t game_state_size = sizeof(game_state_t) + width * height * sizeof(int);
    size_t game_sync_size = sizeof(game_sync_t);

    check_params(argc, width, height);

    connect_shared_memories(game_state_size,game_sync_size,&sync_fd, &state_fd, &game_state, &game_sync);


    close_up(&sync_fd,&state_fd,&game_state, &game_sync);


// while(1){
//     wait(writer)
//     post(writer)

//     wait(readers_count_mutex);
//     if(readers_count_mutex++ == 0) wait(mutex);
//     post(readers_count_mutex);



//     pedir_movimiento(...)
//     wait(mutex)
//     enviar_movimiento(...)
//     post(mutex)
// }
}