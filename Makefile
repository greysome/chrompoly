UNAME := $(shell uname)

ifeq ($(UNAME),Windows_NT)
CC = x86_64-w64-mingw32-gcc
LIBS = -lm -lraylib_windows -lopengl32 -lgdi32 -lwinmm -lWs2_32
OUT = chrompoly.exe
CFLAGS += -Iraylib -Lraylib
else
CC = gcc
LIBS = -lm -lraylib_linux
OUT = chrompoly
CFLAGS += -Lraylib
endif

all: executable

debug: CFLAGS += -g
debug: executable

executable:
	$(CC) chrompoly.c -o $(OUT) -Wall $(CFLAGS) $(LIBS)