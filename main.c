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

/* $Id: main.c,v 1.25 2016/11/18 08:46:26 root Exp root $ */
#include "filesmon.h"
#include "version.h"

#define OPTSTR "L:l:v"
#undef _DEBUG

const char keystr[] = KEYSTR;
FILE *Flist = NULL, *Flog = NULL;
int pipefd[2];
static int reread = 0;
sigset_t mask;

static void usage(const char *s)
{
	err_quit("Usage: %s [ -v | -L <InputList> -l <LogFile> Files... ]", s);
}


static char *isnullstr(char *s)
{
	if (!s)
		return((char *)NULL);
	while (*s) {
		if (*s != ' ')
			return(s);
		s++;
	}
	return((char *)NULL);
}

static void version(void)
{
	char *cp;
	if ((cp = isnullstr(FILESMON_VERSION)))
		fprintf(stderr, "Version: %s\n", cp);
	if ((cp = isnullstr(FILESMON_COPYRIGHT)))
		fprintf(stderr, "%s\n", cp);
	if ((cp = isnullstr(FILESMON_CC)))
		fprintf(stderr, "Compiler: %s\n", cp);
	if ((cp = isnullstr(FILESMON_CCV)))
		fprintf(stderr, "Compiler version: %s\n", cp);
	if ((cp = isnullstr(FILESMON_CCFLAGS)))
		fprintf(stderr, "Compiler flags: %s\n", cp);
	if ((cp = isnullstr(FILESMON_CCDATE)))
		fprintf(stderr, "Constructed: %s\n", cp);
	if ((cp = isnullstr(FILESMON_SYSINFO)))
		fprintf(stderr, "System info: %s\n", cp);

	return;
}

void *thr_fn(void *arg)
{
	int err, signo;
	for ( ; ; ) {
		if ((err = sigwait(&mask, &signo)) != 0) {
			errno = err;
			err_sys("sigwait failed");
		}
		switch (signo) {
			case SIGHUP:
				err_msg("Re-reading configuration file");
				if (write(pipefd[1], "a", 1) != 1)
					err_sys("write pipe error");
				break;
			case SIGTERM:
				err_msg("got SIGTERM; exiting");
				exit(0);
			default:
				err_msg("unexpected signal %d", signo);
		}
	}
	return((void *)0);
}

struct monfile mf[MAXFILES];

int main(int argc, char *argv[])
{
	char *path;
	char mon[PATH_MAX];
	int i, j, nsel;
	static int maxi;
	char buf[RDWR_BUF_SIZE];
	fd_set rset, rcopy;
	int maxfd;
	int ret;
	char *ptr;
	int len;

	pid_t pid;
	uid_t uid, luid;
	gid_t gid;
	char uname[64], lname[64], gname[64];
	time_t sec, nsec;
	struct tm *tm;
	char tmbuf[64];
	int seqnum;
	char progname[64];
	int c;
	char line[MAXLINE];

	int *pfd;
	char *fname;
	char *fmon;
	char config[PATH_MAX];
	struct tms tms;
	static long clktck = 0;
	clock_t start, end;

	pthread_t tid;
	struct sigaction sa;

	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		err_quit("sigaction SIGHUP failed");

	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);
	if ((ret = pthread_sigmask(SIG_BLOCK, &mask, NULL)) != 0) {
		errno = ret;
		err_sys("pthread_sigmask error");
	}

	if ((ret = pthread_create(&tid, NULL, thr_fn, 0)) != 0) {
		errno = ret;
		err_sys("pthread_create error");
	}

	if (argc < 2)
		usage(basename(argv[0]));

	bzero(config, sizeof(config));
	opterr = 0;
	optind = 1;
	while ((c = getopt(argc, argv, OPTSTR)) != -1)
		switch (c) {
			case 'L':
				if (optarg) {
					if (strcmp(optarg, "-") == 0)
						Flist = stdin;
					else {
						strncpy(config, optarg, strlen(optarg));
						if (!(Flist = fopen(config, "r")))
							err_sys("Can't open %s", config);
					}
				}
				break;
			case 'l':
				if (optarg) {
					if (strcmp(optarg, "-") == 0) {
						Flog = stdout;
					}
					else if (!(Flog = fopen(optarg, "a+")))
						err_sys("Can't open %s", optarg);
					if (setvbuf(Flog, line, _IOLBF, sizeof(line)) != 0)
						err_quit("setvbuf error");
					if (dup2(fileno(Flog), fileno(stderr)) < 0)
						err_sys("dup2() error");
				}
				break;
			case 'v':
				version();
				exit(0);
			case '?':
				usage(basename(argv[0]));
		}

	/* Check if already running */
	int lockfd;

	if ((lockfd = open(LOCKFILE, O_RDWR|O_CREAT, 0644)) < 0)
		err_sys("open() lockfd error");
	already_running(lockfd);

	if (pipe(pipefd) < 0)
		err_sys("pipe() error");

	for (i = 0, j = optind; j < argc; i++, j++) {
		if (i >= MAXFILES)
			err_quit("Number of files is too large");
		path = argv[j];
		strncpy(mf[i].fmon, mk_subdir(path, mon, sizeof(mon)), PATH_MAX);
		strncpy(mf[i].fname, path, PATH_MAX);
		if ((mf[i].fd = open(mf[i].fmon, O_CREAT|O_RDWR, 0664)) < 0 && errno != EEXIST)
			err_sys("open() error");

		len = strlen(keystr) + 1;
		if (write(mf[i].fd, keystr, len) != len && errno != EBUSY)
			err_sys("write() error");

#ifdef _DEBUG
		printf("i = %d, fname = %s, fmon = %s, fd = %d\n", i, mf[i].fname, mf[i].fmon, mf[i].fd);
#endif
	}
