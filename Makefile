# Compilador
CC = gcc

# Flags
CFLAGS = -Wall -Wextra -std=c99 -pthread -pedantic

# Linker/Loader Flags
LDFLAGS = -lrt -lpthread -lm

# Directorios
BIN_DIR = bin


all: clean $(BIN_DIR) master player view

# Agregar esta nueva regla
format:
	@echo "Formatting source files..."
	@find src -name "*.c" -o -name "*.h" | xargs clang-format -i
	@echo "Formatting complete!\n"

# Crear directorio bin si no existe
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

master:
	@echo "Compiling master..."
	$(CC) $(CFLAGS) src/master.c src/lib/library.c -o $(BIN_DIR)/master $(LDFLAGS)
	@echo "Master compiled successfully!\n"

player: $(BIN_DIR)
	@echo "Compiling player..."
	$(CC) $(CFLAGS) src/player.c src/lib/library.c src/lib/player_functions.c -o $(BIN_DIR)/player $(LDFLAGS)
	@echo "Player compiled successfully!\n"

view: $(BIN_DIR)
	@echo "Compiling view..."
	$(CC) $(CFLAGS) src/view.c src/lib/library.c src/lib/view_functions.c -o $(BIN_DIR)/view $(LDFLAGS)
	@echo "View compiled successfully!\n"

clean:
	@echo "Cleaning up..."
	@rm -rf $(BIN_DIR)
	@echo "Cleanup complete!\n"

.PHONY: all clean format