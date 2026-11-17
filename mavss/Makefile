CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2

banker: banker.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

.PHONY: clean
clean:
	rm -f banker
