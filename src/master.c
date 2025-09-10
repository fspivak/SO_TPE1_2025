// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com
#define _GNU_SOURCE
#include "lib/common.h"
#include "lib/library.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define VIEW_TIMEOUT_SEC 2
#define VIEW_CLEANUP_TIMEOUT_SEC 1

typedef struct {
    int width;
    int height;
    int delay;
    int timeout;
    unsigned int seed;
    char *view_path;
    char **player_paths;
    int player_count;
} master_config_t;

// Variables globales
game_state_t *game_state = NULL;
game_sync_t *game_sync = NULL;
int state_fd = -1, sync_fd = -1;
pid_t *player_pids = NULL;
pid_t view_pid = -1;
int *player_pipes = NULL;
master_config_t config = {0};
bool cleanup_done = false;
bool view_active = true; // Flag para controlar si view está activa

// Función para verificar si un proceso está vivo
bool is_process_alive(pid_t pid) {
    if (pid <= 0) return false;
    return kill(pid, 0) == 0;
}

// Función para esperar con timeout usando sem_timedwait
int sem_wait_with_timeout(sem_t *sem, int timeout_sec) {
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += timeout_sec;

    int result = sem_timedwait(sem, &timeout);
    if (result == -1 && errno == ETIMEDOUT) {
        printf("Warning: Semaphore timeout after %d seconds\n", timeout_sec);
        return -1;
    }
    return result;
}

