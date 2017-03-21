#mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
#current_dir := $(notdir $(patsubst %/,%,$(dir $(mkfile_path))))

CC = gcc
AR = ar

CFLAGS= -std=c99 
LDFLAGS= 

DEBUG_CFLAGS= -g
RELEASE_CFLAGS= -O2

OBJDIR= ./build/obj
TESTDIR= ./test
LIBDIR= ./lib
INCLUDEDIR= ./include
BINDIR= ./bin

#====================================#
#set directories
ZSH_RESULT:=$(shell mkdir -p $(OBJDIR) $(TESTDIR) $(LIBDIR) $(BINDIR) $(INCLUDEDIR))


# set os-dependent libs
ifeq ($(OS),Windows_NT)
    LDFLAGS += -lWs2_32
endif

#====================================#

SOURCES_SERVER= src/GrylloFTP/server/server.c \
                src/GrylloFTP/server/service.c 
LIBS_SERVER= $(GRYLTOOLS_LIB)

SOURCES_CLIENT= src/GrylloFTP/client/client.c
LIBS_CLIENT= $(GRYLTOOLS_LIB)

SOURCES_TEST1=  src/test/main.c 
LIBS_TEST1= $(GRYLTOOLS_LIB)

SOURCES_GRYLTOOLS = src/GrylloFTP/gryltools/grylthread.c \
                    src/GrylloFTP/gryltools/gsrvsocks.c \
                    src/GrylloFTP/gryltools/hlog.c

HEADERS_GRYLTOOLS=  src/GrylloFTP/gryltools/grylthread.h \
                    src/GrylloFTP/gryltools/gsrvsocks.h \
                    src/GrylloFTP/gryltools/hlog.h
LIBS_GRYLTOOLS=

#====================================#
#$(addprefix $(INCLUDEDIR)/gryltools/, $(notdir HEADERS_GRYLTOOLS))    

GRYLTOOLS_HEAD= $(INCLUDEDIR)/gryltools/
GRYLTOOLS_LIB= $(LIBDIR)/gryltools.a

SERVNAME= $(BINDIR)/server
CLINAME= $(BINDIR)/client
TESTNAME= $(TEST1)
GRYLTOOLS= gryltools

TEST1= $(TESTDIR)/test1

#====================================#

DEBUG_INCLUDES= -Isrc/GrylloFTP/gryltools
RELEASE_INCLUDES= -I$(GRYLTOOLS_HEAD)

#===================================#

debug: CFLAGS += $(DEBUG_CFLAGS) $(DEBUG_INCLUDES)
debug: $(GRYLTOOLS) $(SERVNAME) $(CLINAME) $(TESTNAME)

release: CFLAGS += $(RELEASE_CFLAGS) $(RELEASE_INCLUDES) 
release: $(GRYLTOOLS) $(SERVNAME) $(CLINAME) $(TESTNAME)

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $*.o

$(GRYLTOOLS_HEAD): $(HEADERS_GRYLTOOLS)
	mkdir -p $@    
	for file in $^ ; do \
    	cp $$file $@ ; \
	done

$(GRYLTOOLS_LIB): $(SOURCES_GRYLTOOLS:.c=.o) $(LIBS_GRYLTOOLS)
	$(AR) -rvsc $@ $^ 

$(GRYLTOOLS): $(GRYLTOOLS_LIB) $(GRYLTOOLS_HEAD)    

$(SERVNAME): $(SOURCES_SERVER:.c=.o) $(LIBS_SERVER)
	$(CC) -o $@ $^ $(LDFLAGS)

$(CLINAME): $(SOURCES_CLIENT:.c=.o) $(LIBS_CLIENT)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TEST1): $(SOURCES_TEST1:.c=.o) $(LIBS_TEST1) 
	$(CC) -o $@ $^ $(LDFLAGS)

#===================================#

clean:
	$(RM) *.o */*.o */*/*.o */*/*/*.o

clean_all: clean
	$(RM) $(SERVNAME) $(CLINAME) $(GRYLTOOLS_LIB) $(TESTNAME)
