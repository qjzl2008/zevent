ROOT = .
TARGET = testmysql

mysql_inc = /usr/include/mysql
common_inc = ../common/include
common_arch_inc =../common/include/arch/unix
common_mem_src = ../common/memory
common_locks_src = ../common/locks/unix
common_misc_src = ../common/misc
common_pool_src = ../common/pool

main_src = $(ROOT)
main_inc = $(ROOT)

obj = $(ROOT)/obj
dst = $(ROOT)/bin

target = $(dst)/$(TARGET)

#################################################################
#  Customization of the implicit rules

CC = gcc -funsigned-char
LD = ld
#CC = gcc
DEFS = -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DHAS_THREADS -DHAVE_CLOCK_MONOTONIC

IFLAGS1 = -I$(main_inc) 
IFLAGS2 = -I$(common_inc) -I$(common_arch_inc)
IFLAGS3 = -I$(mysql_inc)
CFLAGS = -g -Wall $(IFLAGS1) $(IFLAGS2) $(IFLAGS3)
SYSLIBS = -lpthread -lrt
MYSQL_LIBS = -lmysqlclient
LFLAGS = $(SYSLIBS) $(MYSQL_LIBS)

LIBS = $(SYSLIBS) 

LDFLAGS = 

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

common_pool_srcs = $(wildcard $(common_pool_src)/*.c)
common_pool_objs = $(patsubst $(common_pool_src)/%.c,$(obj)/%.o,$(common_pool_srcs))

objs =  $(main_objs) $(common_mem_objs) $(common_locks_objs) $(common_misc_objs) $(common_pool_objs)

#################################################################
#  Standard Rules

.PHONY: all clean

all: $(target)
clean: 
	$(RM) $(objs)
	$(RM) $(target)
$(target): $(objs)
	$(CC) $(CFLAGS) -o $(target) $(objs) $(LFLAGS)

#################################################################
#  Additional Rules

$(obj)/%.o: $(common_mem_src)/%.c $(common_incs)
	$(COMPILE.c) $< -o $@

$(obj)/%.o: $(common_misc_src)/%.c $(common_incs) $(common_arch_incs)
	$(COMPILE.c) $< -o $@

$(obj)/%.o: $(common_locks_src)/%.c $(common_incs) $(common_arch_incs)
	$(COMPILE.c) $< -o $@

$(obj)/%.o: $(common_pool_src)/%.c
	$(COMPILE.c) $< -o $@

$(obj)/%.o: $(main_src)/%.c $(main_incs)
	$(COMPILE.c) $< -o $@

