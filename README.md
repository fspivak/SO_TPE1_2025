# ChompChamps - Sistema de Procesos POSIX

## 🎯 Objetivo General del Trabajo

Entender y desarrollar un sistema de procesos que se comuniquen y sincronicen en un sistema POSIX. Este proyecto implementa un juego multijugador donde diferentes procesos (master, view, players) se comunican mediante:

- **Memoria Compartida (Shared Memory)**: Para el estado del juego y sincronización
- **Semáforos**: Para control de acceso y sincronización entre procesos
- **Pipes**: Para comunicación entre master y players
- **Señales**: Para manejo de terminación y cleanup

## 🎮 Descripción del Juego

ChompChamps es un juego multijugador donde:
- Hasta 9 jugadores compiten en un tablero de dimensiones configurables
- Cada jugador se mueve en 8 direcciones posibles (arriba, abajo, izquierda, derecha, diagonales)
- Los jugadores capturan celdas del tablero y compiten por el mayor puntaje
- El juego termina cuando no hay más movimientos válidos disponibles

## 🏗️ Arquitectura del Sistema

### Procesos Principales

1. **Master Process** (`master.c`)
   - Coordina el juego y maneja la lógica principal
   - Gestiona la memoria compartida y sincronización
   - Controla el flujo del juego y termina procesos

2. **View Process** (`view.c`)
   - Muestra el estado del juego en tiempo real
   - Se sincroniza con el master para actualizaciones
   - Implementa el patrón lectores-escritores

3. **Player Processes** (`player.c`)
   - Ejecutan la estrategia de cada jugador
   - Se comunican con el master via pipes
   - Acceden al estado del juego de forma sincronizada

### Estructuras de Datos

- **`player_t`**: Información individual de cada jugador (nombre, puntaje, posición, estado)
- **`game_state_t`**: Estado del juego (tablero, jugadores, puntajes, fin de juego)
- **`game_sync_t`**: Sincronización entre procesos (semáforos, contadores, mutex)
- **`master_config_t`**: Configuración del master (parámetros del juego)
- **`master_context_t`**: Contexto del proceso master (memoria compartida, PIDs, pipes)
- **`view_context_t`**: Contexto del proceso view (acceso a memoria compartida)
- **`player_context_t`**: Contexto del proceso player (acceso a memoria compartida, ID)

## 🛠️ Instrucciones de Compilación

### Requisitos del Sistema

- **Sistema Operativo**: Linux (POSIX compatible)
- **Compilador**: GCC con soporte C99
- **Librerías**: `librt` (real-time), `libpthread` (threads)

### Compilación Local

```bash
# Compilar todos los ejecutables
make all

# O compilar individualmente
make master    # Compila el proceso master
make player    # Compila el proceso player
make view      # Compila el proceso view

# Limpiar archivos compilados
make clean

# Formatear código (requiere clang-format)
make format
```

### Flags de Compilación

El proyecto se compila con las siguientes flags estrictas:
- `-Wall -Wextra`: Todas las advertencias habilitadas
- `-std=c99`: Estándar C99
- `-pthread`: Soporte para threads
- `-pedantic`: Cumplimiento estricto del estándar

## 🐳 Instrucciones de Compilación y Ejecución con Docker

### Usando la Imagen Oficial del ITBA

### Pasos Detallados

1. **Inicializar el contenedor**:
   ```bash
   docker run -v "${PWD}:/root" --privileged -ti --name SO-TPE1 agodio/itba-so-multi-platform:3.0
   ```

2. **Compilar el proyecto**:
   ```bash
   cd /root
   make all
   ```

3. **Ejecutar el programa**:
   ```bash
   ./bin/master -p ./bin/player [otros parámetros]
   ```

### Notas Importantes

- El flag `--privileged` es necesario para el funcionamiento de semáforos y memoria compartida
- El volumen `-v "${PWD}:/root"` monta el directorio actual en `/root` del contenedor
- El contenedor incluye todas las dependencias necesarias (GCC, Make, Valgrind, etc.)

## 🚀 Ejecución del Programa

### Sintaxis de Ejecución

```bash
./bin/master [-w width] [-h height] [-d delay] [-t timeout] [-s seed] [-v ./bin/view] -p ./bin/player1 [./bin/player2] ... [./bin/player9]
```

### Parámetros

#### Parámetros Opcionales (con valores por defecto)

- **`[-w width]`**: Ancho del tablero. **Default y mínimo: 10**
- **`[-h height]`**: Alto del tablero. **Default y mínimo: 10**
- **`[-d delay]`**: Milisegundos que espera el master cada vez que se imprime el estado. **Default: 200**
- **`[-t timeout]`**: Timeout en segundos para recibir solicitudes de movimientos válidos. **Default: 10**
- **`[-s seed]`**: Semilla utilizada para la generación del tablero. **Default: time(NULL)**
- **`[-v ./bin/view]`**: Ruta del binario de la vista. **Default: Sin vista**

#### Parámetros Obligatorios

- **`-p ./bin/player1 [./bin/player2] ... [./bin/player9]`**: Ruta/s de los binarios de los jugadores. **Mínimo: 1, Máximo: 9**

### Ejemplos de Ejecución

```bash
# Juego básico con 1 jugador (usando valores por defecto)
./bin/master -p ./bin/player

# Juego con configuración personalizada
./bin/master -w 15 -h 15 -d 100 -t 15 -v ./bin/view -p ./bin/player

# Juego con múltiples jugadores
./bin/master -w 20 -h 20 -d 50 -t 20 -v ./bin/view -p ./bin/player ./bin/player ./bin/player

# Juego con semilla específica y sin vista
./bin/master -w 12 -h 12 -s 12345 -p ./bin/player ./bin/player

# Juego con configuración completa
./bin/master -w 25 -h 25 -d 75 -t 30 -s 98765 -v ./bin/view -p ./bin/player ./bin/player ./bin/player
```

## 📁 Estructura del Proyecto

```
SO_TPE1_2025/
├── src/        # Código fuente
│   ├── master.c        # Proceso master 
│   ├── view.c          # Proceso view 
│   ├── player.c        # Proceso player
│   └── lib/            # Librerías modulares compartidas
│       ├── common.h                # Estructuras y constantes globales
│       ├── library.c/.h            # Funciones de utilidad generales
│       ├── config_management.c/.h  # Gestión de configuración y argumentos; Visualización de resultados y parámetros
│       ├── memory_management.c/.h  # Gestión de memoria compartida
│       ├── process_management.c/.h # Gestión de procesos (fork, cleanup)
│       ├── game_logic.c/.h         # Lógica del juego (movimientos, validaciones) y Bucle principal del juego
│       ├── view_functions.c/.h     # Funciones específicas del view
│       └── player_functions.c/.h   # Funciones específicas del player
├── bin/                # Ejecutables compilados
├── Makefile            # Archivo de compilación
└── README.md           # Este archivo
```

## 🧪 Testing y Calidad del Código

### Herramientas Utilizadas
- **Valgrind**: Verificación de memory leaks y errores de memoria
- **PVS-Studio**: Análisis estático de código para detección de bugs
- **Strace**: Análisis de llamadas al sistema para debugging

### Resultados
- **Compilación**: Sin warnings con `-Wall -Wextra -Werror`
- **Memoria**: Sin memory leaks detectados
- **Sincronización**: Implementación sin deadlocks
- **Portabilidad**: Compatible con sistemas POSIX
