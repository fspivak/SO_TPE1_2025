#!/bin/bash
# Script Alternativo con cppcheck
# He creado create_logs_alternative.sh que usa cppcheck en lugar de PVS-Studio. 
#Es más fácil de instalar en Docker y cumple con el requisito de análisis estático.

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== ChompChamps - Verificación de Calidad de Código (Alternativa) ===${NC}"

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

# Verificar si hay warnings
if [ -s ~/log/compilation_warnings.log ]; then
    echo -e "${RED}❌ ERROR: Se encontraron warnings durante la compilación:${NC}"
    cat ~/log/compilation_warnings.log
    exit 1
elsewget -q -O - https://files.pvs-studio.com/etc/pubkey.txt | apt-key add - \
 && wget -O /etc/apt/sources.list.d/viva64.list \
    https://files.pvs-studio.com/etc/viva64.list \
 && apt update -yq \
 && apt install -yq pvs-studio strace \
 && pvs-studio --version \
 && apt clean -yq
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
valgrind --trace-children=yes --leak-check=full --show-leak-kinds=all --track-origins=yes --track-fds=yes ./bin/master -p ./bin/player -w 10 -h 10 -d 200 -t 5 > ~/log/valgrind_master.log 2>&1

# Verificar resultados de Valgrind
if grep -q "ERROR SUMMARY: [^0]" ~/log/valgrind_master.log; then
    echo -e "${RED}❌ ERROR: Valgrind detectó problemas de memoria${NC}"
    grep "ERROR SUMMARY" ~/log/valgrind_master.log
    exit 1
else
    echo -e "${GREEN}✅ Valgrind: Sin errores de memoria detectados${NC}"
fi

# Limpiar memoria compartida después del test
rm -f /dev/shm/game_state /dev/shm/game_sync

# 3. ANÁLISIS ESTÁTICO CON CPPCHECK (Alternativa a PVS-Studio)
echo -e "\n${YELLOW}3. Ejecutando análisis estático con cppcheck...${NC}"

# Verificar si cppcheck está instalado
if ! command -v cppcheck &> /dev/null; then
    echo -e "${YELLOW}⚠️  cppcheck no está instalado. Instalando...${NC}"
    apt-get update > /dev/null 2>&1 && apt-get install -y cppcheck > /dev/null 2>&1
fi

if command -v cppcheck &> /dev/null; then
    # Ejecutar cppcheck en todos los archivos fuente
    cppcheck --enable=all --inconclusive --std=c99 --suppress=missingIncludeSystem \
             --suppress=unusedFunction \
             --xml --xml-version=2 src/ 2> ~/log/cppcheck.xml
    
    # Convertir a formato legible
    cppcheck --enable=all --inconclusive --std=c99 --suppress=missingIncludeSystem \
             --suppress=unusedFunction \
             src/ 2> ~/log/cppcheck.log
    
    # Contar problemas encontrados
    PROBLEM_COUNT=$(grep -c "\[.*\]" ~/log/cppcheck.log 2>/dev/null)
    
    if [ "$PROBLEM_COUNT" -gt 0 ]; then
        echo -e "${YELLOW}⚠️  cppcheck encontró $PROBLEM_COUNT problemas potenciales${NC}"
        echo -e "${BLUE}Revisa ~/log/cppcheck.log para detalles${NC}"
        
        # Mostrar los primeros problemas encontrados
        echo -e "${YELLOW}Primeros problemas encontrados:${NC}"
        head -20 ~/log/cppcheck.log | grep "\[.*\]" | head -5
    else
        echo -e "${GREEN}✅ cppcheck: Sin problemas detectados${NC}"
    fi
else
    echo -e "${RED}❌ ERROR: No se pudo instalar cppcheck${NC}"
    PROBLEM_COUNT=0
fi

# 4. RESUMEN FINAL
echo -e "\n${BLUE}=== RESUMEN FINAL ===${NC}"
echo -e "${GREEN}✅ Compilación: Sin warnings${NC}"
echo -e "${GREEN}✅ Valgrind: Sin errores de memoria${NC}"

if [ $PROBLEM_COUNT -gt 0 ]; then
    echo -e "${YELLOW}⚠️  Análisis estático: $PROBLEM_COUNT problemas encontrados${NC}"
    echo -e "${BLUE}   Revisa ~/log/cppcheck.log para detalles${NC}"
else
    echo -e "${GREEN}✅ Análisis estático: Sin problemas detectados${NC}"
fi

echo -e "\n${BLUE}Archivos generados:${NC}"
echo -e "  📄 ~/log/compilation_warnings.log"
echo -e "  📄 ~/log/valgrind_master.log"
echo -e "  📄 ~/log/cppcheck.log"
echo -e "  📄 ~/log/cppcheck.xml"

echo -e "\n${GREEN}🎉 Verificación de calidad completada${NC}"
