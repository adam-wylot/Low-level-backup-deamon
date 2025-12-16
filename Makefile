override CFLAGS=-std=c17 -Wall -Wextra -Wshadow -Wno-unused-parameter -Wno-unused-const-variable -g -O0 -fsanitize=address,undefined,leak

ifdef CI
override CFLAGS=-std=c17 -Wall -Wextra -Wshadow -Werror -Wno-unused-parameter -Wno-unused-const-variable
endif

NAME=sop-backup

.PHONY: clean all

all: ${NAME}

SOURCES=$(shell find src -type f -iname '*.c')

OBJECTS=$(foreach x, $(basename $(SOURCES)), $(x).o)

$(NAME): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(NAME) $(OBJECTS)