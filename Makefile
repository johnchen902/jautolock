CC      = gcc
CFLAGS  = -std=gnu11 -O3 -Wall -Wextra -Wshadow
LDFLAGS = -lX11 -lXss

.PHONY : all clean

all : jautolock

jautolock : jautolock.c
	$(CC) jautolock.c -o jautolock $(CFLAGS) $(LDFLAGS)

clean :
	$(RM) jautolock
