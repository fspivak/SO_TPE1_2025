#!/bin/bash

# Función para mostrar ayuda
show_help() {
    echo "Uso: ./test.sh <test_number>"
    echo "Tests disponibles:"
    echo "  1 - Test con 1 jugador (10x10, delay 200ms)"
    echo "  2 - Test con 2 jugadores (15x15, delay 150ms)"
    echo "  3 - Test con 3 jugadores (20x20, delay 100ms)"
    echo "  4 - Test personalizado: ./test.sh 4 <num_jugadores> [width] [height] [delay]"
    echo "  5 - Test con 6 jugadores (3 player + 3 player_random, 15x15, delay 100ms)"
}

# Verificar argumentos
if [ $# -eq 0 ]; then
    show_help
    exit 1
fi

make

test_num=$1

case $test_num in
    1)
        echo "=== Test 1: 1 jugador en tablero 10x10 ==="
        ./bin/master -p bin/player -v bin/view -w 10 -h 10 -d 200 2> debug.log
        ;;
    2)
        echo "=== Test 2: 2 jugadores en tablero 15x15 ==="
        ./bin/master -p bin/player bin/player -v bin/view -w 10 -h 10 -d 150 2> debug.log
        ;;
    3)
        echo "=== Test 3: 3 jugadores en tablero 20x20 ==="
        ./bin/master -p bin/player bin/player bin/player -v bin/view -w 10 -h 10 -d 100 2> debug.log
        ;;
    4)
        if [ $# -lt 2 ]; then
            echo "Error: Test 4 requiere número de jugadores"
            echo "Uso: ./test.sh 4 <num_jugadores> [width] [height] [delay]"
            exit 1
        fi
        
        n=$2
        width=${3:-10}    # Default 10
        height=${4:-10}   # Default 10
        delay=${5:-200}   # Default 200ms
        
        # Construir lista de jugadores
        players=""
        for ((i=1; i<=n; i++)); do
            players="$players bin/player"
        done
        
        echo "=== Test 4: $n jugadores en tablero ${width}x${height}, delay ${delay}ms ==="
        ./bin/master -p $players -v bin/view -w $width -h $height -d $delay 2> debug.log
        ;;
    5)
        echo "=== Test 5: 6 jugadores (3 player + 3 player_random) en tablero 15x15 ==="
        ./bin/master -p bin/player bin/player bin/player bin/player_random bin/player_random bin/player_random -v bin/view -w 15 -h 15 -d 100 2> debug.log
        ;;
    *)
        echo "Error: Test $test_num no existe"
        show_help
        exit 1
        ;;
esac