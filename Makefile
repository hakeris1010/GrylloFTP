CC = gcc

CFLAGS= -g
LDFLAGS= 

ifeq ($(OS),Windows_NT)
    LDFLAGS += -lWs2_32
endif

SOURCES_SERVER= src/server/server.c \
                src/server/service.c \
                src/server/gsrvsocks.c \
                src/server/helper/hlog.h \
                src/server/helper/grylthread.c 

SOURCES_CLIENT= src/client/client.c

SOURCES_TEST1=  src/test/main.c \
                src/server/service.c \
                src/server/gsrvsocks.c \
                src/server/helper/hlog.h \
                src/server/helper/grylthread.c 


OBJDIR= build/obj
TESTDIR= build/test

SERVNAME= server
#CLINAME= client

TEST1= test1
TESTNAME= $(TEST1)

all: $(SERVNAME) $(TESTNAME)

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $*.o

$(SERVNAME): $(SOURCES_SERVER:.c=.o)
	$(CC) -o $@ $^ $(LDFLAGS)

#$(CLINAME): $(SOURCES_CLIENT:.c=.o)
#	$(CC) -o $@ $^ $(LDFLAGS)

$(TEST1): $(SOURCES_TEST1:.c=.o)
	$(CC) -o $(TESTDIR)/$@ $^ $(LDFLAGS)


clean:
	$(RM) *.o */*.o */*/*.o */*/*/*.o

clean_all: clean
	$(RM) $(SERVNAME) $(CLINAME)
