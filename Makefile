LBITS := $(shell getconf LONG_BIT)
ifeq ($(LBITS),64)
	CFLAGS += -m64 -fPIC
	SOURCES = HDE/hde64.c
	SOURCES_NOPY = HDE/hde64.c
else
	CFLAGS += -m32 -fPIC
	SOURCES = HDE/hde32.c
	SOURCES_NOPY =  HDE/hde32.c
endif

CC = gcc
CFLAGS += -shared -std=gnu11
LDFLAGS_NOPY += -ldl
LDFLAGS += $(shell python3.5-config --libs)
SOURCES_NOPY += dllmain.c commands.c simple_hook.c hooks.c misc.c maps_parser.c
SOURCES += dllmain.c commands.c python_embed.c python_dispatchers.c simple_hook.c hooks.c misc.c maps_parser.c
OBJS = $(SOURCES:.c=.o)
OBJS_NOPY = $(SOURCES_NOPY:.c=.o)
OUTPUT = minqlx.so
OUTPUT_NOPY = minqlx_nopy.so

.PHONY: depend clean

all: CFLAGS += $(shell python3.5-config --cflags)
all: $(OUTPUT)
	@echo Done!

debug: CFLAGS += $(shell python3.5-config --includes) -gdwarf-2 -Wall -O0 -fvar-tracking
debug: $(OUTPUT)
	@echo Done!

nopy: CFLAGS += -Wall -DNOPY
nopy: $(OUTPUT_NOPY)
	@echo Done!

nopy_debug: CFLAGS +=  -gdwarf-2 -Wall -O0 -DNOPY
nopy_debug: $(OUTPUT_NOPY)
	@echo Done!

$(OUTPUT): $(OBJS) 
	$(CC) $(CFLAGS) -o $(OUTPUT) $(OBJS) $(LDFLAGS)

$(OUTPUT_NOPY): $(OBJS_NOPY) 
	$(CC) $(CFLAGS) -o $(OUTPUT_NOPY) $(OBJS_NOPY) $(LDFLAGS_NOPY)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo Cleaning...
	@$(RM) *.o *~ $(OUTPUT)
	@$(RM) HDE/*.o HDE/*~ $(OUTPUT)
	@$(RM) *.o *~ $(OUTPUT_NOPY)
	@$(RM) HDE/*.o HDE/*~ $(OUTPUT_NOPY)
	@echo Done!
