/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Gong Cun <gongcunjust@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _FILES_MON_H
#define _FILES_MON_H

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include <usersec.h> /* IDtouser() */
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>
#include <strings.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/ahafs_evProds.h>

#define MAXLINE 4096
#define RDWR_BUF_SIZE 4096
#define MODFILE_MON_FACTORY "/aha/fs/modFile.monFactory"
#define MODDIR_MON_FACTORY "/aha/fs/modDir.monFactory"
#define KEYSTR "WAIT_TYPE=WAIT_IN_SELECT;CHANGED=YES;INFO_LVL=1;NOTIFY_CNT=-1;BUF_SIZE=4096"
#define MAXFILES 1024 /* Less than FD_SETSIZE(65534) */
#define min(a,b) ((a) < (b) ? (a) : (b))
#define max(a,b) ((a) > (b) ? (a) : (b))
#define TIMEOUT 5
#define LOCKFILE "/tmp/filesmon.pid"


struct monfile {
	char fname[PATH_MAX]; /* file which be monitored */
	char fmon[PATH_MAX]; /* /aha/fs/.../file.mon */
	int fd;
};

void err_ret(const char *fmt, ...);
void err_sys(const char *fmt, ...);
void err_dump(const char *fmt, ...);
void err_msg(const char *fmt, ...);
void err_quit(const char *fmt, ...);

char *mk_subdir(char *path, char *mon, int);
int skip_lines(char **p, int n);
void already_running(int fd);

#endif
