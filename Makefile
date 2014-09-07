TARGET=caudater
LIBS=-lyaml -lpthread -lpcre -lcurl

CC ?= gcc
CFLAGS += -std=gnu99 -Wall -pedantic -DUSE_YAML_CONFIG
INSTALL ?= install

OBJECTS = $(patsubst %.c, %.o, $(wildcard src/*.c))
HEADERS = $(wildcard *.h)

.PHONY: default all clean

default: $(TARGET)

all: default

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LIBS) -o $@

install: $(TARGET)
	$(INSTALL) -D -m 755 $(TARGET) $(DESTDIR)/usr/bin/$(TARGET)
	$(INSTALL) -D -m 644 contrib/config-default.yaml $(DESTDIR)/etc/yandex/$(TARGET)/config-default.yaml
	$(INSTALL) -D -m 644 contrib/caudater.init.conf $(DESTDIR)/etc/init/caudater.conf
	

clean:
	rm -f src/*.o
	rm -f $(TARGET)
