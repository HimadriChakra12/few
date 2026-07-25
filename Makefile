LIBS = -lX11
CFLAGS += -std=c99 -Wall -Wextra -pedantic -Os
CC ?= gcc

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
XSESSIONDIR ?= $(PREFIX)/share/xsessions

all: few

few: few.o
	$(CC) -o $@ $^ $(LIBS) $(LDFLAGS)

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -m755 few $(DESTDIR)$(BINDIR)

	install -d $(DESTDIR)$(XSESSIONDIR)
	install -m644 few.desktop $(DESTDIR)$(XSESSIONDIR)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/few
	rm -f $(DESTDIR)$(XSESSIONDIR)/few.desktop

clean:
	rm -f few *.o

.PHONY: all install uninstall clean
