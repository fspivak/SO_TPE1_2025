#include "lib/common.h"
#include "lib/library.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <semaphore.h> 

static game_state_t *game_state = NULL;
static game_sync_t *game_sync = NULL;
static int state_fd = -1, sync_fd = -1;



// TODO: clean vista, entrar a la memoria compartida parwa escribir el tablero.

// int init_shared_memory(size_t game_state_size, size_t game_sync_size){
//     // open the shared memory for game state
//     state_fd = shm_open(GAME_STATE_SHM,O_RDONLY,0);
//     if(state_fd == -1){
//         perror("Error oppening shared memory (state)");
//         return -1;
//     }

//     //Mapping the shared memory
//     game_state = mmap(NULL,game_state_size,PROT_READ,MAP_SHARED,state_fd,0);

//     if(game_state == MAP_FAILED){
//         perror("Error mapping the shared memory (state)");
//         return -1;
//     }

//     sync_fd = shm_open(GAME_SYNC_SHM,O_RDWR,0);
//     if(sync_fd == -1){
//         perror("Error oppening shared memory (sync)");
//         return -1;
//     }

//     game_sync = mmap(NULL,game_sync_size,PROT_WRITE | PROT_READ,MAP_SHARED,sync_fd,0);

//     if(game_sync == MAP_FAILED){
//         perror("Error mapping the shared memory (sync)");
//         return -1;
//     }

//     return 0;
// }

void wraper_close_up()
{
    close_up(&sync_fd, &state_fd, &game_state, &game_sync);
}

void view_signal_handler(int sig)
{
    printf("View received signal %d, cleaning up resources...\n", sig);
    wraper_close_up();
    exit(EXIT_FAILURE);
}

void print_game_state()
{
    unsigned short width = game_state->width;
    unsigned short height = game_state->height;
    unsigned short players_count = game_state->player_count;
    const player_t *players_array = game_state->players;

    printf("\033c"); // Clear terminal
    printf("===ChompChamps=== \n Board: %d x %d", width, height);

    printf("\n\nllego\n\n");

    for (int i = 0; i < players_count; i++)
    {
        printf("primer print");
        printf("Player %d: points: | validMoves: %d | invallidMoves: %d | status: %s \n", i, players_array[i].valid_moves, players_array[i].invalid_moves, (players_array[i].is_blocked ? "Blocked" : "Free"));
    }

    printf("GAME:\n");
    for (int i = 0; i < width; i++)
    {
        printf("|");
        for (int j = 0; j < height; j++)
        {
            printf(" %d |", *get_cell(game_state,j,i));
        }
        printf("\n");
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, view_signal_handler);
    signal(SIGTERM, view_signal_handler);

    check_params(argc, argv);

    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    size_t game_state_size = sizeof(game_state_t) + width * height * sizeof(int);
    size_t game_sync_size = sizeof(game_sync_t);

    if (connect_shared_memories(game_state_size, game_sync_size, &sync_fd, &state_fd, &game_state, &game_sync))
    {
        fprintf(stderr, "Error to initialize shared memory view");
        exit(EXIT_FAILURE);
    }

    atexit(wraper_close_up);

    while (1)
    {
        // if(sem_wait(&game_sync->view_ready)!=0){
        //     perror("Error recieving signal from Master");
        //     break;
        // }
        sem_wait(&game_sync->view_ready);

        // Imprimir estado del juego
        print_game_state();

        // Notificar al master que terminamos de imprimir
        sem_post(&game_sync->view_done);

        // Si el juego terminó, salir
        if (game_state->game_finished) {
            printf("View: Game over, closing...\n");
            break;
        }






        //////////////////////////////////////////////////7
        // if (sem_wait(&game_sync->view_ready) != 0)
        // {
        //     perror("Error recieving signal from Master");
        //     break;
        // }

        // print_game_state();

        // if (game_state->game_finished)
        // {
        //     printf("GAME FINISHED!!!");
        //     sem_post(&game_sync->view_done);
        //     break;
        // }

        // if (sem_post(&(game_sync->view_done)) != 0)
        // {
        //     perror("Error sending signal to Master");
        //     break;
        // }
    }

    printf("View finishing... \n");
    wraper_close_up();
    return EXIT_SUCCESS;
}