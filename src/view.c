#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static game_state_t *game_state = NULL;
static game_sync_t *game_sync =NULL;
static int state_fd = -1, sync_fd=-1;


//TODO: clean vista, entrar a la memoria compartida parwa escribir el tablero.

int init_shared_memory(size_t game_state_size, size_t game_sync_size){
    // open the shared memory for game state
    state_fd = shm_open(GAME_STATE_SHM,O_RDONLY,0);
    if(state_fd == -1){
        perror("Error oppening shared memory (state)");
        return -1;
    }


    //Mapping the shared memory
    game_state = mmap(NULL,game_state_size,PROT_READ,MAP_SHARED,state_fd,0);

    if(game_state == MAP_FAILED){
        perror("Error mapping the shared memory (state)");
        return -1;
    }


    sync_fd = shm_open(GAME_SYNC_SHM,O_RDWR,0);
    if(sync_fd == -1){
        perror("Error oppening shared memory (sync)");
        return -1;
    }

    

    game_sync = mmap(NULL,game_sync_size,PROT_WRITE | PROT_READ,MAP_SHARED,sync_fd,0);

    if(game_sync == MAP_FAILED){
        perror("Error mapping the shared memory (sync)");
        return -1;
    }

    return 0;
}

void close_up(){
    if(state_fd != NULL){
        munmap(game_state,sizeof(game_state_t)+game_state->width * game_state->height * sizeof(int));
    }
    if(sync_fd != NULL){
        munmap(game_sync,sizeof(game_sync));
    }

    if(state_fd != -1) close(state_fd);
    if(sync_fd != -1) close(sync_fd);
}

void print_game_state(){
    int width= &game_state->width;
    int height=&game_state->height;
    int players_count=&game_state->player_count;
    const player_t *players_array=&game_state->players;

    printf("===ChompChamps=== \n Board: %d x %d", width, height);

    for(int i=0; i< players_count; i++){
        printf("Player %d: points: | validMoves: %d | invallidMoves: %d | status: %s \n", i, players_array[i].valid_moves, players_array[i].invalid_moves, (players_array[i].is_blocked? "Blocked" : "Free"));
    }
    printf("GAME:\n");
    for(int i=0; i< width; i++){
        printf("|");
        for(int j=0; j< height; j++){
            printf(" %d |", &game_state->board[j*(i+1)]);
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]){

    if(argc!=3){
        fprintf(stderr,"Error calling view");
        exit(EXIT_FAILURE);
    }

    int width =atoi(argv[1]);
    int height =atoi(argv[2]);
    size_t game_state_size = sizeof(game_state_t) + width * height * sizeof(int);
    size_t game_sync_size = sizeof(game_sync_t);

    if(init_shared_memory(game_state_size, game_sync_size)!=0){
        fprintf(stderr, "Error to initialize shared memory view");
        exit(EXIT_FAILURE);
    }

    atexit(close_up);


    while(1){
        if(sem_wait(&game_sync->view_ready)!=0){
            perror("Error recieving signal from Master");
            break;
        }
            
        print_game_state();
        
        if(&game_state->game_finished){
            printf("GAME FINISHED!!!");
            sem_post(&game_sync->view_done);
            break;
        }

        if(sem_post(&game_sync->view_done)!=0){
            perror("Error sending signal to Master");
            break;
        }
    }

    printf("View finishing... \n");
    return EXIT_SUCCESS;
}