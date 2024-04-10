PROG ?= iot-provision
DEFS ?= -liot-base -liot-json -llua
EXTRA_CFLAGS ?= -Wall -Werror
CFLAGS += $(DEFS) $(EXTRA_CFLAGS)

SRCS = main.c provision.c

all: $(PROG)

$(PROG):
	$(CC) $(SRCS) $(CFLAGS) -o $@


clean:
	rm -rf $(PROG) *.o
