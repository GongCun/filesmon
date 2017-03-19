CC = gcc
CCV = $(shell ${CC} --version | grep '^gcc')
CFLAGS = -D_DEBUG -g -Wall -lpthread
SYSINFO = $(shell echo `uname -s` `oslevel`)
## git tag | tail -1 >./version
VERSION = $(shell cat ./version)
INSTALL_PATH = /usr/local/bin

PROGS = filesmon
SRC = main.c error.c funcs.c
HEAD = filesmon.h

all: ${PROGS}

filesmon: ${SRC} ${HEAD} version.h
	${CC} ${CFLAGS} -o $@ ${SRC}

version.h: $(SRC) $(HEAD)
	@echo Constructing version.h
	@rm -f version.h
	@echo '#define FILESMON_CC "${CC}"' > version.h
	@echo '#define FILESMON_CCV "${CCV}"' >> version.h
	@echo '#define FILESMON_CCDATE "'`date`'"' >> version.h
	@echo '#define FILESMON_CCFLAGS "${CFLAGS}"' >> version.h
	@echo '#define FILESMON_SYSINFO "${SYSINFO}"' >> version.h
	@echo '#define FILESMON_VERSION "${VERSION}"' >> version.h
	@echo '#define FILESMON_COPYRIGHT \
		"Copyright (c) '`date "+%Y"`' Gong Cun <gong_cun@bocmacau.com>"' >> version.h

install: ${PROGS}
	@echo Copy to ${INSTALL_PATH}
	[ -d $(INSTALL_PATH)/ ] || \
		(mkdir -p $(INSTALL_PATH)/; chmod 755 $(INSTALL_PATH)/)
	install -s -S -f $(INSTALL_PATH)/ -M 755 -O root -G system ${PROGS}


clean:
	rm -r -f ${PROGS} *.o *.BAK *~ core

