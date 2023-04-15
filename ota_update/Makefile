CC= gcc
CFLAGS= -Wall -Wextra -o2

EXEC=ota_update

all: $(EXEC)

$(EXEC): ota_update.c
	$(CC) $@.c $(CFLAGS) -o $@

clean:
	rm -f $(EXEC)


install:
