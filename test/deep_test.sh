#!/bin/bash
# Sistema de Testing Integral para ChompChamps
# Verifica: deadlocks, limpieza de recursos, pipes, memoria compartida, memoria leak, overflows y semáforos
# Este script es para ejecutar el test_deep.sh en el contenedor de Docker
# Tiempo de ejecución TOTAL:  8 minutos 28 segundos (508 s)

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuración
TEST_ITERATIONS=5
TIMEOUT_SECONDS=30
LOG_FILE="./test/deep_test.log"
MASTER_BIN="./bin/master"
PLAYER_BIN="./bin/player"
VIEW_BIN="./bin/view"

# Contadores de tests
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

# Función para logging
log() {
    echo -e "$1" | tee -a "$LOG_FILE"
}

# Función de cleanup exhaustivo
cleanup_resources() {
    log "${YELLOW}Ejecutando cleanup de recursos...${NC}"
    
    # Matar todos los procesos relacionados
    pkill -f "$MASTER_BIN" 2>/dev/null
    pkill -f "$PLAYER_BIN" 2>/dev/null
    pkill -f "$VIEW_BIN" 2>/dev/null
    
    # Limpiar memoria compartida
    rm -f /dev/shm/game_state_* /dev/shm/game_sync_* 2>/dev/null
    rm -f /dev/shm/*chomp* /dev/shm/*game* 2>/dev/null
    
    # Limpiar semáforos
    ipcs -s | grep $(whoami) | awk '{print $2}' | xargs -r -n 1 ipcrm -s 2>/dev/null
    
    # Limpiar pipes con nombres específicos del juego
    find /tmp -name "*chomp*" -type p -delete 2>/dev/null
    find /tmp -name "*game*" -type p -delete 2>/dev/null
    
    # Limpiar file descriptors huérfanos (buscar por nombres de archivos conocidos)
    lsof +D /dev/shm 2>/dev/null | grep -E "(DEL|chomp|game)" | awk '{print $2}' | xargs -r kill -9 2>/dev/null
    
    sleep 1  # Dar tiempo para que el sistema libere recursos
}

# Función para verificar el estado del sistema
check_system_cleanliness() {
    local test_name="$1"
    local issues_found=0
    
    log "${CYAN}Verificando estado del sistema para: $test_name${NC}"
    
    # 1. Verificar memoria compartida
    if ls /dev/shm/ | grep -E "(game_state|game_sync|chomp)" >/dev/null 2>&1; then
        log "${RED}❌ PROBLEMA: Memoria compartida no liberada${NC}"
        ls -la /dev/shm/ | grep -E "(game_state|game_sync|chomp)" | tee -a "$LOG_FILE"
        issues_found=$((issues_found + 1))
    fi
    
    # 2. Verificar procesos activos
    local active_processes=$(ps aux | grep -E "($MASTER_BIN|$PLAYER_BIN|$VIEW_BIN)" | grep -v grep | wc -l)
    if [ "$active_processes" -gt 0 ]; then
        log "${RED}❌ PROBLEMA: Procesos aún activos ($active_processes)${NC}"
        ps aux | grep -E "($MASTER_BIN|$PLAYER_BIN|$VIEW_BIN)" | grep -v grep | tee -a "$LOG_FILE"
        issues_found=$((issues_found + 1))
    fi
    
    # 3. Verificar semáforos
    local active_semaphores=$(ipcs -s | grep $(whoami) | wc -l)
    if [ "$active_semaphores" -gt 0 ]; then
        log "${RED}❌ PROBLEMA: Semáforos no liberados ($active_semaphores)${NC}"
        ipcs -s | grep $(whoami) | tee -a "$LOG_FILE"
        issues_found=$((issues_found + 1))
    fi
    
    # 4. Verificar file descriptors/pipes abiertos
    local open_fds=$(lsof 2>/dev/null | grep -E "(game_state|game_sync|chomp)" | wc -l)
    if [ "$open_fds" -gt 0 ]; then
        log "${RED}❌ PROBLEMA: File descriptors/pipes no cerrados ($open_fds)${NC}"
        lsof 2>/dev/null | grep -E "(game_state|game_sync|chomp)" | head -10 | tee -a "$LOG_FILE"
        issues_found=$((issues_found + 1))
    fi
    
    # 5. Verificar pipes con nombre en /tmp
    local named_pipes=$(find /tmp -name "*chomp*" -o -name "*game*" -type p 2>/dev/null | wc -l)
    if [ "$named_pipes" -gt 0 ]; then
        log "${RED}❌ PROBLEMA: Pipes con nombre no eliminados ($named_pipes)${NC}"
        find /tmp -name "*chomp*" -o -name "*game*" -type p 2>/dev/null | tee -a "$LOG_FILE"
        issues_found=$((issues_found + 1))
    fi
    
    if [ "$issues_found" -eq 0 ]; then
        log "${GREEN}✅ Sistema limpio - todos los recursos liberados correctamente${NC}"
        return 0
    else
        log "${RED}❌ Se encontraron $issues_found problemas de limpieza${NC}"
        return 1
    fi
}

# Función para ejecutar una prueba con timeout
run_test() {
    local test_id="$1"
    local test_cmd="$2"
    local test_name="$3"
    
    TESTS_TOTAL=$((TESTS_TOTAL + 1))
    log "\n${BLUE}=== Ejecutando Test $test_id: $test_name ===${NC}"
    
    # Limpiar antes de la prueba
    cleanup_resources
    
    # Ejecutar prueba con timeout
    log "${CYAN}Comando: $test_cmd${NC}"
    timeout $TIMEOUT_SECONDS bash -c "$test_cmd" >/dev/null 2>&1 &
    local test_pid=$!
    
    # Esperar a que termine
    wait $test_pid 2>/dev/null
    local exit_code=$?
    
    # Verificar si fue terminado por timeout
    if [ $exit_code -eq 124 ]; then
        log "${RED}❌ TEST FALLIDO: Timeout - posible deadlock${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        cleanup_resources
        return 1
    fi
    
    # Pequeña pausa para permitir liberación de recursos
    sleep 1
    
    # Verificar limpieza del sistema
    if check_system_cleanliness "$test_name"; then
        log "${GREEN}✅ TEST EXITOSO: $test_name${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        log "${RED}❌ TEST FALLIDO: Problemas de limpieza en $test_name${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        cleanup_resources
        return 1
    fi
}

# Función para mostrar resultados finales
show_final_results() {
    log "\n${CYAN}=== RESULTADOS FINALES ===${NC}"
    log "Tests totales:   $TESTS_TOTAL"
    log "${GREEN}Tests exitosos: $TESTS_PASSED${NC}"
    
    if [ $TESTS_FAILED -eq 0 ]; then
        log "${GREEN}Tests fallidos:  $TESTS_FAILED${NC}"
        log "\n🎉 ${GREEN}TODOS LOS TESTS PASARON! El sistema limpia recursos correctamente.${NC}"
        return 0
    else
        log "${RED}Tests fallidos:  $TESTS_FAILED${NC}"
        log "\n❌ ${RED}ALGUNOS TESTS FALLARON. Revisar los logs para detalles.${NC}"
        return 1
    fi
}

# Función principal
main() {
    local start_time=$(date +%s)  # Registrar tiempo de inicio

    log "${BLUE}Iniciando Sistema de Testing para ChompChamps${NC}"
    log "Hora de inicio: $(date)"
    log "Iteraciones por test: $TEST_ITERATIONS"
    log "Timeout por test: ${TIMEOUT_SECONDS}s"
    echo "" > "$LOG_FILE"  # Limpiar log file

    # Crea los binarios si no existen
    # make > /dev/null 2>&1

    # Verificar que los binarios existen
    if [ ! -f "$MASTER_BIN" ] || [ ! -f "$PLAYER_BIN" ]; then
        log "${RED}Error: Binarios no encontrados. Ejecuta 'make' primero.${NC}"
        exit 1
    fi

    # Tests a ejecutar
    declare -A tests=(
        ["1"]="$MASTER_BIN -p $PLAYER_BIN -w 10 -h 10 -d 50"
        ["2"]="$MASTER_BIN -p $PLAYER_BIN $PLAYER_BIN -w 10 -h 10 -d 100"
        ["3"]="$MASTER_BIN -p $PLAYER_BIN $PLAYER_BIN $PLAYER_BIN -w 10 -h 10 -d 75"
        ["4"]="$MASTER_BIN -p $PLAYER_BIN $PLAYER_BIN -v $VIEW_BIN -w 10 -h 10 -d 150"
        ["5"]="$MASTER_BIN -p $PLAYER_BIN -w 10 -h 10 -d 200 -t 10"
    )

    # Ejecutar cada test múltiples veces
    for test_id in "${!tests[@]}"; do
        for ((i=1; i<=$TEST_ITERATIONS; i++)); do
            run_test "${test_id}-${i}" "${tests[$test_id]}" "Test ${test_id} (Iteración ${i})"
        done
    done

    # Tests de stress adicionales
    log "\n${BLUE}=== EJECUTANDO TESTS DE STRESS ===${NC}"
    for ((i=1; i<=3; i++)); do
        run_test "S-$i" "$MASTER_BIN -p $PLAYER_BIN $PLAYER_BIN $PLAYER_BIN $PLAYER_BIN -w 12 -h 12 -d 30" "Test de Stress $i"
    done

    # Mostrar resultados finales
    show_final_results

    # Limpieza final
    cleanup_resources

    local end_time=$(date +%s)  # Registrar tiempo de finalización
    local elapsed_time=$((end_time - start_time))
    log "${CYAN}Tiempo total de ejecución: ${elapsed_time} segundos${NC}"

    exit $?
}

# Manejar señal de interrupción (Ctrl+C)
trap 'log "${YELLOW}Interrumpido por usuario. Realizando cleanup...${NC}"; cleanup_resources; exit 1' INT

# Ejecutar función principal
main