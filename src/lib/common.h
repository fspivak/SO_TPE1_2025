#ifndef COMMON_H
#define COMMON_H

#include <semaphore.h>
#include <stdbool.h>
#include <sys/types.h>

#define MAX_PLAYERS 9
#define MAX_NAME_LEN 16
#define GAME_STATE_SHM "/game_state"
#define GAME_SYNC_SHM "/game_sync"

#define VIEW_TIMEOUT_SEC 2
#define VIEW_CLEANUP_TIMEOUT_SEC 1
#define DEFAULT_BOARD_WIDTH 10
#define DEFAULT_BOARD_HEIGHT 10
#define DEFAULT_DELAY_MS 200
#define DEFAULT_TIMEOUT_SEC 10
#define MAX_CLEANUP_ATTEMPTS 10
#define CLEANUP_SLEEP_MS 100
#define FINAL_SYNC_SLEEP_MS 500
#define INIT_SYNC_SLEEP_MS 100
#define START_SLEEP_SEC 3

// Direcciones de movimiento (0-7, empezando por arriba y en sentido horario)
typedef enum {
	DIR_UP = 0,		// Arriba
	DIR_UP_RIGHT,	// Arriba-derecha
	DIR_RIGHT,		// Derecha
	DIR_DOWN_RIGHT, // Abajo-derecha
	DIR_DOWN,		// Abajo
	DIR_DOWN_LEFT,	// Abajo-izquierda
	DIR_LEFT,		// Izquierda
	DIR_UP_LEFT		// Arriba-izquierda
} direction_t;

// Estructura de un jugador
typedef struct {
	char name[MAX_NAME_LEN];	// Nombre del jugador
	unsigned int score;			// Puntaje
	unsigned int invalid_moves; // Cantidad de movimientos inválidos
	unsigned int valid_moves;	// Cantidad de movimientos válidos
	unsigned short x, y;		// Coordenadas x e y en el tablero
	pid_t pid;					// Identificador de proceso
	bool is_blocked;			// Indica si el jugador está bloqueado
} player_t;

// Estado del juego
typedef struct {
	unsigned short width;		   // Ancho del tablero
	unsigned short height;		   // Alto del tablero
	unsigned int player_count;	   // Cantidad de jugadores
	player_t players[MAX_PLAYERS]; // Lista de jugadores
	bool game_finished;			   // Indica si el juego se ha terminado
	int board[];				   // Tablero (flexible array member)
} game_state_t;

// Estructura de sincronización
typedef struct {
	sem_t view_ready;				// Master indica a vista que hay cambios (A)
	sem_t view_done;				// Vista indica a master que terminó (B)
	sem_t reader_writer_mutex;		// Mutex para evitar inanición del master (C)
	sem_t state_mutex;				// Mutex para el estado del juego (D)
	sem_t reader_count_mutex;		// Mutex para reader_count (E)
	unsigned int reader_count;		// Cantidad de jugadores leyendo estado (F)
	sem_t player_turn[MAX_PLAYERS]; // Semáforos para cada jugador (G)
} game_sync_t;

// Configuración del master
typedef struct {
	int width;			 // Ancho del tablero
	int height;			 // Alto del tablero
	int delay;			 // Retardo entre movimientos (ms)
	int timeout;		 // Tiempo de espera para la vista
	unsigned int seed;	 // Semilla para la generación de números aleatorios
	char *view_path;	 // Ruta de la vista
	char **player_paths; // Rutas de los ejecutables de los jugadores
	int player_count;	 // Cantidad de jugadores
} master_config_t;

// Contexto del master - contiene todas las variables globales
typedef struct {
	game_state_t *game_state; // Estado del juego
	game_sync_t *game_sync;	  // Estructura de sincronización
	int state_fd;			  // Descriptor de memoria compartida del estado
	int sync_fd;			  // Descriptor de memoria compartida de sincronización
	pid_t *player_pids;		  // Array de PIDs de jugadores
	pid_t view_pid;			  // PID del proceso de vista
	int *player_pipes;		  // Array de pipes para comunicación con jugadores
	master_config_t config;	  // Configuración del master
	bool cleanup_done;		  // Flag de limpieza completada
	bool view_active;		  // Flag de vista activa
} master_context_t;

// Contexto del view - contiene las variables globales del view
typedef struct {
	game_state_t *game_state; // Estado del juego
	game_sync_t *game_sync;	  // Estructura de sincronización
	int state_fd;			  // Descriptor de memoria compartida del estado
	int sync_fd;			  // Descriptor de memoria compartida de sincronización
} view_context_t;

// Contexto del player - contiene las variables globales del player
typedef struct {
	game_state_t *game_state; // Estado del juego
	game_sync_t *game_sync;	  // Estructura de sincronización
	int state_fd;			  // Descriptor de memoria compartida del estado
	int sync_fd;			  // Descriptor de memoria compartida de sincronización
	int player_id;			  // ID del jugador
} player_context_t;

#endif // COMMON_H