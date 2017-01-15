CC      = gcc
CFLAGS  = -std=gnu11 -O3 -Wall -Wextra -Wshadow -D_GNU_SOURCE
LDFLAGS = -lX11 -lXss -lconfuse
TARGET  = jautolock
OBJECTS = jautolock.o timecalc.o fifo.o

.PHONY : all clean
all : $(TARGET)

$(TARGET) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

-include $(OBJECTS:.o=.d)
$(OBJECTS):
	$(CC) $(CFLAGS) -c $*.c -o $*.o -MMD -MP -MF $*.d

clean :
	$(RM) $(TARGET) *.o *.d