START:

	if (Flist) {
		if (reread && config[0] != 0) {
			if ((Flist = freopen(config, "r", Flist)) == NULL)
				err_sys("freopen %s error", config);
			reread = 0;
			if ((i = argc - optind) <= 0)
				i = 0;
		}

		while (fgets(line, sizeof(line), Flist) != NULL) {
			if (i >= MAXFILES)
				err_quit("Number of files is too large");
			if (line[0] == '\n' || line[0] == '#')
				continue;
			if (line[strlen(line) - 1] == '\n')
				line[strlen(line) - 1] = 0;
			strncpy(mf[i].fmon, mk_subdir(line, mon, sizeof(mon)), PATH_MAX);
			strncpy(mf[i].fname, line, PATH_MAX);

			if (mf[i].fd <= 0) {
				if ((mf[i].fd = open(mf[i].fmon, O_CREAT|O_RDWR, 0664)) < 0 && errno != EEXIST)
					err_sys("open() error");
				len = strlen(keystr) + 1;
				if (write(mf[i].fd, keystr, len) != len && errno != EBUSY)
					err_sys("write() error");
			}

#ifdef _DEBUG
			printf("i = %d, fname = %s, fmon = %s, fd = %d\n", i, mf[i].fname, mf[i].fmon, mf[i].fd);
#endif

			i++;
		}
		if (ferror(Flist))
			err_sys("fgets() error");
	}

	for (j = i; j < maxi; j++) {
		if (mf[j].fname[0] != 0 && mf[j].fd > 0) {
			close(mf[j].fd);
			bzero(&mf[j], sizeof(struct monfile));
		}
	}
	maxi = i;
	maxfd = 0;

	FD_ZERO(&rset);
	for (i = 0; i < maxi; i++) {
		if (mf[i].fd >= 0) {
			FD_SET(mf[i].fd, &rset); 
			maxfd = max(maxfd, mf[i].fd);
		}
	}
	FD_SET(pipefd[0], &rset);


	for ( ; ; ) {
#ifdef _DEBUG
		printf("select begin\n");
#endif

		for (i = 0; i < maxi; i++) {
			pfd = &mf[i].fd;
			fmon = mf[i].fmon;
			fname = mf[i].fname;

			if (*pfd < 0) {
#ifdef _DEBUG
				printf("fd less than 0: i:%d %s\n", i, fmon);
#endif
				if ((*pfd = open(fmon, O_CREAT | O_RDWR, 0664)) < 0 && errno != ENODEV) {
					err_sys("open() %s error", fmon);
				} else if (*pfd >= 0) {
					err_msg("File %s was recreated", fname);
					FD_SET(*pfd, &rset);
					len = strlen(keystr) + 1;
					if (write(*pfd, keystr, len) != len)
						err_sys("write() error");
					maxfd = max(maxfd, *pfd);
				}
#ifdef _DEBUG
				printf("fd = %d\n", *pfd);
#endif
			}
		}

		rcopy = rset; /* structure assignment */
		if ((nsel = select(maxfd + 1, &rcopy, NULL, NULL, NULL)) < 0) {
			/*
 			 * All errors in event monitoring will cause select to
 			 * return EBADF.  Read to see if any additional data is available.
			 */
			if (errno == EINTR || errno == EBADF)
				continue;
			else
				err_sys("select() error");
		}
#ifdef _DEBUG
		printf("nsel = %d, maxi = %d\n", nsel, maxi);
#endif
		if (FD_ISSET(pipefd[0], &rcopy)) {
			if (read(pipefd[0], line, sizeof(line)) <= 0)
				err_quit("read() pipe error");
			reread = 1;
			goto START;
		}

		for (i = 0; i < maxi; i++) {
			pfd = &mf[i].fd;
			fname = mf[i].fname;
			fmon = mf[i].fmon;

			if (*pfd < 0) continue;
			if (FD_ISSET(*pfd, &rcopy)) {
				printf("%s\n", fname);
				if ((ret = read(*pfd, buf, sizeof(buf))) < 0)
					err_sys("read() error");
				else if (ret == 0) {
					err_msg("read() error");
					if (close(*pfd) < 0)
						err_sys("close() error");
					FD_CLR(*pfd, &rset);
					*pfd = -1;
					continue;
				}
#ifdef _DEBUG
				printf("%s\n", buf);
#endif
				if (strncmp(buf, "BUF_WRAP", strlen("BUF_WRAP")) == 0) {
					err_msg("Buffer wrap detected, Some event occurrences lost!");
					continue;
				}
				ptr = buf;
				if (skip_lines(&ptr, 1) != 1)	/* Skip
								 * "BEGIN_EVENT_INFO"
								 * header */
					err_quit("Skip lines error");
				if (sscanf(ptr, "TIME_tvsec=%d\nTIME_tvnsec=%d\nSEQUENCE_NUM=%d\n",
					   &sec, &nsec, &seqnum) != 3) {
					err_quit("sscanf() error");
				}
				tm = localtime(&sec);
				strftime(tmbuf, sizeof(tmbuf), "%Y-%m-%d %H:%M:%S", tm);
				snprintf(line, sizeof(line), "%s.%.06d seq(%d) ", tmbuf, nsec / 1000, seqnum);
				if (skip_lines(&ptr, 3) != 3)
					err_quit("Skip lines error");

				if (sscanf(ptr, "PID=%d\nUID=%d\nUID_LOGIN=%d\nGID=%d\nPROG_NAME=%s\n",
				  &pid, &uid, &luid, &gid, progname) != 5) {
					err_quit("sscanf() error");
				}
				strcpy(uname, IDtouser(uid));
				strcpy(lname, IDtouser(luid));
				strcpy(gname, IDtogroup(gid));
				snprintf(line+strlen(line), sizeof(line)-strlen(line),
						"pid(%d) user(%s) login(%s) group(%s) prog(%s) file(%s)",
						pid, uname, lname, gname, progname, fname);
				err_msg("%s", line);

				skip_lines(&ptr, 5);	/* skip to
							 * 'RC_FROM_EVPROD'
							 * segment */
				if (sscanf(ptr, "RC_FROM_EVPROD=%d\nEND_EVENT_INFO", &ret) == 1) {
					switch(ret) {
						case AHAFS_MODFILE_WRITE:
							err_msg("File written.");
							break;
						case AHAFS_MODFILE_UNMOUNT:
							err_msg("Filesystem unmounted.");
							break;
						case AHAFS_MODFILE_MAP:
							err_msg("File mapped.");
							break;
						case AHAFS_MODFILE_FCLEAR:
							err_msg("File cleared.");
							break;
						case AHAFS_MODFILE_FTRUNC:
							err_msg("File truncated.");
							break;
						case AHAFS_MODFILE_OVERMOUNT:
							err_msg("File overmounted.");
							break;
						case AHAFS_MODFILE_RENAME:
						case AHAFS_MODFILE_REMOVE:
							if (ret == AHAFS_MODFILE_RENAME)
								err_msg("File renamed.");
							else
								err_msg("File removed.");
							if (close(*pfd) < 0)
								err_sys("close() error");
							FD_CLR(*pfd, &rset);
							*pfd = -1;
							err_msg("Recreate object.");
							if ((start = times(&tms)) < 0)
								err_sys("times() error");
RETRY:
							if ((end = times(&tms)) < 0)
								err_sys("times() error");
							if (clktck == 0 && (clktck = sysconf(_SC_CLK_TCK)) < 0)
								err_sys("sysconf() error");
							if ((end-start) / clktck > TIMEOUT) {
								err_msg("Recreate object timed out.\n");
								continue;
							}
							if ((*pfd = open(fmon, O_CREAT | O_RDWR, 0664)) < 0) {
								if (errno == ENODEV)
									goto RETRY;
								else
									err_sys("open() %s error", fmon);
							}
							len = strlen(keystr) + 1;
							if (write(*pfd, keystr, len) != len)
								err_sys("write() error");
							FD_SET(*pfd, &rset);
							maxfd = max(maxfd, *pfd);
							err_msg("Recreate object success.");
							break;
						default:
							err_msg("Unknown event.");
					}
				}
				err_msg("");
				if (--nsel == 0)
					break;
			}
		}
	}
	return(0);
}

