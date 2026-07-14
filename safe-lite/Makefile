CC      := gcc
CFLAGS  := -Wall -Wextra -std=c99 -pedantic
TARGET  := safe-lite
OBJS    := main.o safe_common.o safe_wdg.o safe_e2e.o safe_queue.o fault_inject.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean
