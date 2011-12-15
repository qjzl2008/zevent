COMMON_ROOT = ../../znet
ROOT = .
BINNAME = gateserver

common_inc = $(COMMON_ROOT)/common/include
common_arch_inc =$(COMMON_ROOT)/common/include/arch/unix
common_mem_src = $(COMMON_ROOT)/common/memory
common_locks_src = $(COMMON_ROOT)/common/locks/unix
common_misc_src = $(COMMON_ROOT)/common/misc

znet_inc = $(COMMON_ROOT)/src
main_src = $(ROOT)/src
main_inc = $(ROOT)/src

obj = $(ROOT)/obj
dbgobj = $(ROOT)/dbgobj
dst = $(ROOT)/bin

bin = $(dst)/$(BINNAME)


#################################################################
#  Customization of the implicit rules

CC = gcc -funsigned-char
LD = ld
#CC = gcc
DEFS = -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DHAS_THREADS -DEVLOOP_EPOLL -DHAVE_CLOCK_MONOTONIC

IFLAGS1 = -I$(znet_inc) -I$(main_inc) 
IFLAGS2 = -I$(common_inc) -I$(common_arch_inc)
CFLAGS = -O2 -fpic -Wall $(IFLAGS1) $(IFLAGS2)
SYSLIBS = -lpthread -lrt -lznet -ljson
LFLAGS = 
LDFLAGS = 

LIBS = $(SYSLIBS) 
LDIRS = -L../../znet/bin

RM = /bin/rm -f
COMPILE.c = $(CC) $(DEFS) $(CFLAGS) -c
LINK = $(CC) $(CFLAGS) $(LFLAGS) -o


#################################################################
#  Modules

main_incs = $(wildcard $(main_inc)/*.h)
main_sources = $(wildcard $(main_src)/*.c)
main_objs = $(patsubst $(main_src)/%.c,$(obj)/%.o,$(main_sources))

common_incs = $(wildcard $(common_inc)/*.h)
common_arch_incs = $(wildcard $(common_arch_inc)/*.h)
common_mem_srcs = $(wildcard $(common_mem_src)/*.c)
common_mem_objs = $(patsubst $(common_mem_src)/%.c,$(obj)/%.o,$(common_mem_srcs))

common_locks_srcs = $(wildcard $(common_locks_src)/*.c)
common_locks_objs = $(patsubst $(common_locks_src)/%.c,$(obj)/%.o,$(common_locks_srcs))

common_misc_srcs = $(wildcard $(common_misc_src)/*.c)
common_misc_objs = $(patsubst $(common_misc_src)/%.c,$(obj)/%.o,$(common_misc_srcs))

objs =  $(common_mem_objs) $(common_locks_objs) $(main_objs) $(common_misc_objs)

dbgobjs = $(patsubst $(obj)/%.o,$(dbgobj)/%.o,$(objs))


#################################################################
#  Standard Rules

.PHONY: all clean

all: $(bin)
clean: 
	$(RM) $(objs)
	$(RM) $(bin)
$(bin): $(objs)
	$(CC) $(LDFLAGS) -o $(bin) $(objs) $(LIBS) $(LDIRS)

#################################################################
#  Additional Rules

$(obj)/%.o: $(common_mem_src)/%.c $(common_incs)
	$(COMPILE.c) $< -o $@

$(obj)/%.o: $(common_misc_src)/%.c $(common_incs) $(common_arch_incs)
	$(COMPILE.c) $< -o $@

$(obj)/%.o: $(common_locks_src)/%.c $(common_incs) $(common_arch_incs)
	$(COMPILE.c) $< -o $@

$(obj)/%.o: $(main_src)/%.c $(main_incs)
	$(COMPILE.c) $< -o $@

