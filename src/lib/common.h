#ifndef COMMON_H
#define COMMON_H

#include <semaphore.h>
#include <stdbool.h>
#include <sys/types.h>

#define MAX_PLAYERS 9
#define MAX_NAME_LEN 16
#define GAME_STATE_SHM "/game_state"
#define GAME_SYNC_SHM "/game_sync"

// Direcciones de movimiento (0-7, empezando por arriba y en sentido horario)
typedef enum {
    DIR_UP = 0,     // Arriba
    DIR_UP_RIGHT,   // Arriba-derecha
    DIR_RIGHT,      // Derecha
    DIR_DOWN_RIGHT, // Abajo-derecha
    DIR_DOWN,       // Abajo
    DIR_DOWN_LEFT,  // Abajo-izquierda
    DIR_LEFT,       // Izquierda
    DIR_UP_LEFT     // Arriba-izquierda
} direction_t;

// Estructura de un jugador
typedef struct {
    char name[MAX_NAME_LEN];    // Nombre del jugador
    unsigned int score;         // Puntaje
    unsigned int invalid_moves; // Cantidad de movimientos inválidos
    unsigned int valid_moves;   // Cantidad de movimientos válidos
    unsigned short x, y;        // Coordenadas x e y en el tablero
    pid_t pid;                  // Identificador de proceso
    bool is_blocked;            // Indica si el jugador está bloqueado
} player_t;

// Estado del juego
typedef struct {
    unsigned short width;          // Ancho del tablero
    unsigned short height;         // Alto del tablero
    unsigned int player_count;     // Cantidad de jugadores
    player_t players[MAX_PLAYERS]; // Lista de jugadores
    bool game_finished;            // Indica si el juego se ha terminado
    int board[];                   // Tablero (flexible array member)
} game_state_t;

// Estructura de sincronización
typedef struct {
    sem_t view_ready;               // Master indica a vista que hay cambios (A)
    sem_t view_done;                // Vista indica a master que terminó (B)
    sem_t reader_writer_mutex;      // Mutex para evitar inanición del master (C)
    sem_t state_mutex;              // Mutex para el estado del juego (D)
    sem_t reader_count_mutex;       // Mutex para reader_count (E)
    unsigned int reader_count;      // Cantidad de jugadores leyendo estado (F)
    sem_t player_turn[MAX_PLAYERS]; // Semáforos para cada jugador (G)
} game_sync_t;

#endif // COMMON_H