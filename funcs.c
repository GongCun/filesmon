/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Gong Cun <gongcunjust@gmail.com>
 * Copyright (c) 2011 Cheryl L. Jennings, Trishali Nayar
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

#include "filesmon.h"

char *mk_subdir(char *path, char *mon, int size)
{
	char cmd[PATH_MAX];
	struct stat buf;

	if (path[strlen(path)-1] == '/')
		path[strlen(path)-1] = 0;

	if (stat(path, &buf) < 0)
		err_sys("stat() %s error", path);

	if (S_ISREG(buf.st_mode)) {
		snprintf(cmd, sizeof(cmd), "/usr/bin/mkdir -p %s/%s", MODFILE_MON_FACTORY, dirname(path));
		snprintf(mon, size, "%s/%s/%s.mon", MODFILE_MON_FACTORY, dirname(path), basename(path));
	} else {
		snprintf(cmd, sizeof(cmd), "/usr/bin/mkdir -p %s/%s", MODDIR_MON_FACTORY, dirname(path));
		snprintf(mon, size, "%s/%s/%s.mon", MODDIR_MON_FACTORY, dirname(path), basename(path));
	}

	if (system(cmd) != 0)
		err_quit("system() error");

	return(mon);
}

int skip_lines(char **p, int n)
{
	int lines = 0;

	while (n > 0) {
		*p = strchr(*p, '\n');
		if (*p == NULL)
			return(lines);
		(*p)++;
		n--;
		lines++;
	}

	return(lines);
}

static int lockfile(int fd)
{
	struct flock lock;

	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 0;

	return(fcntl(fd, F_SETLK, &lock));

}

static pid_t lockpid(int fd)
{
	struct flock lock;

	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_len = 0;

	if (fcntl(fd, F_GETLK, &lock) < 0)
		err_sys("fcntl() F_GETLK error");
	if (lock.l_type == F_UNLCK)
		return(0);

	return(lock.l_pid);
}

void already_running(int fd)
{
	int ret;
	char buf[16];
	pid_t pid;

	if ((pid = lockpid(fd)) != (pid_t) 0)
		err_quit("the process %d is already running", pid);
	if ((ret = lockfile(fd)) < 0) {
		if (errno == EACCES || errno == EAGAIN) {
			err_ret("lockfile() error");
			close(fd);
			return;
		} else
			err_sys("lockfile() error");
	}
	ftruncate(fd, 0);
	sprintf(buf, "%d\n", getpid());
	write(fd, buf, strlen(buf)+1);
	return;
}
