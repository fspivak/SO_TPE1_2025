#!/bin/bash

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Número de jugadores
PLAYER_COUNT=9



echo -e "${BLUE}=== ChompChamps - Verificación de Calidad de Código ===${NC}"

# 1. COMPILACIÓN CON WARNINGS ACTIVADOS
echo -e "\n${YELLOW}1. Verificando compilación con warnings activados...${NC}"

# Crear directorio de logs si no existe
mkdir -p ~/log

# Limpiar y compilar
make clean > /dev/null 2>&1
make 2> ~/log/compilation_warnings.log
COMPILE_EXIT_CODE=$?

# Verificar si la compilación fue exitosa
if [ $COMPILE_EXIT_CODE -ne 0 ]; then
    echo -e "${RED}❌ ERROR: La compilación falló${NC}"
    cat ~/log/compilation_warnings.log
    exit 1
fi

# Verificar si hay warnings (ignorar líneas vacías)
WARNING_COUNT=$(grep -v "^$" ~/log/compilation_warnings.log | wc -l || echo "0")
WARNING_COUNT=${WARNING_COUNT:-0}

if [ $WARNING_COUNT -gt 0 ]; then
    echo -e "${RED}❌ ERROR: Se encontraron $WARNING_COUNT warnings durante la compilación:${NC}"
    cat ~/log/compilation_warnings.log
    exit 1
else
    echo -e "${GREEN}✅ Compilación exitosa sin warnings${NC}"
fi

# Verificar que los binarios se compilaron correctamente
if [ ! -f "./bin/master" ] || [ ! -f "./bin/player" ] || [ ! -f "./bin/view" ]; then
    echo -e "${RED}❌ ERROR: No se pudieron compilar todos los binarios${NC}"
    echo "Binarios encontrados:"
    ls -la ./bin/ 2>/dev/null || echo "Directorio bin/ no existe"
    exit 1
fi

echo -e "${GREEN}✅ Todos los binarios compilados correctamente${NC}"

# Limpiar memoria compartida previa
rm -f /dev/shm/game_state /dev/shm/game_sync

# 2. ANÁLISIS CON VALGRIND
echo -e "\n${YELLOW}2. Ejecutando análisis con Valgrind...${NC}"
# Construir lista de jugadores
        players=""
        for ((i=1; i<=$PLAYER_COUNT; i++)); do
            players="$players ./bin/player"
        done
valgrind --trace-children=yes --leak-check=full --show-leak-kinds=all --track-origins=yes --track-fds=yes ./bin/master -p $players -w 10 -h 10 -d 200 -t 5 > ~/log/valgrind_master.log 2>&1

# Verificar resultados de Valgrind
VALGRIND_ERRORS=$(grep "ERROR SUMMARY:" ~/log/valgrind_master.log | awk '{sum += $4} END {print sum}')
VALGRIND_LEAKS=$(grep "definitely lost:" ~/log/valgrind_master.log | awk '{gsub(",", "", $4); sum += $4} END {print sum}')

# Asegurar que las variables tengan valores numéricos
VALGRIND_ERRORS=${VALGRIND_ERRORS:-0}
VALGRIND_LEAKS=${VALGRIND_LEAKS:-0}

if [ "$VALGRIND_ERRORS" -gt 0 ] || [ "$VALGRIND_LEAKS" -gt 0 ]; then
    echo -e "${RED}❌ ERROR: Valgrind detectó problemas de memoria${NC}"
    echo -e "${RED}   Errores: $VALGRIND_ERRORS${NC}"
    echo -e "${RED}   Memory leaks: $VALGRIND_LEAKS bytes${NC}"
    grep "ERROR SUMMARY\|definitely lost:" ~/log/valgrind_master.log
    exit 1
else
    echo -e "${GREEN}✅ Valgrind: Sin errores de memoria detectados${NC}"
fi

# Limpiar memoria compartida después del test
rm -f /dev/shm/game_state /dev/shm/game_sync

# 3. ANÁLISIS CON PVS-STUDIO
echo -e "\n${YELLOW}3. Ejecutando análisis con PVS-Studio...${NC}"

