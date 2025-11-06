# Jeremy Cuthbert
# CS333 - Jesse Chaney
# Lab 2 Makefile

CC = gcc
DEBUG = -g
DEFINES =
CFLAGS = $(DEBUG) -Wall -Wextra -Wshadow -Wunreachable-code -Wredundant-decls \
	-Wmissing-declarations -Wold-style-definition -Wmissing-prototypes \
	-Wdeclaration-after-statement -Wno-return-local-addr -Werror \
	-Wunsafe-loop-optimizations -Wuninitialized $(DEFINES)
LDFLAGS = -lmd
PROG = arvik-md4

all: $(PROG)

$(PROG): $(PROG).o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(PROG).o: $(PROG).c arvik.h
	$(CC) $(CFLAGS) -c $<

clean cls:
	rm -f $(PROG) *.o *~ \#*

git:
	if [ ! -d .git ]; then \
	echo "Initializing new Git repository..."; \
	git init; \
	fi
	@echo "Adding source files to Git..."
	git add *.[ch] Makefile
	@echo "Committing changes..."
	git commit -m "Auto commit on $$(date '+%Y-%m-%d %H:%M:%S')"
	@echo "Pushing to remote repository..."
	git push -u origin main

TAR_FILE = ${LOGNAME}_Lab2.tar.gz
tar:
	rm -f $(TAR_FILE)
	tar cvaf $(TAR_FILE) arvik-md4.c Makefile
