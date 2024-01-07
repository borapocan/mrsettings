PROG = chaos_settings_menu

SRC = chaos_settings_menu.c
OBJ = ${SRC:.c=.o}

CC = gcc

CFLAGS = `pkg-config --cflags gtk4` -g -std=gnu99

LIBS = `pkg-config --libs gtk4`

%.o: %.c
	${CC} -c ${CFLAGS} $<

${PROG}: ${OBJ}
	${CC} ${LIBS} ${OBJ} -o $@

.PHONY: all hw
