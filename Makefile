# -- Detección del Sistema Operativo
ifeq ($(OS),Windows_NT)
	EXE = .exe
	FIXPATH = $(subst /,\,$1)
	RM_FILE = del /Q /F
	RM_DIR = rmdir /Q /S
	MKDIR = if not exist $(call FIXPATH,$1) mkdir $(call FIXPATH,$1)
	
	# PDCurses para Windows (con soporte UTF-8 y caracteres anchos)
	PDCURSES_DIR = third-party/PDCurses
	PDCURSES_LIB = $(PDCURSES_DIR)/wincon/pdcurses.a
	CURSES_INC = -I$(PDCURSES_DIR) -DPDC_WIDE -DPDC_FORCE_UTF8
	CURSES_LIB = $(PDCURSES_LIB)
else
	EXE =
	FIXPATH = $1
	RM_FILE = rm -f
	RM_DIR = rm -rf
	MKDIR = mkdir -p $1
	
	# Ncurses para Linux / macOS (soporte UTF-8 con libncursesw)
	PDCURSES_LIB =
	CURSES_INC =
	CURSES_LIB = -lncursesw
	# Fallback a -lncurses si libncursesw no está disponible
	ifeq ($(shell ldconfig -p 2>/dev/null | grep libncursesw),)
		CURSES_LIB = -lncurses
	endif
endif

# -- Por defecto en modo depuración (?= op. asignar solo si no está definido)
BUILD_TYPE ?= Debug

# -- Compilador y directorios
CC = gcc
SRC_DIR = src
BIN_DIR = out/$(BUILD_TYPE)/bin
TARGET = $(BIN_DIR)/simulador$(EXE)

# -- Banderas de compilación base
CFLAGS = -Wall -Wextra -std=c11 -Iinclude $(CURSES_INC) -D_CRT_SECURE_NO_WARNINGS
LDFLAGS = $(CURSES_LIB) -lm

# -- Banderas de compilación según el tipo
ifeq ($(BUILD_TYPE), Release)
	CFLAGS += -O3 -DNDEBUG
else
	CFLAGS += -g -O0 -DDEBUG
endif

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/simulador.c $(SRC_DIR)/tui.c $(SRC_DIR)/utilidades.c $(SRC_DIR)/validaciones.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

ifeq ($(OS),Windows_NT)
$(TARGET): $(PDCURSES_LIB) $(OBJS)
	@$(call MKDIR,$(BIN_DIR))
	$(CC) $(OBJS) $(PDCURSES_LIB) -o $@ $(LDFLAGS)

$(PDCURSES_LIB):
	@echo [PDCurses] Compilando libreria estatica con soporte UTF-8 para Windows...
	@$(MAKE) -C $(PDCURSES_DIR)/wincon -f Makefile WIDE=Y UTF8=Y libs
else
$(TARGET): $(OBJS)
	@$(call MKDIR,$(BIN_DIR))
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
endif

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
ifeq ($(OS),Windows_NT)
	-@$(RM_FILE) $(call FIXPATH,$(SRC_DIR)\*.o) 2>nul || exit 0
	-@$(RM_DIR) $(call FIXPATH,out) 2>nul || exit 0
else
	@$(RM_FILE) $(SRC_DIR)/*.o
	@$(RM_DIR) out/
endif

clean-all: clean
ifeq ($(OS),Windows_NT)
	@$(MAKE) -C $(PDCURSES_DIR)/wincon -f Makefile clean
endif

.PHONY: all clean clean-all