// Función para cleanup agresivo de view
void cleanup_view_process(void) {
    if (view_pid <= 0) return;

    printf("Cleaning up view process (PID: %d)\n", view_pid);

    // 1. Verificar si ya terminó
    int status;
    pid_t result = waitpid(view_pid, &status, WNOHANG);
    if (result > 0) {
        printf("View already terminated\n");
        view_pid = -1;
        return;
    }

    // 2. Terminación educada con SIGTERM
    if (is_process_alive(view_pid)) {
        kill(view_pid, SIGTERM);

        // Esperar con timeout
        for (int i = 0; i < 10 && is_process_alive(view_pid); i++) {
            usleep(100000); // 100ms
        }
    }

    // 3. Si aún está vivo, terminación forzada
    if (is_process_alive(view_pid)) {
        printf("View not responding to SIGTERM, using SIGKILL\n");
        kill(view_pid, SIGKILL);
        waitpid(view_pid, &status, 0);
    } else {
        // Recoger el proceso si terminó
        waitpid(view_pid, &status, WNOHANG);
    }

    if (WIFEXITED(status)) {
        printf("View exited (%d)\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("View terminated by signal %d\n", WTERMSIG(status));
    }

    view_pid = -1;
    view_active = false;
}

// Función para cleanup completo
void master_cleanup(void) {
    if (player_pids != NULL) {
        for (int i = 0; i < config.player_count; i++) {
            if (player_pids[i] > 0) {
                kill(player_pids[i], SIGTERM);
                waitpid(player_pids[i], NULL, 0);
            }
        }
        free(player_pids);
        player_pids = NULL;
    }

    if (player_pipes != NULL) {
        for (int i = 0; i < config.player_count; i++) {
            if (player_pipes[i] >= 0) {
                close(player_pipes[i]);
                player_pipes[i] = -1; // Marcar como cerrado
            }
        }
        free(player_pipes);
        player_pipes = NULL;
    }

    if (sync_fd >= 0) {
        close(sync_fd);
        sync_fd = -1;
    }

    if (state_fd >= 0) {
        close(state_fd);
        state_fd = -1;
    }

    if (game_state != NULL) {
        munmap(game_state, sizeof(game_state_t) + config.width * config.height * sizeof(int));
        game_state = NULL;
    }

    if (game_sync != NULL) {
        munmap(game_sync, sizeof(game_sync_t));
        game_sync = NULL;
    }

    shm_unlink(GAME_STATE_SHM);
    shm_unlink(GAME_SYNC_SHM);

    if (config.player_paths != NULL) {
        // NO liberar config.player_paths[i] porque apuntan a argv (no son malloc'd)
        // Solo liberar el array de punteros
        free(config.player_paths);
        config.player_paths = NULL;
    }

    printf("Master cleanup completed\n");
}

// wrapper
void master_close_up(void) { master_cleanup(); }

// wrapper
void master_signal_wrapper(int sig) {
    printf("Master received signal %d\n", sig);
    generic_signal_handler(sig, "Master", -1, master_close_up);
}

void parse_arguments(int argc, char *argv[], master_config_t *config) {
    // Valores por defecto
    config->width = 10;
    config->height = 10;
    config->delay = 200;
    config->timeout = 10;
    config->seed = time(NULL);
    config->view_path = NULL;
    config->player_paths = NULL;
    config->player_count = 0;

    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            config->width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            config->height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            config->delay = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            config->timeout = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            config->seed = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0 && i + 1 < argc) {
            config->view_path = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0) {
            // Contar jugadores que siguen después de -p
            int j = i + 1;
            while (j < argc && argv[j][0] != '-') {
                config->player_count++;
                j++;
            }

            if (config->player_count == 0 || config->player_count > MAX_PLAYERS) {
                fprintf(stderr, "Error: Must specify 1-%d players\n", MAX_PLAYERS);
                exit(EXIT_FAILURE);
            }

            // Allocar memoria para paths de jugadores
            config->player_paths = malloc(config->player_count * sizeof(char *));
            if (config->player_paths == NULL) {
                fprintf(stderr, "Error: Failed to allocate memory for player paths\n");
                exit(EXIT_FAILURE);
            }
            for (int k = 0; k < config->player_count; k++) {
                config->player_paths[k] = argv[i + 1 + k];
            }

            // Avanzar el índice correctamente
            i = j - 1; // j-1 porque el loop hará i++ al final
        }
        i++;
    }

    if (config->player_count == 0) {
        fprintf(stderr, "Error: At least one player must be specified with -p\n");
        exit(EXIT_FAILURE);
    }

    // Validación de datos
    if (config->width <= 0 || config->height <= 0) {
        fprintf(stderr, "Error: Width and height must be positive\n");
        exit(EXIT_FAILURE);
    }
    if (config->delay < 0) {
        fprintf(stderr, "Error: Delay must be non-negative\n");
        exit(EXIT_FAILURE);
    }
    if (config->player_count <= 0 || config->player_count > MAX_PLAYERS) {
        fprintf(stderr, "Error: Invalid number of players (1-%d allowed)\n", MAX_PLAYERS);
        exit(EXIT_FAILURE);
    }
}

int create_shared_memories(void) {
    size_t state_size = sizeof(game_state_t) + config.width * config.height * sizeof(int);
    size_t sync_size = sizeof(game_sync_t);

    // Crear memoria compartida para el estado del juego
    state_fd = shm_open(GAME_STATE_SHM, O_CREAT | O_RDWR | O_EXCL, 0666);
    if (state_fd == -1) {
        perror("Error creating game state shared memory");
        return -1;
    }

    if (ftruncate(state_fd, state_size) == -1) {
        perror("Error setting game state size");
        close(state_fd);
        shm_unlink(GAME_STATE_SHM);
        return -1;
    }

    game_state = mmap(NULL, state_size, PROT_READ | PROT_WRITE, MAP_SHARED, state_fd, 0);
    if (game_state == MAP_FAILED) {
        perror("Error mapping game state");
        close(state_fd);
        shm_unlink(GAME_STATE_SHM);
        return -1;
    }

    // Crear memoria compartida para sincronización
    sync_fd = shm_open(GAME_SYNC_SHM, O_CREAT | O_RDWR | O_EXCL, 0666);
    if (sync_fd == -1) {
        perror("Error creating game sync shared memory");
        munmap(game_state, state_size);
        close(state_fd);
        shm_unlink(GAME_STATE_SHM);
        return -1;
    }

    if (ftruncate(sync_fd, sync_size) == -1) {
        perror("Error setting game sync size");
        munmap(game_state, state_size);
        close(state_fd);
        shm_unlink(GAME_STATE_SHM);
        close(sync_fd);
        shm_unlink(GAME_SYNC_SHM);
        return -1;
    }

    game_sync = mmap(NULL, sync_size, PROT_READ | PROT_WRITE, MAP_SHARED, sync_fd, 0);
    if (game_sync == MAP_FAILED) {
        perror("Error mapping game sync");
        munmap(game_state, state_size);
        close(state_fd);
        shm_unlink(GAME_STATE_SHM);
        close(sync_fd);
        shm_unlink(GAME_SYNC_SHM);
        return -1;
    }

    return 0;
}

void initialize_game_state(void) {
    memset(game_state, 0, sizeof(game_state_t) + config.width * config.height * sizeof(int));

    game_state->width = config.width;
    game_state->height = config.height;
    game_state->player_count = config.player_count;
    game_state->game_finished = false;

    // Verificar que player_paths esté inicializado
    if (config.player_paths == NULL) {
        fprintf(stderr, "Error: player_paths not initialized\n");
        exit(EXIT_FAILURE);
    }

    srand(config.seed);

    // Inicializar tablero con recompensas aleatorias (1-9)
    for (int i = 0; i < config.width * config.height; i++) {
        game_state->board[i] = (rand() % 9) + 1;
    }

    // Posicionar jugadores de forma determinística
    for (int i = 0; i < config.player_count; i++) {
        // Extraer el nombre del archivo del path completo
        char *player_name = strrchr(config.player_paths[i], '/');
        if (player_name) {
            player_name++; // Saltar el '/'
        } else {
            player_name = config.player_paths[i]; // Si no hay '/', usar todo el path
        }

        snprintf(game_state->players[i].name, MAX_NAME_LEN, "%s", player_name);
        game_state->players[i].score = 0;
        game_state->players[i].valid_moves = 0;
        game_state->players[i].invalid_moves = 0;
        game_state->players[i].is_blocked = false;
        game_state->players[i].pid = 0;

        // Distribución simple: esquinas y bordes
        switch (i) {
        case 0:
            game_state->players[i].x = 0;
            game_state->players[i].y = 0;
            break;
        case 1:
            game_state->players[i].x = config.width - 1;
            game_state->players[i].y = 0;
            break;
        case 2:
            game_state->players[i].x = 0;
            game_state->players[i].y = config.height - 1;
            break;
        case 3:
            game_state->players[i].x = config.width - 1;
            game_state->players[i].y = config.height - 1;
            break;
        default: {
            int side = (i - 4) % 4;
            int pos = (i - 4) / 4 + 1;
            switch (side) {
            case 0:
                game_state->players[i].x = pos * config.width / (config.player_count - 3);
                game_state->players[i].y = 0;
                break;
            case 1:
                game_state->players[i].x = config.width - 1;
                game_state->players[i].y = pos * config.height / (config.player_count - 3);
                break;
            case 2:
                game_state->players[i].x = config.width - 1 - pos * config.width / (config.player_count - 3);
                game_state->players[i].y = config.height - 1;
                break;
            case 3:
                game_state->players[i].x = 0;
                game_state->players[i].y = config.height - 1 - pos * config.height / (config.player_count - 3);
                break;
            }
            break;
        }
        }

        // Marcar celda como ocupada
        int *cell = get_cell(game_state, game_state->players[i].x, game_state->players[i].y);
        *cell = -(i);
        printf("DEBUG: Player %d at (%d,%d), cell value = %d\n", i, game_state->players[i].x, game_state->players[i].y,
               *cell);
    }
}

void initialize_synchronization(void) {
    sem_init(&game_sync->view_ready, 1, 0);
    sem_init(&game_sync->view_done, 1, 0);
    sem_init(&game_sync->reader_writer_mutex, 1, 1);
    sem_init(&game_sync->state_mutex, 1, 1);
    sem_init(&game_sync->reader_count_mutex, 1, 1);
    game_sync->reader_count = 0;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        sem_init(&game_sync->player_turn[i], 1, 0);
    }

    for (int i = 0; i < config.player_count; i++) {
        sem_post(&game_sync->player_turn[i]);
    }
}

