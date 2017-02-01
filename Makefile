DEPENDS += x11 xscrnsaver libxdg-basedir libconfuse
CFLAGS  += -std=gnu11 -Wall -Wextra -Wshadow -D_GNU_SOURCE $(shell pkg-config --cflags $(DEPENDS))
LDFLAGS +=
LIBS    += $(shell pkg-config --libs $(DEPENDS))
TARGET  = jautolock
OBJECTS = jautolock.o timecalc.o fifo.o userconfig.o tasks.o messages.o

.PHONY : all clean install
all : $(TARGET)

$(TARGET) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBS)

-include $(OBJECTS:.o=.d)
$(OBJECTS):
	$(CC) $(CFLAGS) -c $*.c -o $*.o -MMD -MP -MF $*.d

clean :
	$(RM) $(TARGET) *.o *.d

install :
	install -m 755 -d $(DESTDIR)/usr/bin
	install -m 755 jautolock $(DESTDIR)/usr/bin/jautolock
