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
PROG1 = arvik-md4
PROG2 =
PROGS = $(PROG1)

all: $(PROGS)

$(PROG1): $(PROG1).o
	$(CC) $(CFLAGS) -o $@ $^

$(PROG1).o: $(PROG1).c
	$(CC) $(CFLAGS) -c $<

clean cls:
	rm -f $(PROGS) *.o *~ \#*

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

tar:
	tar cvfa ${LOGNAME}_Lab2.tar.gz *.[ch] Makefile
