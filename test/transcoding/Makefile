CC := gcc
CPPFLAGS  += -I$(CALIDUS_ROOT_DIR)/include -I./include
CFLAGS += -std=c99 -g
LDFLAGS += -L$(CALIDUS_ROOT_DIR)/lib
all: ff_main

ff_main: ff_main.o ff_encoder.o ff_decoder.o
	$(CC) $(LDFLAGS) -o ff_main $^ -lavformat -lavcodec -lavutil -lswresample -lavfilter -lpthread

ff_main.o: ff_main.c ff_decoder.h ff_priv.h ff_encoder.h
ff_encoder.o: ff_encoder.c ff_encoder.h ff_decoder.h ff_priv.h
ff_decoder.o: ff_decoder.c ff_decoder.h ff_priv.h

.PHONY: all clean

clean:
	@rm -rf ff_main ff_main.o ff_encoder.o ff_decoder.o
	@rm -rf abc.*
