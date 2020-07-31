DESTDIR =
prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
etcdir = ${prefix}/etc
CC = gcc

# PC/SC Lite libraries and headers.
PCSC_CFLAGS ?= `pkg-config libpcsclite --cflags`
PCSC_LDLIBS ?= `pkg-config libpcsclite --libs`

TARGET = recskapa
TARGETS = $(TARGET)
RELEASE_VERSION = "1.0.0"

CPPFLAGS = -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
CFLAGS   = -O2 -Wall -pthread ${PCSC_CFLAGS}

LIBS     = -lpthread -lm ${PCSC_LDLIBS}
LDFLAGS  =

OBJS  = recskapa.o recskapacore.o decoder.o mkpath.o b1/arib_std_b1.o b1/b_cas_card.o b1/multi2.o b1/ts_section_parser.o
OBJALL = $(OBJS)
DEPEND = .deps

all: $(TARGETS)

clean:
	rm -f $(OBJALL) $(TARGETS) $(DEPEND)

distclean: clean
	rm -f Makefile config.h config.log config.status

maintainer-clean: distclean
	rm -fr configure config.h.in aclocal.m4 autom4te.cache *~

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(DEPEND): version.h
	$(CC) -MM $(OBJS:.o=.c) $(CPPFLAGS) > $@

install: $(TARGET)
	install -m 755 $(TARGETS) $(DESTDIR)$(bindir)
	install -m 644 skapa.conf $(DESTDIR)$(etcdir)

-include .deps