int create_processes(void) {
    player_pids = calloc(config.player_count, sizeof(pid_t));
    player_pipes = malloc(config.player_count * sizeof(int));

    char width_str[16], height_str[16];
    snprintf(width_str, sizeof(width_str), "%d", config.width);
    snprintf(height_str, sizeof(height_str), "%d", config.height);

    // Crear procesos de jugadores
    for (int i = 0; i < config.player_count; i++) {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            perror("Error creating pipe for player");
            return -1;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("Error forking player process");
            return -1;
        } else if (pid == 0) {
            // Proceso hijo (jugador)
            close(pipefd[0]);

            if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                perror("Error redirecting stdout in player");
                exit(EXIT_FAILURE);
            }
            close(pipefd[1]);

            execl(config.player_paths[i], config.player_paths[i], width_str, height_str, NULL);
            perror("Error executing player program");
            exit(EXIT_FAILURE);
        } else {
            close(pipefd[1]);
            player_pids[i] = pid;
            player_pipes[i] = pipefd[0];
            game_state->players[i].pid = pid;
        }
    }

    // Crear proceso de vista si se especificó
    if (config.view_path != NULL) {
        view_pid = fork();
        if (view_pid == -1) {
            perror("Error forking view process");
            printf("Warning: Continuing without view\n");
            config.view_path = NULL;
            view_active = false;
            return 0;
        } else if (view_pid == 0) {
            execl(config.view_path, config.view_path, width_str, height_str, NULL);
            perror("Error executing view program");
            exit(EXIT_FAILURE);
        } else {
            printf("View process created (PID: %d)\n", view_pid);
            view_active = true;
        }
    }

    return 0;
}

bool is_valid_player_move(int player_id, unsigned char direction) {
    if (direction > 7) return false;

    player_t *player = &game_state->players[player_id];
    int dx, dy;
    get_direction_offset((direction_t) direction, &dx, &dy);

    int new_x = player->x + dx;
    int new_y = player->y + dy;

    if (new_x < 0 || new_y < 0 || new_x >= config.width || new_y >= config.height) {
        return false;
    }

    int *cell = get_cell(game_state, new_x, new_y);
    return (*cell > 0);
}

void execute_player_move(int player_id, unsigned char direction) {
    player_t *player = &game_state->players[player_id];
    int dx, dy;
    get_direction_offset((direction_t) direction, &dx, &dy);

    int new_x = player->x + dx;
    int new_y = player->y + dy;

    int *cell = get_cell(game_state, new_x, new_y);
    int reward = *cell;

    player->x = new_x;
    player->y = new_y;
    player->score += reward;
    player->valid_moves++;

    *cell = -(player_id);
}

bool check_game_end(void) {
    bool any_active = false;

    for (int player_id = 0; player_id < config.player_count; player_id++) {
        if (game_state->players[player_id].is_blocked) continue;

        bool has_valid_moves = false;
        for (unsigned char dir = 0; dir < 8; dir++) {
            if (is_valid_player_move(player_id, dir)) {
                has_valid_moves = true;
                any_active = true;
                break;
            }
        }

        if (!has_valid_moves && !game_state->players[player_id].is_blocked) {
            // Jugador se bloqueó, marcarlo pero darle tiempo para que termine naturalmente
            game_state->players[player_id].is_blocked = true;
            // NO cerramos el pipe aquí - el jugador necesita leer el estado is_blocked
        }
    }

    return !any_active;
}