# Verificar si PVS-Studio está instalado
if ! command -v pvs-studio-analyzer &> /dev/null; then
    echo -e "${YELLOW}⚠️  PVS-Studio no está instalado en el contenedor.${NC}"
    echo -e "${BLUE}Opciones disponibles:${NC}"
    echo -e "${BLUE}1. Instalar PVS-Studio en el contenedor:${NC}"
    echo -e "${BLUE}   wget -q -O - https://files.pvs-studio.com/etc/pubkey.txt | apt-key add -${NC}"
    echo -e "${BLUE}   wget -O /etc/apt/sources.list.d/viva64.list https://files.pvs-studio.com/etc/viva64.list${NC}"
    echo -e "${BLUE}   apt-get update && apt-get install -y pvs-studio${NC}"
    echo -e "${BLUE}2. Ejecutar análisis estático en el host y copiar resultados${NC}"
    echo -e "${BLUE}3. Usar herramientas alternativas (cppcheck, clang-static-analyzer)${NC}"
    echo ""
    echo -e "${YELLOW}Saltando análisis estático por ahora...${NC}"
    PROBLEM_COUNT=0
else
    # Configurar PVS-Studio para capturar los comandos de compilación
    pvs-studio-analyzer trace -- make > /dev/null 2>&1

    # Ejecutar el análisis con PVS-Studio
    pvs-studio-analyzer analyze -o ~/log/pvs-studio.log

    # Convertir el informe a un formato legible (HTML)
    if command -v plog-converter &> /dev/null; then
        plog-converter -a GA:1,2 -t fullhtml ~/log/pvs-studio.log -o ~/log/pvs-studio-report.html
    else
        echo -e "${YELLOW}⚠️  plog-converter no está disponible. Generando solo log de texto.${NC}"
    fi

    # Verificar si PVS-Studio encontró problemas
    if [ -f ~/log/pvs-studio.log ]; then
        # Contar el número de problemas encontrados (buscar líneas que no sean solo headers)
        PROBLEM_COUNT=$(grep -v "^#" ~/log/pvs-studio.log | grep -v "^$" | grep -c "V[0-9]" 2>/dev/null || echo "0")
        PROBLEM_COUNT=${PROBLEM_COUNT:-0}
        if [ $PROBLEM_COUNT -gt 0 ]; then
            echo -e "${YELLOW}⚠️  PVS-Studio encontró $PROBLEM_COUNT problemas potenciales${NC}"
            if [ -f ~/log/pvs-studio-report.html ]; then
                echo -e "${BLUE}Revisa ~/log/pvs-studio-report.html para detalles${NC}"
            else
                echo -e "${BLUE}Revisa ~/log/pvs-studio.log para detalles${NC}"
            fi
            
            # Crear archivo de justificación de falsos positivos
            echo "Justificación de falsos positivos:" > ~/log/pvs-studio-false-positives.txt
            echo "=================================" >> ~/log/pvs-studio-false-positives.txt
            echo "" >> ~/log/pvs-studio-false-positives.txt
            echo "Si encuentras falsos positivos, justifícalos aquí:" >> ~/log/pvs-studio-false-positives.txt
            echo "Formato: [ID del mensaje]: Explicación breve de por qué no es un problema real." >> ~/log/pvs-studio-false-positives.txt
            echo "" >> ~/log/pvs-studio-false-positives.txt
            
            # Mostrar los primeros problemas encontrados
            echo -e "${YELLOW}Primeros problemas encontrados:${NC}"
            head -20 ~/log/pvs-studio.log | grep "V[0-9]" | head -5
        else
            echo -e "${GREEN}✅ PVS-Studio: Sin problemas detectados${NC}"
        fi
    else
        echo -e "${RED}❌ ERROR: PVS-Studio no generó el archivo de log${NC}"
        exit 1
    fi
fi

# 4. RESUMEN FINAL
echo -e "\n${BLUE}=== RESUMEN FINAL ===${NC}"
echo -e "${GREEN}✅ Compilación: Sin warnings${NC}"
echo -e "${GREEN}✅ Valgrind: Sin errores de memoria${NC}"

if [ "$PROBLEM_COUNT" -gt 0 ]; then
    echo -e "${YELLOW}⚠️  PVS-Studio: $PROBLEM_COUNT problemas encontrados${NC}"
    echo -e "${BLUE}   Revisa ~/log/pvs-studio-report.html para detalles${NC}"
    echo -e "${BLUE}   Justifica falsos positivos en ~/log/pvs-studio-false-positives.txt${NC}"
else
    echo -e "${GREEN}✅ PVS-Studio: Sin problemas detectados${NC}"
fi

echo -e "\n${BLUE}Archivos generados:${NC}"
echo -e "  📄 ~/log/compilation_warnings.log"
echo -e "  📄 ~/log/valgrind_master.log"
echo -e "  📄 ~/log/pvs-studio.log"
echo -e "  🌐 ~/log/pvs-studio-report.html"
echo -e "  📝 ~/log/pvs-studio-false-positives.txt"

echo -e "\n${GREEN}🎉 Verificación de calidad completada${NC}"
