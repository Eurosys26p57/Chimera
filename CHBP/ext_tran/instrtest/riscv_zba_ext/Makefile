CC = riscv64-unknown-linux-gnu-gcc
CFLAGS = -march=rv64gc_zba
SRC = $(wildcard *.c)
OUT = $(SRC:.c=)

all: $(OUT)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

check: $(OUT)
	@for bin in $(OUT); do \
		echo "Running $$bin..."; \
		qemu-riscv64 -L /opt/riscv64/sysroot/ -cpu max ./$$bin; \
	done

clean:
	rm -f $(OUT)
