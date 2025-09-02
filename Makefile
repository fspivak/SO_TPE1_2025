# Compilador
CC = gcc

# Flags
CFLAGS = -Wall -Wextra -std=c99 -pthread -pedantic

# Linker/Loader Flags
LDFLAGS = -lrt -lpthread -lm

# Directorios
BIN_DIR = bin


all: clean $(BIN_DIR) player view

# Crear directorio bin si no existe
$(BIN_DIR):
	 mkdir -p $(BIN_DIR)

player: $(BIN_DIR)
	 $(CC) $(CFLAGS) src/player.c src/lib/library.c -o $(BIN_DIR)/player $(LDFLAGS)

view: $(BIN_DIR)
	 $(CC) $(CFLAGS) src/view.c src/lib/library.c -o $(BIN_DIR)/view $(LDFLAGS)


# master:
#    $(CC) $(CFLAGS) src/master.c src/lib/library.c -o $(BIN_DIR)/master $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean