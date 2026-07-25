LIBS = -lX11
CFLAGS += -std=c99 -Wall -Wextra -pedantic -Os
CC ?= gcc

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
XSESSIONDIR ?= $(PREFIX)/share/xsessions

all: vswm

vswm: vswm.o
	$(CC) -o $@ $^ $(LIBS) $(LDFLAGS)

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -m755 vswm $(DESTDIR)$(BINDIR)

	install -d $(DESTDIR)$(XSESSIONDIR)
	install -m644 vswm.desktop $(DESTDIR)$(XSESSIONDIR)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/vswm
	rm -f $(DESTDIR)$(XSESSIONDIR)/vswm.desktop

clean:
	rm -f vswm *.o

.PHONY: all install uninstall clean
