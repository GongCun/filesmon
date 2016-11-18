CC = gcc
CCV = $(shell ${CC} --version | grep '^gcc')
CFLAGS = -D_DEBUG -g -Wall -lpthread
SYSINFO = $(shell echo `uname -s` `oslevel`)
VERSION = $(shell grep '\$$Id' main.c | awk '{print $$4}')

PROGS = filesmon
SRC = main.c error.c funcs.c

all: ${PROGS}

filesmon: ${SRC} version.h
	${CC} ${CFLAGS} -o $@ ${SRC}

version.h: main.c
	@echo Constructing version.h
	@rm -f version.h
	@echo '#define FILESMON_CC "${CC}"' > version.h
	@echo '#define FILESMON_CCV "${CCV}"' >> version.h
	@echo '#define FILESMON_CCDATE "'`date`'"' >> version.h
	@echo '#define FILESMON_CCFLAGS "${CFLAGS}"' >> version.h
	@echo '#define FILESMON_SYSINFO "${SYSINFO}"' >> version.h
	@echo '#define FILESMON_VERSION "${VERSION}"' >> version.h
	@echo '#define FILESMON_COPYRIGHT \
		"Copyright (c) 2016 Gong Cun <gongcunjust@gmail.com>"' >> version.h
	@touch -r main.c version.h

clean:
	rm -r -f ${PROGS} *.o *.BAK *~ core

