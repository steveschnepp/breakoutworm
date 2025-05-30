CFLAGS += -Wall -Oz -ffunction-sections -fdata-sections # -g
LDFLAGS = -s -Wl,--gc-sections
LDFLAGS +=
LDLIBS = -mconsole

CFLAGS += -DNDEBUG

SOURCES := $(wildcard *.c)
TARGETS := $(SOURCES:.c=)

COMMON_DIR := common
COMMON_SRC := $(wildcard $(COMMON_DIR)/*.c)
COMMON_OBJ := $(COMMON_SRC:.c=.o)

CFLAGS += -I$(COMMON_DIR)

all: $(TARGETS)

$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

%: %.c $(COMMON_SRC)
	$(CC) $< $(LDLIBS) $(CFLAGS) $(LDFLAGS) -o $@  $(COMMON_SRC)

breakoutworm: LDFLAGS += -ld3d9 -ld3dx9 -lwinmm -nostdlib -lkernel32 -luser32 -lucrtbase
breakoutworm: CFLAGS +=  \
	-flto \
	-fno-asynchronous-unwind-tables \
#    -ffast-math \
#    -fmerge-all-constants \
#    -fms-extensions \
#    -fno-exceptions \
#    -fno-pic \
#    -fno-pie \
#    -fno-rtti \
#    -fno-semantic-interposition \
#    -fno-stack-protector \
#    -fno-threadsafe-statics \
#    -fno-unwind-tables \
#    -fstrict-aliasing \
#    -mno-stack-arg-probe \
#    -nostdlib \
#    -w

clean:
	rm -f $(TARGETS) $(COMMON_OBJ)
