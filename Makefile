CC=clang
CFLAGS= -g -Wall -Wextra -pedantic -O2 -std=c99

TARGETS= temp_control

export MAKEFLAGS="-j 4"

all: $(TARGETS)

temp_control: temp_control.c pi_helpers.h
	$(CC) $(CFLAGS) -o $@ $< -lm

clean:
	rm -f $(TARGETS) *.o

