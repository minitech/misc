CC := clang
CFLAGS := -std=c11 -pedantic -Ofast -march=native
WARNINGS := -Wall -Wextra -Weverything -Wno-padded

army: army.c
	$(CC) $(CFLAGS) $(WARNINGS) $< -o $@

clean:
	rm -f army

.PHONY: clean
