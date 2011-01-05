OBJECTS = txmond.o txctl.o
TARGETS = txmond txctl

# All

all: $(TARGETS)

# Daemon

txmond: txmond.o
	$(CC) -o txmond txmond.o

txmond.o: txmond.c txmond.h
	$(CC) -O2 -Wall -c txmond.c

# User interface

txctl: txctl.o
	$(CC) -o txctl txctl.o

txctl.o: txctl.c txmond.h
	$(CC) -O2 -Wall -c txctl.c

# Clean

clean:
	rm $(TARGETS) $(OBJECTS)
