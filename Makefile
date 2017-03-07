CC = gcc

CFLAGS= -g
LDFLAGS= 

ifeq ($(OS),Windows_NT)
    LDFLAGS += -lWs2_32
endif

SOURCES_SERVER= src/server/server.c
SOURCES_CLIENT= src/client/client.c

OBJDIR= build/obj

SERVNAME= server
CLINAME= client

all: $(CLINAME) $(SERVNAME)

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $*.o

$(SERVNAME): $(SOURCES_SERVER:.c=.o)
	$(CC) -o $(SERVNAME) $^ $(LDFLAGS)

$(CLINAME): $(SOURCES_CLIENT:.c=.o)
	$(CC) -o $(CLINAME) $^ $(LDFLAGS)

clean:
	$(RM) *.o */*.o */*/*.o */*/*/*.o

clean_all: clean
	$(RM) $(SERVNAME) $(CLINAME)
