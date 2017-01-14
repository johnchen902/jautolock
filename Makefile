CC      = gcc
CFLAGS  = -std=gnu11 -O3 -Wall -Wextra -Wshadow
LDFLAGS = -lX11 -lXss
TARGET  = jautolock
OBJECTS = jautolock.o timecalc.o fifo.o

.PHONY : all clean
all : $(TARGET)

$(TARGET) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

jautolock.o : jautolock.c action.h fifo.h timecalc.h timespecop.h
timecalc.o: timecalc.c timecalc.h action.h timespecop.h
fifo.o: fifo.c fifo.h
$(OBJECTS):
	$(CC) $(CFLAGS) -c $< -o $@

clean :
	$(RM) $(TARGET) $(OBJECTS)
