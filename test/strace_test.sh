#!/bin/bash

# Configuración del test
PLAYER_COUNT=9
WIDTH=10
HEIGHT=10
DELAY=200
TIMEOUT=5

# Construir lista de jugadores
players=""
for ((i=1; i<=PLAYER_COUNT; i++)); do
    players="$players ./bin/player"
done

# Ejecutar el master con strace
strace -o ./test/strace_master.log -f ./bin/master -p $players -v ./bin/view -w $WIDTH -h $HEIGHT -d $DELAY -t $TIMEOUT
