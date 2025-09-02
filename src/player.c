#define _GNU_SOURCE

#include "./lib/common.h"
#include "lib/library.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

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

static game_state_t *game_state = NULL;
static game_sync_t *game_sync = NULL;
static int state_fd = -1, sync_fd = -1, player_id = -1;

void player_signal_handler(int sig){
    fprintf(stderr, "Player %d received signal %d, cleaning up...\n", player_id + 1, sig);
    close_up(&sync_fd, &state_fd, &game_state, &game_sync);
    exit(EXIT_FAILURE);
}

int find_my_player_id(void){
    pid_t my_pid = getpid();
    for (unsigned int i = 0; i < game_state->player_count; i++){
        if (game_state->players[i].pid == my_pid){
            return (int)i;
        }
    }
    return -1;
}

void enter_read_state(void){
    // Readers-Writers para evitar inanición

    sem_wait(&game_sync->reader_writer_mutex); // Aseguro que no haya escritores esperando
    sem_wait(&game_sync->reader_count_mutex);  // Aseguro exclusión mutua para reader_count

    game_sync->reader_count++;
    if (game_sync->reader_count == 1){
        sem_wait(&game_sync->state_mutex); // Espero mi turno y obtengo el estado del juego. Cada jugador lo tendrá una vez por ronda
    }

    sem_post(&game_sync->reader_count_mutex);  // Libero exclusión mutua para reader_count
    sem_post(&game_sync->reader_writer_mutex); // Libero para que otros lectores puedan entrar
}

void exit_read_state(void){
    sem_wait(&game_sync->reader_count_mutex); // Aseguro exclusión mutua para reader_count

    game_sync->reader_count--;
    if (game_sync->reader_count == 0){
        sem_post(&game_sync->state_mutex); // Devuelvo el estado del juego al resto
    }

    sem_post(&game_sync->reader_count_mutex); // Libero exclusión mutua para reader_count
}

bool is_valid_move(int player_id, direction_t direction){
    player_t *player = &game_state->players[player_id];
    int dx, dy;
    // futura posicion del player    
    get_direction_offset(direction, &dx, &dy);
    int new_x = player->x + dx;
    int new_y = player->y + dy;

    // validacion de que la nueva posicion este dentro del tablero
    if (new_x < 0 || new_y < 0 || new_x >= game_state->width || game_state->height){
        return false;
    }
    // Haabría que validar que no este ocupado
    int* cell= get_cell(game_state, new_x,new_y);
    return (*cell >0);
}

direction_t choose_strategic_move(){
    // int move= randint(0,8);
    // direction_t direction = (direction_t)move;
    // while(!is_valid_move(player_id, direction)){
    // }

    return (direction_t)(rand() % 8);
}

void send_move(direction_t move){
    unsigned char move_byte = (unsigned char)move;
    ssize_t bytes_written = write(STDOUT_FILENO, &move_byte, 1);
    if (bytes_written != 1){
        perror("Error sending movement");
    }
}

int main(int argc, char *argv[]){

    signal(SIGINT, player_signal_handler);
    signal(SIGTERM, player_signal_handler);

    check_params(argc, argv);

    int width = atoi(argv[1]);
    int height = atoi(argv[2]);
    size_t game_state_size = sizeof(game_state_t) + width * height * sizeof(int);
    size_t game_sync_size = sizeof(game_sync_t);

    // generador de numeros aleatorios
    srand(time(NULL) + getpid());

    if (connect_shared_memories(game_state_size, game_sync_size, &sync_fd, &state_fd, &game_state, &game_sync) != 0)
    {
        close_up(&sync_fd, &state_fd, &game_state, &game_sync);
        exit(EXIT_FAILURE);
    }

    // Encontrar mi ID de jugador
    enter_read_state();
    player_id = find_my_player_id();
    exit_read_state();

    if (player_id == -1)
    {
        fprintf(stderr, "Error: Could not find player ID\n");
        close_up(&sync_fd, &state_fd, &game_state, &game_sync);
        return (EXIT_FAILURE);
    }

    fprintf(stderr, "Player %d started (PID: %d)\n", player_id + 1, getpid());

    // Loop principal del jugador
    while (true)
    {
        // Esperar mi turno
        sem_wait(&game_sync->player_turn[player_id]);

        // Leer el estado del juego de forma sincronizada
        enter_read_state();

        // Verificar si el juego terminó o estoy bloqueado
        if (game_state->game_finished || game_state->players[player_id].is_blocked)
        {
            exit_read_state();
            break;
        }

        // Elegir movimiento usando estrategia híbrida
        direction_t chosen_move = choose_strategic_move();

        exit_read_state();

        // Enviar movimiento al master
        send_move(chosen_move);
    }

    fprintf(stderr, "Player %d finished...\n", player_id + 1);
    close_up(&sync_fd, &state_fd, &game_state, &game_sync);
    return 0;
}
