PROG    = mrsettings
SRC     = mrsettings.c
OBJ     = ${SRC:.c=.o}
CC      = gcc
CFLAGS  = `pkg-config --cflags gtk4` -g -std=gnu99
LIBS    = `pkg-config --libs gtk4` -lm -lgd

all: ${PROG}

%.o: %.c
	${CC} -c ${CFLAGS} $<

${PROG}: ${OBJ}
	${CC} ${OBJ} ${LIBS} -o $@

clean:
	rm -f ${OBJ} ${PROG}

install:
	install -Dm755 ${PROG} /usr/bin/${PROG}
	install -Dm644 mrsettings.desktop /usr/share/applications/mrsettings.desktop

uninstall:
	rm -f /usr/bin/${PROG}
	rm -f /usr/share/applications/mrsettings.desktop

.PHONY: all clean install uninstall