// Función mejorada para sincronizar con view
void sync_with_view(void) {
    if (!view_active || config.view_path == NULL) return;

    // Verificar si view sigue viva antes de cualquier operación
    if (!is_process_alive(view_pid)) {
        printf("View process died, disabling view\n");
        view_active = false;
        return;
    }

    // Notificar a view con verificación post-signal
    sem_post(&game_sync->view_ready);

    // Verificación adicional inmediatamente después del post
    if (!is_process_alive(view_pid)) {
        printf("View died immediately after signal, disabling view\n");
        view_active = false;
        // Intentar drenar el semáforo que acabamos de postear
        struct timespec immediate_timeout = {0, 1000000}; // 1ms
        sem_timedwait(&game_sync->view_done, &immediate_timeout);
        return;
    }

    // Esperar respuesta con timeout
    if (sem_wait_with_timeout(&game_sync->view_done, VIEW_TIMEOUT_SEC) == -1) {
        printf("View timeout - disabling view for rest of game\n");
        view_active = false;

        // Verificación final para determinar si murió durante la espera
        if (!is_process_alive(view_pid)) {
            printf("View died during timeout wait\n");
        }
        return;
    }
}

void game_loop(void) {
    fd_set readfds;
    struct timeval timeout_tv;
    time_t last_valid_move = time(NULL);
    int current_player = 0;

    printf("Starting game loop\n");

    // Sincronización inicial con view
    if (view_active && config.view_path != NULL) {
        printf("Initial sync with view\n");
        sync_with_view();
    }

    while (!game_state->game_finished) {
        FD_ZERO(&readfds);
        int max_fd = 0;

        for (int i = 0; i < config.player_count; i++) {
            if (!game_state->players[i].is_blocked && player_pipes[i] != -1) {
                FD_SET(player_pipes[i], &readfds);
                if (player_pipes[i] > max_fd) {
                    max_fd = player_pipes[i];
                }
            }
        }

        timeout_tv.tv_sec = 1;
        timeout_tv.tv_usec = 0;

        int ready = select(max_fd + 1, &readfds, NULL, NULL, &timeout_tv);

        if (ready == -1) {
            if (errno == EINTR) continue;
            perror("Select error");
            break;
        }

        if (ready == 0) {
            if (difftime(time(NULL), last_valid_move) >= config.timeout) {
                printf("Timeout reached, ending game\n");
                break;
            }
            continue;
        }

        bool movement_processed = false;

        // Procesar movimientos de jugadores
        for (int attempts = 0; attempts < config.player_count && !movement_processed; attempts++) {
            int player_id = (current_player + attempts) % config.player_count;

            if (game_state->players[player_id].is_blocked || !FD_ISSET(player_pipes[player_id], &readfds)) {
                continue;
            }

            unsigned char move;
            ssize_t bytes_read = read(player_pipes[player_id], &move, 1);

            if (bytes_read <= 0) {
                game_state->players[player_id].is_blocked = true;
                if (player_pipes[player_id] != -1) {
                    close(player_pipes[player_id]);
                    player_pipes[player_id] = -1;
                }
                continue;
            }

            sem_wait(&game_sync->reader_writer_mutex);
            sem_wait(&game_sync->state_mutex);
            sem_post(&game_sync->reader_writer_mutex);

            if (is_valid_player_move(player_id, move)) {
                execute_player_move(player_id, move);
                last_valid_move = time(NULL);
            } else {
                game_state->players[player_id].invalid_moves++;
            }

            sem_post(&game_sync->state_mutex);
            sem_post(&game_sync->player_turn[player_id]);

            movement_processed = true;
            current_player = (player_id + 1) % config.player_count;
        }

        // Verificar fin de juego después de procesar movimientos
        if (check_game_end()) {
            printf("Game ended - no more valid moves available\n");
            game_state->game_finished = true;
            // Notificar a todos los jugadores para que despierten y salgan
            for (int i = 0; i < config.player_count; i++) {
                sem_post(&game_sync->player_turn[i]);
            }
            break;
        }

        // Sincronizar con view si hubo movimientos
        if (movement_processed) {
            sync_with_view();
            if (config.delay > 0) {
                usleep(config.delay * 1000);
            }
        }

        // Timeout global del juego
        if (time(NULL) - last_valid_move > config.timeout) {
            printf("Game timeout reached\n");
            game_state->game_finished = true;

            // Notificar a todos los jugadores para que despierten y salgan
            for (int i = 0; i < config.player_count; i++) {
                sem_post(&game_sync->player_turn[i]);
            }
            break;
        }
    }

    // Notificación final a view
    if (view_active && config.view_path != NULL) {
        printf("Final sync with view\n");
        sync_with_view();
        usleep(500000); // Dar tiempo para que view procese el estado final
    }

    printf("Game loop ended\n");
}

