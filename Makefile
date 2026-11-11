# -- Detección del Sistema Operativo
ifeq ($(OS),Windows_NT)
	EXE = .exe
	FIXPATH = $(subst /,\,$1)
	RM_FILE = del /Q /F
	RM_DIR = rmdir /Q /S
	MKDIR = if not exist $(call FIXPATH,$1) mkdir $(call FIXPATH,$1)
else
	EXE =
	FIXPATH = $1
	RM_FILE = rm -f
	RM_DIR = rm -rf
	MKDIR = mkdir -p $1
endif

# -- Por defecto en modo depuración (?= op. asignar solo si no está definido)
BUILD_TYPE ?= Debug

# -- Compilador y directorios
CC = gcc
SRC_DIR = src
BIN_DIR = out/$(BUILD_TYPE)/bin
TARGET = $(BIN_DIR)/simulador$(EXE)

# -- Banderas de compilación base
CFLAGS = -Wall -Wextra -std=c11 -Iinclude -D_CRT_SECURE_NO_WARNINGS
LDFLAGS = -lm

# -- Banderas de compilación según el tipo
ifeq ($(BUILD_TYPE), Release)
	CFLAGS += -O3 -DNDEBUG
else
	CFLAGS += -g -O0 -DDEBUG
endif

SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/simulador.c $(SRC_DIR)/utilidades.c $(SRC_DIR)/validaciones.c $(SRC_DIR)/render.c $(SRC_DIR)/visualizacion.c $(SRC_DIR)/terminal_mcc.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	@$(call MKDIR,$(BIN_DIR))
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

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

.PHONY: all clean