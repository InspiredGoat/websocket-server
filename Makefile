SOURCES = $(wildcard ./src/*.c)
SRC = $(addprefix src/, $(SOURCES))
OBJ = $(addsuffix .o, $(addprefix bin/, $(basename $(notdir $(SRC)))));
INCLUDE = -I include
CFLAGS = -W # -D_DEBUG_

all: websocket

again: clean websocket

websocket: $(OBJ)
	echo $(SOURCES)
	gcc -W $^ -lssl -lcrypto -o $@

bin/%.o : src/%.c
	gcc $(INCLUDE) $(CFLAGS) -c $< -o $@

clean:
	rm -f bin/*
	rm websocket

install:
	echo "Can't install surry"

try: websocket
	./websocket

run:
	./websocket