void print_final_results(void) {
    printf("\n=== FINAL RESULTS ===\n");

    // Ordenar jugadores por puntuación
    int sorted_indices[MAX_PLAYERS];
    for (int i = 0; i < config.player_count; i++) {
        sorted_indices[i] = i;
    }

    // Ordenamiento burbuja simple
    for (int i = 0; i < config.player_count - 1; i++) {
        for (int j = 0; j < config.player_count - i - 1; j++) {
            if (game_state->players[sorted_indices[j]].score < game_state->players[sorted_indices[j + 1]].score ||
                (game_state->players[sorted_indices[j]].score == game_state->players[sorted_indices[j + 1]].score &&
                 game_state->players[sorted_indices[j]].invalid_moves >
                     game_state->players[sorted_indices[j + 1]].invalid_moves)) {
                int temp = sorted_indices[j];
                sorted_indices[j] = sorted_indices[j + 1];
                sorted_indices[j + 1] = temp;
            }
        }
    }

    // Imprimir resultados ordenados
    for (int i = 0; i < config.player_count; i++) {
        int idx = sorted_indices[i];
        printf("%d. %s: %u points (%u valid, %u invalid moves)\n", i + 1, game_state->players[idx].name,
               game_state->players[idx].score, game_state->players[idx].valid_moves,
               game_state->players[idx].invalid_moves);
    }

    printf("=====================\n");
}

int main(int argc, char *argv[]) {
    // Registrar cleanup automático para el master
    atexit(master_cleanup);
    
    // Instalar manejadores de señales
    signal(SIGINT, master_signal_wrapper); // Ctrl+C
    signal(SIGTERM, master_signal_wrapper); // kill
    signal(SIGCHLD, SIG_IGN); // Evitar procesos zombie

    printf("ChompChamps Master starting...\n");

    // Parsear argumentos
    parse_arguments(argc, argv, &config);

    printf("Game configuration:\n");
    printf("  Board: %dx%d\n", config.width, config.height);
    printf("  Delay: %dms\n", config.delay);
    printf("  Timeout: %ds\n", config.timeout);
    printf("  Seed: %u\n", config.seed);
    printf("  Players: %d\n", config.player_count);
    for (int i = 0; i < config.player_count; i++) {
        printf("    %d: %s\n", i, config.player_paths[i]);
    }
    if (config.view_path) {
        printf("  View: %s\n", config.view_path);
    }

    // Crear memoria compartida
    if (create_shared_memories() != 0) {
        fprintf(stderr, "Failed to create shared memories\n");
        exit(EXIT_FAILURE);
    }

    // Inicializar estado del juego
    initialize_game_state();

    // Inicializar sincronización
    initialize_synchronization();

    // Crear procesos
    if (create_processes() != 0) {
        fprintf(stderr, "Failed to create processes\n");
        master_cleanup();
        exit(EXIT_FAILURE);
    }

    printf("All processes created successfully\n");

    // Dar tiempo a los procesos para conectarse a la memoria compartida
    usleep(100000); // 100ms

    // Notificar a view que la memoria compartida está lista
    if (view_active && config.view_path != NULL) {
        sem_post(&game_sync->view_ready);
    }

    // Ejecutar bucle principal del juego
    game_loop();

    // Imprimir resultados finales
    print_final_results();

    // Cleanup
    master_cleanup();

    printf("ChompChamps Master finished\n");
    return EXIT_SUCCESS;
}