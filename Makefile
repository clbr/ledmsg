PREFIX ?= /usr

CFLAGS += -Wall -Wextra
LDFLAGS += -Wl,-O1

NAME = ledmsg
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

.PHONY: all clean install

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) -o $(NAME) $(OBJ) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *.o $(NAME)

install: all
	install -D -m 755 $(NAME) $(DESTDIR)$(PREFIX)/bin
