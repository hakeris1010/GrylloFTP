#mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
#current_dir := $(notdir $(patsubst %/,%,$(dir $(mkfile_path))))

CC = gcc
AR = ar

CFLAGS= -std=c99 
LDFLAGS= 

DEBUG_CFLAGS= -g
RELEASE_CFLAGS= -O2

OBJDIR= ./build/obj
TESTDIR= ./bin/test
LIBDIR= ./lib
INCLUDEDIR= ./include
BINDIR= ./bin

BINDIR_RELEASE= $(BINDIR)/release
BINDIR_DEBUG= $(BINDIR)/debug

#====================================#
#set directories
ZSH_RESULT:=$(shell mkdir -p $(OBJDIR) $(TESTDIR) $(LIBDIR) $(BINDIR_RELEASE) $(BINDIR_DEBUG) $(INCLUDEDIR))


#====================================#

SOURCES_SERVER= src/GrylloFTP/server/server.c \
                src/GrylloFTP/server/service.c 
LIBS_SERVER= $(GRYLTOOLS_LIB)

SOURCES_CLIENT= src/GrylloFTP/client/client.c \
                src/GrylloFTP/gftp/gftp.c
LIBS_CLIENT= $(GRYLTOOLS_LIB)

SOURCES_GRYLTOOLS = src/GrylloFTP/gryltools/grylthread.c \
                    src/GrylloFTP/gryltools/grylsocks.c \
                    src/GrylloFTP/gryltools/hlog.c \
                    src/GrylloFTP/gryltools/gmisc.c

HEADERS_GRYLTOOLS=  src/GrylloFTP/gryltools/grylthread.h \
                    src/GrylloFTP/gryltools/grylsocks.h \
                    src/GrylloFTP/gryltools/hlog.h \
                    src/GrylloFTP/gryltools/gmisc.h \
                    src/GrylloFTP/gryltools/systemcheck.h
LIBS_GRYLTOOLS=

#--------- Test sources ---------#

SOURCES_TEST1=  src/test/test1.c 
LIBS_TEST1= $(GRYLTOOLS_LIB)
TEST1= $(TESTDIR)/test1

SOURCES_TEST2=  src/test/test2.c 
LIBS_TEST2= $(GRYLTOOLS_LIB)
TEST2= $(TESTDIR)/test2

#---------  Test  list  ---------# 

TESTNAME= $(TEST1) $(TEST2)

#====================================#

# set os-dependent libs
ifeq ($(OS),Windows_NT)
    LDFLAGS += -lkernel32 -lWs2_32
	#LIBS_GRYLTOOLS += Ws2_32 kernel32
else
    CFLAGS += -std=gnu99 -pthread	
    LDFLAGS += -pthread
endif

#====================================#

#====================================#
GRYLTOOLS_INCL= $(INCLUDEDIR)/gryltools/
GRYLTOOLS_LIB= $(LIBDIR)/gryltools.a

GRYLTOOLS_INCLHEAD= #$(addprefix $(GRYLTOOLS_INCL), $(notdir HEADERS_GRYLTOOLS))    

SERVNAME= server
CLINAME= client
GRYLTOOLS= gryltools

#====================================#

DEBUG_INCLUDES= -Isrc/GrylloFTP/gryltools
RELEASE_INCLUDES= -I$(GRYLTOOLS_INCL)

BINPREFIX= 

#===================================#

all: debug

debops: 
	$(eval CFLAGS += $(DEBUG_CFLAGS) $(DEBUG_INCLUDES))
	$(eval BINPREFIX = $(BINDIR_DEBUG)) 

relops: 
	$(eval CFLAGS += $(RELEASE_CFLAGS) $(RELEASE_INCLUDES)) 
	$(eval BINPREFIX = $(BINDIR_RELEASE)) 

debug: debops $(GRYLTOOLS) $(CLINAME) $(TESTNAME)
release: relops $(GRYLTOOLS) $(CLINAME) $(TESTNAME)

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $*.o

gryltools_incl: $(HEADERS_GRYLTOOLS)
	mkdir -p $(GRYLTOOLS_INCL)    
	for file in $^ ; do \
		cp $$file $(GRYLTOOLS_INCL) ; \
	done

$(GRYLTOOLS_LIB): $(SOURCES_GRYLTOOLS:.c=.o) $(LIBS_GRYLTOOLS)
	$(AR) -rvsc $@ $^ 

$(GRYLTOOLS): $(GRYLTOOLS_LIB) gryltools_incl    
$(GRYLTOOLS)_debug: debops $(GRYLTOOLS)

$(SERVNAME): $(SOURCES_SERVER:.c=.o) $(LIBS_SERVER)
	$(CC) -o $(BINPREFIX)/$@ $^ $(LDFLAGS)
$(SERVNAME)_debug: debops $(SERVNAME)    

$(CLINAME): $(SOURCES_CLIENT:.c=.o) $(LIBS_CLIENT)
	$(CC) -o $(BINPREFIX)/$@ $^ $(LDFLAGS)
$(CLINAME)_debug: debops $(CLINAME)    

#===================================#
# Tests

test_debug: debops $(TESTNAME)

$(TEST1): $(SOURCES_TEST1:.c=.o) $(LIBS_TEST1) 
	$(CC) -o $@ $^ $(LDFLAGS)

$(TEST2): $(SOURCES_TEST2:.c=.o) $(LIBS_TEST2) 
	$(CC) -o $@ $^ $(LDFLAGS)

## $1 - target name, $2 - sources, $3 - libs
#define make_test_target
# $1: \$(patsubst %.c,%.o, $2) $3
#	$(CC) -o $@ $^ $(LDFLAGS)
#endef    
#
#$(foreach elem, $(TESTNAME), $(eval $(call make_test_target, elem, )) )

#===================================#

clean:
	$(RM) *.o */*.o */*/*.o */*/*/*.o

clean_all: clean
	$(RM) $(BINDIR)/*.* $(BINDIR)/*/*.* $(GRYLTOOLS_LIB) $(TESTNAME)
