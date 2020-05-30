/*	$NetBSD: util.c,v 1.54 2013/11/26 13:44:41 joerg Exp $	*/

/*
 * Missing stuff from OS's
 *
 *	$Id: util.c,v 1.33 2014/01/02 02:29:49 sjg Exp $
 */
#if defined(__MINT__) || defined(__linux__)
#include <signal.h>
#if (defined _WIN32 && ! defined __CYGWIN__)
#include "headers-mingw/signal.h"
#endif
#ifndef SIGQUIT
#define SIGQUIT SIGTERM
#endif
#endif

#include "make.h"

#ifndef MAKE_NATIVE
static char rcsid[] = "$NetBSD: util.c,v 1.54 2013/11/26 13:44:41 joerg Exp $";
#else
#ifndef lint
__RCSID("$NetBSD: util.c,v 1.54 2013/11/26 13:44:41 joerg Exp $");
#endif
#endif

#include <errno.h>
#include <time.h>
#include <signal.h>
#if (defined _WIN32 && ! defined __CYGWIN__)
#include "headers-mingw/signal.h"
#endif
#ifndef SIGQUIT
#define SIGQUIT SIGTERM
#endif

#ifndef NO_REGEX

#include    <sys/types.h>

# if (defined _WIN32 && !defined __CYGWIN__)
#include "headers-mingw/regex.h"
#include "headers-mingw/regex_internal.h"
# else

#include    <regex.h>

# endif
#endif

#if !defined(HAVE_PIPE) && (defined _WIN32 && ! defined __CYGWIN__)
/* Create a pipe.
   Copyright (C) 2009-2018 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <https://www.gnu.org/licenses/>.  */

/* Specification.  */
#include <unistd.h>
#include "headers-mingw/unistd.h"
/* Native Windows API.  */

/* Get _pipe().  */
#include <io.h>

/* Get _O_BINARY.  */
#include <fcntl.h>
#include "headers-mingw/fcntl.h"

int
pipe (int fd[2])
{
  /* Mingw changes fd to {-1,-1} on failure, but this violates
     http://austingroupbugs.net/view.php?id=467 */
  int tmp[2];
  int result = _pipe (tmp, 4096, _O_BINARY);
  if (!result)
    {
      fd[0] = tmp[0];
      fd[1] = tmp[1];
    }
  return result;
}

#endif

#if !defined(HAVE_GETDTABLESIZE) && (defined _WIN32 && ! defined __CYGWIN__)
/* getdtablesize() function: Return maximum possible file descriptor value + 1.
   Copyright (C) 2008-2018 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2008.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Specification.  */
#include <unistd.h>
#include "headers-mingw/unistd.h"

# include <stdio.h>

#  define _setmaxstdio_nothrow _setmaxstdio

/* Cache for the previous getdtablesize () result.  Safe to cache because
   Windows also lacks setrlimit.  */
static int dtablesize;

int
getdtablesize (void)
{
  if (dtablesize == 0)
    {
      /* We are looking for the number N such that the valid file descriptors
         are 0..N-1.  It can be obtained through a loop as follows:
           {
             int fd;
             for (fd = 3; fd < 65536; fd++)
               if (dup2 (0, fd) == -1)
                 break;
             return fd;
           }
         On Windows XP, the result is 2048.
         The drawback of this loop is that it allocates memory for a libc
         internal array that is never freed.

         The number N can also be obtained as the upper bound for
         _getmaxstdio ().  _getmaxstdio () returns the maximum number of open
         FILE objects.  The sanity check in _setmaxstdio reveals the maximum
         number of file descriptors.  This too allocates memory, but it is
         freed when we call _setmaxstdio with the original value.  */
      int orig_max_stdio = _getmaxstdio ();
      unsigned int bound;
      for (bound = 0x10000; _setmaxstdio_nothrow (bound) < 0; bound = bound / 2)
        ;
      _setmaxstdio_nothrow (orig_max_stdio);
      dtablesize = bound;
    }
  return dtablesize;
}

#endif

#if !defined(HAVE_FCNTL) && (defined _WIN32 && ! defined __CYGWIN__)
/* Provide file descriptor control.

   Copyright (C) 2009-2018 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Eric Blake <ebb9@byu.net>.  */

/* Specification.  */
#include "headers-mingw/fcntl.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <unistd.h>

/* Get declarations of the native Windows API functions.  */
# define WIN32_LEAN_AND_MEAN
# include <windows.h>

/* Get _get_osfhandle.  */
# if GNULIB_MSVC_NOTHROW
#  include "msvc-nothrow.h"
# else
#  include <io.h>
# endif

/* Upper bound on getdtablesize().  See lib/getdtablesize.c.  */
# define OPEN_MAX_MAX 0x10000

/* Duplicate OLDFD into the first available slot of at least NEWFD,
   which must be positive, with FLAGS determining whether the duplicate
   will be inheritable.  */
static int
dupfd (int oldfd, int newfd, int flags)
{
  /* Mingw has no way to create an arbitrary fd.  Iterate until all
     file descriptors less than newfd are filled up.  */
  HANDLE curr_process = GetCurrentProcess ();
  HANDLE old_handle = (HANDLE) _get_osfhandle (oldfd);
  unsigned char fds_to_close[OPEN_MAX_MAX / CHAR_BIT];
  unsigned int fds_to_close_bound = 0;
  int result;
  BOOL inherit = flags & O_CLOEXEC ? FALSE : TRUE;
  int mode;

  if (newfd < 0 || getdtablesize () <= newfd)
    {
      errno = EINVAL;
      return -1;
    }
  if (old_handle == INVALID_HANDLE_VALUE
      || (mode = setmode (oldfd, O_BINARY)) == -1)
    {
      /* oldfd is not open, or is an unassigned standard file
         descriptor.  */
      errno = EBADF;
      return -1;
    }
  setmode (oldfd, mode);
  flags |= mode;

  for (;;)
    {
      HANDLE new_handle;
      int duplicated_fd;
      unsigned int index;

      if (!DuplicateHandle (curr_process,           /* SourceProcessHandle */
                            old_handle,             /* SourceHandle */
                            curr_process,           /* TargetProcessHandle */
                            (PHANDLE) &new_handle,  /* TargetHandle */
                            (DWORD) 0,              /* DesiredAccess */
                            inherit,                /* InheritHandle */
                            DUPLICATE_SAME_ACCESS)) /* Options */
        {
          switch (GetLastError ())
            {
              case ERROR_TOO_MANY_OPEN_FILES:
                errno = EMFILE;
                break;
              case ERROR_INVALID_HANDLE:
              case ERROR_INVALID_TARGET_HANDLE:
              case ERROR_DIRECT_ACCESS_HANDLE:
                errno = EBADF;
                break;
              case ERROR_INVALID_PARAMETER:
              case ERROR_INVALID_FUNCTION:
              case ERROR_INVALID_ACCESS:
                errno = EINVAL;
                break;
              default:
                errno = EACCES;
                break;
            }
          result = -1;
          break;
        }
      duplicated_fd = _open_osfhandle ((intptr_t) new_handle, flags);
      if (duplicated_fd < 0)
        {
          CloseHandle (new_handle);
          result = -1;
          break;
        }
      if (newfd <= duplicated_fd)
        {
          result = duplicated_fd;
          break;
        }

      /* Set the bit duplicated_fd in fds_to_close[].  */
      index = (unsigned int) duplicated_fd / CHAR_BIT;
      if (fds_to_close_bound <= index)
        {
          if (sizeof fds_to_close <= index)
            /* Need to increase OPEN_MAX_MAX.  */
            abort ();
          memset (fds_to_close + fds_to_close_bound, '\0',
                  index + 1 - fds_to_close_bound);
          fds_to_close_bound = index + 1;
        }
      fds_to_close[index] |= 1 << ((unsigned int) duplicated_fd % CHAR_BIT);
    }

  /* Close the previous fds that turned out to be too small.  */
  {
    int saved_errno = errno;
    unsigned int duplicated_fd;

    for (duplicated_fd = 0;
         duplicated_fd < fds_to_close_bound * CHAR_BIT;
         duplicated_fd++)
      if ((fds_to_close[duplicated_fd / CHAR_BIT]
           >> (duplicated_fd % CHAR_BIT))
          & 1)
        close (duplicated_fd);

    errno = saved_errno;
  }
  return result;
}

/* Forward declarations, because we '#undef fcntl' in the middle of this
   compilation unit.  */
/* Our implementation of fcntl (fd, F_DUPFD, target).  */
static int rpl_fcntl_DUPFD (int fd, int target);
/* Our implementation of fcntl (fd, F_DUPFD_CLOEXEC, target).  */
static int rpl_fcntl_DUPFD_CLOEXEC (int fd, int target);

/* Perform the specified ACTION on the file descriptor FD, possibly
   using the argument ARG further described below.  This replacement
   handles the following actions, and forwards all others on to the
   native fcntl.  An unrecognized ACTION returns -1 with errno set to
   EINVAL.

   F_DUPFD - duplicate FD, with int ARG being the minimum target fd.
   If successful, return the duplicate, which will be inheritable;
   otherwise return -1 and set errno.

   F_DUPFD_CLOEXEC - duplicate FD, with int ARG being the minimum
   target fd.  If successful, return the duplicate, which will not be
   inheritable; otherwise return -1 and set errno.

   F_GETFD - ARG need not be present.  If successful, return a
   non-negative value containing the descriptor flags of FD (only
   FD_CLOEXEC is portable, but other flags may be present); otherwise
   return -1 and set errno.  */

int
fcntl (int fd, int action, /* arg */...)
{
  va_list arg;
  int result = -1;
  va_start (arg, action);
  switch (action)
    {
    case F_DUPFD:
      {
        int target = va_arg (arg, int);
        result = rpl_fcntl_DUPFD (fd, target);
        break;
      }

    case F_DUPFD_CLOEXEC:
      {
        int target = va_arg (arg, int);
        result = rpl_fcntl_DUPFD_CLOEXEC (fd, target);
        break;
      }

    case F_GETFD:
      {
        HANDLE handle = (HANDLE) _get_osfhandle (fd);
        DWORD flags;
        if (handle == INVALID_HANDLE_VALUE
            || GetHandleInformation (handle, &flags) == 0)
          errno = EBADF;
        else
          result = (flags & HANDLE_FLAG_INHERIT) ? 0 : FD_CLOEXEC;
        break;
      } /* F_GETFD */

      /* Implementing F_SETFD on mingw is not trivial - there is no
         API for changing the O_NOINHERIT bit on an fd, and merely
         changing the HANDLE_FLAG_INHERIT bit on the underlying handle
         can lead to odd state.  It may be possible by duplicating the
         handle, using _open_osfhandle with the right flags, then
         using dup2 to move the duplicate onto the original, but that
         is not supported for now.  */

    default:
      {
        errno = EINVAL;
        break;
      }
    }
  va_end (arg);
  return result;
}

static int
rpl_fcntl_DUPFD (int fd, int target)
{
  int result;
  result = dupfd (fd, target, 0);
  return result;
}

static int
rpl_fcntl_DUPFD_CLOEXEC (int fd, int target)
{
  int result;
  result = dupfd (fd, target, O_CLOEXEC);
  return result;
}

#endif

#if !defined(HAVE_FORK) && (defined _WIN32 && ! defined __CYGWIN__)
/*
 * fork.c
 * Experimental fork() on Windows.  Requires NT 6 subsystem or
 * newer.
 *
 * Copyright (c) 2012 William Pitcock <nenolod@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winnt.h>
#include <winternl.h>
#include <stdio.h>
#include <errno.h>
#include <process.h>

typedef struct _SECTION_IMAGE_INFORMATION {
	PVOID EntryPoint;
	ULONG StackZeroBits;
	ULONG StackReserved;
	ULONG StackCommit;
	ULONG ImageSubsystem;
	WORD SubSystemVersionLow;
	WORD SubSystemVersionHigh;
	ULONG Unknown1;
	ULONG ImageCharacteristics;
	ULONG ImageMachineType;
	ULONG Unknown2[3];
} SECTION_IMAGE_INFORMATION, *PSECTION_IMAGE_INFORMATION;

typedef struct _RTL_USER_PROCESS_INFORMATION {
	ULONG Size;
	HANDLE Process;
	HANDLE Thread;
	CLIENT_ID ClientId;
	SECTION_IMAGE_INFORMATION ImageInformation;
} RTL_USER_PROCESS_INFORMATION, *PRTL_USER_PROCESS_INFORMATION;

#define RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED	0x00000001
#define RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES		0x00000002
#define RTL_CLONE_PROCESS_FLAGS_NO_SYNCHRONIZE		0x00000004

#define RTL_CLONE_PARENT				0
#define RTL_CLONE_CHILD					297

typedef NTSTATUS (*RtlCloneUserProcess_f)(ULONG ProcessFlags,
	PSECURITY_DESCRIPTOR ProcessSecurityDescriptor /* optional */,
	PSECURITY_DESCRIPTOR ThreadSecurityDescriptor /* optional */,
	HANDLE DebugPort /* optional */,
	PRTL_USER_PROCESS_INFORMATION ProcessInformation);

pid_t fork(void)
{
	HMODULE mod;
	RtlCloneUserProcess_f clone_p;
	RTL_USER_PROCESS_INFORMATION process_info;
	NTSTATUS result;

	mod = GetModuleHandle("ntdll.dll");
	if (!mod)
		return -ENOSYS;

	clone_p = (RtlCloneUserProcess_f)GetProcAddress(mod, "RtlCloneUserProcess");
	if (clone_p == NULL)
		return -ENOSYS;

	/* lets do this */
	result = clone_p(RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED | RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES, NULL, NULL, NULL, &process_info);

	if (result == RTL_CLONE_PARENT)
	{
		HANDLE me, hp, ht, hcp = 0;
		DWORD pi, ti, mi;
		me = GetCurrentProcess();
		pi = (DWORD)process_info.ClientId.UniqueProcess;
		ti = (DWORD)process_info.ClientId.UniqueThread;

		ResumeThread(ht);
		CloseHandle(ht);
		CloseHandle(hp);
		return (pid_t)pi;
	}
	else if (result == RTL_CLONE_CHILD)
	{
		/* fix stdio */
		AllocConsole();
		return 0;
	}
	else
		return -1;

	/* NOTREACHED */
	return -1;
}
#endif

#if !defined(HAVE_GETENV) || !defined(HAVE_SETENV) || !defined(HAVE_UNSETENV)
extern char **environ;

static char *
findenv(const char *name, int *offset)
{
	size_t i, len;
	char *p, *q;

	len = strlen(name);
	for (i = 0; (q = environ[i]); i++) {
		p = strchr(q, '=');
		if (p == NULL || p - q != len)
			continue;
		if (strncmp(name, q, len) == 0) {
			*offset = i;
			return q + len + 1;
		}
	}
	*offset = i;
	return NULL;
}

char *
getenv(const char *name)
{
    int offset;

    return(findenv(name, &offset));
}

int
unsetenv(const char *name)
{
	char **p;
	int offset;

	if (name == NULL || *name == '\0' || strchr(name, '=') != NULL) {
		errno = EINVAL;
		return -1;
	}

	while (findenv(name, &offset))	{ /* if set multiple times */
		for (p = &environ[offset];; ++p)
			if (!(*p = *(p + 1)))
				break;
	}
	return 0;
}

int
setenv(const char *name, const char *value, int rewrite)
{
	char *c, **newenv;
	const char *cc;
	size_t l_value, size;
	int offset;

	if (name == NULL || value == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (*value == '=')			/* no `=' in value */
		++value;
	l_value = strlen(value);

	/* find if already exists */
	if ((c = findenv(name, &offset))) {
		if (!rewrite)
			return 0;
		if (strlen(c) >= l_value)	/* old larger; copy over */
			goto copy;
	} else {					/* create new slot */
		size = sizeof(char *) * (offset + 2);
		if (savedEnv == environ) {		/* just increase size */
			if ((newenv = realloc(savedEnv, size)) == NULL)
				return -1;
			savedEnv = newenv;
		} else {				/* get new space */
			/*
			 * We don't free here because we don't know if
			 * the first allocation is valid on all OS's
			 */
			if ((savedEnv = malloc(size)) == NULL)
				return -1;
			(void)memcpy(savedEnv, environ, size - sizeof(char *));
		}
		environ = savedEnv;
		environ[offset + 1] = NULL;
	}
	for (cc = name; *cc && *cc != '='; ++cc)	/* no `=' in name */
		continue;
	size = cc - name;
	/* name + `=' + value */
	if ((environ[offset] = malloc(size + l_value + 2)) == NULL)
		return -1;
	c = environ[offset];
	(void)memcpy(c, name, size);
	c += size;
	*c++ = '=';
copy:
	(void)memcpy(c, value, l_value + 1);
	return 0;
}

#ifdef TEST
int
main(int argc, char *argv[])
{
	setenv(argv[1], argv[2], 0);
	printf("%s\n", getenv(argv[1]));
	unsetenv(argv[1]);
	printf("%s\n", getenv(argv[1]));
	return 0;
}
#endif

#endif


#if defined(__hpux__) || defined(__hpux)
/* strrcpy():
 *	Like strcpy, going backwards and returning the new pointer
 */
static char *
strrcpy(char *ptr, char *str)
{
    int len = strlen(str);

    while (len)
	*--ptr = str[--len];

    return (ptr);
} /* end strrcpy */


char    *sys_siglist[] = {
        "Signal 0",
        "Hangup",                       /* SIGHUP    */
        "Interrupt",                    /* SIGINT    */
        "Quit",                         /* SIGQUIT   */
        "Illegal instruction",          /* SIGILL    */
        "Trace/BPT trap",               /* SIGTRAP   */
        "IOT trap",                     /* SIGIOT    */
        "EMT trap",                     /* SIGEMT    */
        "Floating point exception",     /* SIGFPE    */
        "Killed",                       /* SIGKILL   */
        "Bus error",                    /* SIGBUS    */
        "Segmentation fault",           /* SIGSEGV   */
        "Bad system call",              /* SIGSYS    */
        "Broken pipe",                  /* SIGPIPE   */
        "Alarm clock",                  /* SIGALRM   */
        "Terminated",                   /* SIGTERM   */
        "User defined signal 1",        /* SIGUSR1   */
        "User defined signal 2",        /* SIGUSR2   */
        "Child exited",                 /* SIGCLD    */
        "Power-fail restart",           /* SIGPWR    */
        "Virtual timer expired",        /* SIGVTALRM */
        "Profiling timer expired",      /* SIGPROF   */
        "I/O possible",                 /* SIGIO     */
        "Window size changes",          /* SIGWINDOW */
        "Stopped (signal)",             /* SIGSTOP   */
        "Stopped",                      /* SIGTSTP   */
        "Continued",                    /* SIGCONT   */
        "Stopped (tty input)",          /* SIGTTIN   */
        "Stopped (tty output)",         /* SIGTTOU   */
        "Urgent I/O condition",         /* SIGURG    */
        "Remote lock lost (NFS)",       /* SIGLOST   */
        "Signal 31",                    /* reserved  */
        "DIL signal"                    /* SIGDIL    */
};
#endif /* __hpux__ || __hpux */

#if defined(__hpux__) || defined(__hpux)
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/signal.h>
#include <sys/stat.h>
#if (defined _WIN32 && ! defined __CYGWIN__)
#include "headers-mingw/sys_stat.h"
#endif
#include <dirent.h>
#include <sys/time.h>
#include <unistd.h>
#if (defined _WIN32 && ! defined __CYGWIN__)
#include "headers-mingw/unistd.h"
#endif

int
killpg(int pid, int sig)
{
    return kill(-pid, sig);
}

#if !defined(__hpux__) && !defined(__hpux)
void
srandom(long seed)
{
    srand48(seed);
}

long
random(void)
{
    return lrand48();
}
#endif

#if !defined(__hpux__) && !defined(__hpux)
int
utimes(char *file, struct timeval tvp[2])
{
    struct utimbuf t;

    t.actime  = tvp[0].tv_sec;
    t.modtime = tvp[1].tv_sec;
    return(utime(file, &t));
}
#endif

#if !defined(BSD) && !defined(d_fileno)
# define d_fileno d_ino
#endif

#ifndef DEV_DEV_COMPARE
# define DEV_DEV_COMPARE(a, b) ((a) == (b))
#endif
#define ISDOT(c) ((c)[0] == '.' && (((c)[1] == '\0') || ((c)[1] == '/')))
#define ISDOTDOT(c) ((c)[0] == '.' && ISDOT(&((c)[1])))

char *
getwd(char *pathname)
{
    DIR    *dp;
    struct dirent *d;
    extern int errno;

    struct stat st_root, st_cur, st_next, st_dotdot;
    char    pathbuf[MAXPATHLEN], nextpathbuf[MAXPATHLEN * 2];
    char   *pathptr, *nextpathptr, *cur_name_add;

    /* find the inode of root */
    if (stat("/", &st_root) == -1) {
	(void)sprintf(pathname,
			"getwd: Cannot stat \"/\" (%s)", strerror(errno));
	return NULL;
    }
    pathbuf[MAXPATHLEN - 1] = '\0';
    pathptr = &pathbuf[MAXPATHLEN - 1];
    nextpathbuf[MAXPATHLEN - 1] = '\0';
    cur_name_add = nextpathptr = &nextpathbuf[MAXPATHLEN - 1];

    /* find the inode of the current directory */
    if (lstat(".", &st_cur) == -1) {
	(void)sprintf(pathname,
			"getwd: Cannot stat \".\" (%s)", strerror(errno));
	return NULL;
    }
    nextpathptr = strrcpy(nextpathptr, "../");

    /* Descend to root */
    for (;;) {

	/* look if we found root yet */
	if (st_cur.st_ino == st_root.st_ino &&
	    DEV_DEV_COMPARE(st_cur.st_dev, st_root.st_dev)) {
	    (void)strcpy(pathname, *pathptr != '/' ? "/" : pathptr);
	    return (pathname);
	}

	/* open the parent directory */
	if (stat(nextpathptr, &st_dotdot) == -1) {
	    (void)sprintf(pathname,
			    "getwd: Cannot stat directory \"%s\" (%s)",
			    nextpathptr, strerror(errno));
	    return NULL;
	}
	if ((dp = opendir(nextpathptr)) == NULL) {
	    (void)sprintf(pathname,
			    "getwd: Cannot open directory \"%s\" (%s)",
			    nextpathptr, strerror(errno));
	    return NULL;
	}

	/* look in the parent for the entry with the same inode */
	if (DEV_DEV_COMPARE(st_dotdot.st_dev, st_cur.st_dev)) {
	    /* Parent has same device. No need to stat every member */
	    for (d = readdir(dp); d != NULL; d = readdir(dp))
		if (d->d_fileno == st_cur.st_ino)
		    break;
	}
	else {
	    /*
	     * Parent has a different device. This is a mount point so we
	     * need to stat every member
	     */
	    for (d = readdir(dp); d != NULL; d = readdir(dp)) {
		if (ISDOT(d->d_name) || ISDOTDOT(d->d_name))
		    continue;
		(void)strcpy(cur_name_add, d->d_name);
		if (lstat(nextpathptr, &st_next) == -1) {
		    (void)sprintf(pathname,
			"getwd: Cannot stat \"%s\" (%s)",
			d->d_name, strerror(errno));
		    (void)closedir(dp);
		    return NULL;
		}
		/* check if we found it yet */
		if (st_next.st_ino == st_cur.st_ino &&
		    DEV_DEV_COMPARE(st_next.st_dev, st_cur.st_dev))
		    break;
	    }
	}
	if (d == NULL) {
	    (void)sprintf(pathname,
		"getwd: Cannot find \".\" in \"..\"");
	    (void)closedir(dp);
	    return NULL;
	}
	st_cur = st_dotdot;
	pathptr = strrcpy(pathptr, d->d_name);
	pathptr = strrcpy(pathptr, "/");
	nextpathptr = strrcpy(nextpathptr, "../");
	(void)closedir(dp);
	*cur_name_add = '\0';
    }
} /* end getwd */

#endif /* __hpux */

#if !defined(HAVE_RANDOM)
/* Copyright (C) 1995-2018 Free Software Foundation, Inc.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

/*
 * This is derived from the Berkeley source:
 *      @(#)random.c    5.5 (Berkeley) 7/6/88
 * It was reworked for the GNU C Library by Roland McGrath.
 * Rewritten to use reentrant functions by Ulrich Drepper, 1995.
 */

/*
   Copyright (C) 1983 Regents of the University of California.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   4. Neither the name of the University nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.*/

#ifndef _LIBC
# include "headers-mingw/libc-config.h"
#endif

/* Specification.  */
#include <stdlib.h>
#if (defined _WIN32 && ! defined __CYGWIN__)
#include "headers-mingw/stdlib.h"
#endif

#include <errno.h>
#include <stddef.h>
#include <string.h>

#ifdef _LIBC
# include <libc-lock.h>
#else
/* Allow memory races; that's random enough.  */
# define __libc_lock_define_initialized(class, name)
# define __libc_lock_lock(name) ((void) 0)
# define __libc_lock_unlock(name) ((void) 0)
#endif

/* An improved random number generation package.  In addition to the standard
   rand()/srand() like interface, this package also has a special state info
   interface.  The initstate() routine is called with a seed, an array of
   bytes, and a count of how many bytes are being passed in; this array is
   then initialized to contain information for random number generation with
   that much state information.  Good sizes for the amount of state
   information are 32, 64, 128, and 256 bytes.  The state can be switched by
   calling the setstate() function with the same array as was initialized
   with initstate().  By default, the package runs with 128 bytes of state
   information and generates far better random numbers than a linear
   congruential generator.  If the amount of state information is less than
   32 bytes, a simple linear congruential R.N.G. is used.  Internally, the
   state information is treated as an array of longs; the zeroth element of
   the array is the type of R.N.G. being used (small integer); the remainder
   of the array is the state information for the R.N.G.  Thus, 32 bytes of
   state information will give 7 longs worth of state information, which will
   allow a degree seven polynomial.  (Note: The zeroth word of state
   information also has some other information stored in it; see setstate
   for details).  The random number generation technique is a linear feedback
   shift register approach, employing trinomials (since there are fewer terms
   to sum up that way).  In this approach, the least significant bit of all
   the numbers in the state table will act as a linear feedback shift register,
   and will have period 2^deg - 1 (where deg is the degree of the polynomial
   being used, assuming that the polynomial is irreducible and primitive).
   The higher order bits will have longer periods, since their values are
   also influenced by pseudo-random carries out of the lower bits.  The
   total period of the generator is approximately deg*(2**deg - 1); thus
   doubling the amount of state information has a vast influence on the
   period of the generator.  Note: The deg*(2**deg - 1) is an approximation
   only good for large deg, when the period of the shift register is the
   dominant factor.  With deg equal to seven, the period is actually much
   longer than the 7*(2**7 - 1) predicted by this formula.  */



/* For each of the currently supported random number generators, we have a
   break value on the amount of state information (you need at least this many
   bytes of state info to support this random number generator), a degree for
   the polynomial (actually a trinomial) that the R.N.G. is based on, and
   separation between the two lower order coefficients of the trinomial.  */

/* Linear congruential.  */
#define TYPE_0          0
#define BREAK_0         8
#define DEG_0           0
#define SEP_0           0

/* x**7 + x**3 + 1.  */
#define TYPE_1          1
#define BREAK_1         32
#define DEG_1           7
#define SEP_1           3

/* x**15 + x + 1.  */
#define TYPE_2          2
#define BREAK_2         64
#define DEG_2           15
#define SEP_2           1

/* x**31 + x**3 + 1.  */
#define TYPE_3          3
#define BREAK_3         128
#define DEG_3           31
#define SEP_3           3

/* x**63 + x + 1.  */
#define TYPE_4          4
#define BREAK_4         256
#define DEG_4           63
#define SEP_4           1


/* Array versions of the above information to make code run faster.
   Relies on fact that TYPE_i == i.  */

#define MAX_TYPES       5       /* Max number of types above.  */


/* Initially, everything is set up as if from:
        initstate(1, randtbl, 128);
   Note that this initialization takes advantage of the fact that srandom
   advances the front and rear pointers 10*rand_deg times, and hence the
   rear pointer which starts at 0 will also end up at zero; thus the zeroth
   element of the state information, which contains info about the current
   position of the rear pointer is just
        (MAX_TYPES * (rptr - state)) + TYPE_3 == TYPE_3.  */

static int32_t randtbl[DEG_3 + 1] =
        {
                TYPE_3,

                -1726662223, 379960547, 1735697613, 1040273694, 1313901226,
                1627687941, -179304937, -2073333483, 1780058412, -1989503057,
                -615974602, 344556628, 939512070, -1249116260, 1507946756,
                -812545463, 154635395, 1388815473, -1926676823, 525320961,
                -1009028674, 968117788, -123449607, 1284210865, 435012392,
                -2017506339, -911064859, -370259173, 1132637927, 1398500161,
                -205601318,
        };


static struct random_data unsafe_state =
        {
/* FPTR and RPTR are two pointers into the state info, a front and a rear
   pointer.  These two pointers are always rand_sep places apart, as they
   cycle through the state information.  (Yes, this does mean we could get
   away with just one pointer, but the code for random is more efficient
   this way).  The pointers are left positioned as they would be from the call:
        initstate(1, randtbl, 128);
   (The position of the rear pointer, rptr, is really 0 (as explained above
   in the initialization of randtbl) because the state table pointer is set
   to point to randtbl[1] (as explained below).)  */

                .fptr = &randtbl[SEP_3 + 1],
                .rptr = &randtbl[1],

/* The following things are the pointer to the state information table,
   the type of the current generator, the degree of the current polynomial
   being used, and the separation between the two pointers.
   Note that for efficiency of random, we remember the first location of
   the state information, not the zeroth.  Hence it is valid to access
   state[-1], which is used to store the type of the R.N.G.
   Also, we remember the last location, since this is more efficient than
   indexing every time to find the address of the last element to see if
   the front and rear pointers have wrapped.  */

                .state = &randtbl[1],

                .rand_type = TYPE_3,
                .rand_deg = DEG_3,
                .rand_sep = SEP_3,

                .end_ptr = &randtbl[sizeof (randtbl) / sizeof (randtbl[0])]
        };

/* POSIX.1c requires that there is mutual exclusion for the 'rand' and
   'srand' functions to prevent concurrent calls from modifying common
   data.  */
__libc_lock_define_initialized (static, lock)

/* Initialize the random number generator based on the given seed.  If the
   type is the trivial no-state-information type, just remember the seed.
   Otherwise, initializes state[] based on the given "seed" via a linear
   congruential generator.  Then, the pointers are set to known locations
   that are exactly rand_sep places apart.  Lastly, it cycles the state
   information a given number of times to get rid of any initial dependencies
   introduced by the L.C.R.N.G.  Note that the initialization of randtbl[]
   for default usage relies on values produced by this routine.  */
void
__srandom (unsigned int x)
{
    __libc_lock_lock (lock);
    (void) __srandom_r (x, &unsafe_state);
    __libc_lock_unlock (lock);
}

inline void
srandom (unsigned int x)
{
    return __srandom(x);
}

inline void
srand (unsigned int x)
{
    return __srandom(x);
}

/* Initialize the state information in the given array of N bytes for
   future random number generation.  Based on the number of bytes we
   are given, and the break values for the different R.N.G.'s, we choose
   the best (largest) one we can and set things up for it.  srandom is
   then called to initialize the state information.  Note that on return
   from srandom, we set state[-1] to be the type multiplexed with the current
   value of the rear pointer; this is so successive calls to initstate won't
   lose this information and will be able to restart with setstate.
   Note: The first thing we do is save the current state, if any, just like
   setstate so that it doesn't matter when initstate is called.
   Returns a pointer to the old state.  */
char *
__initstate (unsigned int seed, char *arg_state, size_t n)
{
    int32_t *ostate;
    int ret;

    __libc_lock_lock (lock);

    ostate = &unsafe_state.state[-1];

    ret = __initstate_r (seed, arg_state, n, &unsafe_state);

    __libc_lock_unlock (lock);

    return ret == -1 ? NULL : (char *) ostate;
}

inline char *
initstate (unsigned int seed, char *arg_state, size_t n)
{
    return __initstate(seed, arg_state, n);
}

/* Restore the state from the given state array.
   Note: It is important that we also remember the locations of the pointers
   in the current state information, and restore the locations of the pointers
   from the old state information.  This is done by multiplexing the pointer
   location into the zeroth word of the state information. Note that due
   to the order in which things are done, it is OK to call setstate with the
   same state as the current state
   Returns a pointer to the old state information.  */
char *
__setstate (char *arg_state)
{
    int32_t *ostate;

    __libc_lock_lock (lock);

    ostate = &unsafe_state.state[-1];

    if (__setstate_r (arg_state, &unsafe_state) < 0)
        ostate = NULL;

    __libc_lock_unlock (lock);

    return (char *) ostate;
}

inline char *
setstate (char *arg_state)
{
    return __setstate(arg_state);
}

/* If we are using the trivial TYPE_0 R.N.G., just do the old linear
   congruential bit.  Otherwise, we do our fancy trinomial stuff, which is the
   same in all the other cases due to all the global variables that have been
   set up.  The basic operation is to add the number at the rear pointer into
   the one at the front pointer.  Then both pointers are advanced to the next
   location cyclically in the table.  The value returned is the sum generated,
   reduced to 31 bits by throwing away the "least random" low bit.
   Note: The code takes advantage of the fact that both the front and
   rear pointers can't wrap on the same call by not testing the rear
   pointer if the front one has wrapped.  Returns a 31-bit random number.  */

long int
__random (void)
{
    int32_t retval;

    __libc_lock_lock (lock);

    (void) __random_r (&unsafe_state, &retval);

    __libc_lock_unlock (lock);

    return retval;
}
inline long int random(void)
{
    return __random();
}

/* Array versions of the above information to make code run faster.
   Relies on fact that TYPE_i == i.  */

struct random_poly_info
{
    int seps[MAX_TYPES];
    int degrees[MAX_TYPES];
};

static const struct random_poly_info random_poly_info =
        {
                { SEP_0, SEP_1, SEP_2, SEP_3, SEP_4 },
                { DEG_0, DEG_1, DEG_2, DEG_3, DEG_4 }
        };

static int32_t
get_int32 (void *p)
{
    int32_t v;
    memcpy (&v, p, sizeof v);
    return v;
}

static void
set_int32 (void *p, int32_t v)
{
    memcpy (p, &v, sizeof v);
}


/* Initialize the random number generator based on the given seed.  If the
   type is the trivial no-state-information type, just remember the seed.
   Otherwise, initializes state[] based on the given "seed" via a linear
   congruential generator.  Then, the pointers are set to known locations
   that are exactly rand_sep places apart.  Lastly, it cycles the state
   information a given number of times to get rid of any initial dependencies
   introduced by the L.C.R.N.G.  Note that the initialization of randtbl[]
   for default usage relies on values produced by this routine.  */
int
__srandom_r (unsigned int seed, struct random_data *buf)
{
    int type;
    int32_t *state;
    long int i;
    int32_t word;
    int32_t *dst;
    int kc;

    if (buf == NULL)
        goto fail;
    type = buf->rand_type;
    if ((unsigned int) type >= MAX_TYPES)
        goto fail;

    state = buf->state;
    /* We must make sure the seed is not 0.  Take arbitrarily 1 in this case.  */
    if (seed == 0)
        seed = 1;
    set_int32 (&state[0], seed);
    if (type == TYPE_0)
        goto done;

    dst = state;
    word = seed;
    kc = buf->rand_deg;
    for (i = 1; i < kc; ++i)
    {
        /* This does:
             state[i] = (16807 * state[i - 1]) % 2147483647;
           but avoids overflowing 31 bits.  */
        long int hi = word / 127773;
        long int lo = word % 127773;
        word = 16807 * lo - 2836 * hi;
        if (word < 0)
            word += 2147483647;
        set_int32 (++dst, word);
    }

    buf->fptr = &state[buf->rand_sep];
    buf->rptr = &state[0];
    kc *= 10;
    while (--kc >= 0)
    {
        int32_t discard;
        (void) __random_r (buf, &discard);
    }

    done:
    return 0;

    fail:
    return -1;
}

inline int
srandom_r (unsigned int seed, struct random_data *buf)
{
    return __srandom_r(seed, buf);
}

/* Initialize the state information in the given array of N bytes for
   future random number generation.  Based on the number of bytes we
   are given, and the break values for the different R.N.G.'s, we choose
   the best (largest) one we can and set things up for it.  srandom is
   then called to initialize the state information.  Note that on return
   from srandom, we set state[-1] to be the type multiplexed with the current
   value of the rear pointer; this is so successive calls to initstate won't
   lose this information and will be able to restart with setstate.
   Note: The first thing we do is save the current state, if any, just like
   setstate so that it doesn't matter when initstate is called.
   Returns 0 on success, non-zero on failure.  */
int
__initstate_r (unsigned int seed, char *arg_state, size_t n,
               struct random_data *buf)
{
    if (buf == NULL)
        goto fail;

    int32_t *old_state = buf->state;
    if (old_state != NULL)
    {
        int old_type = buf->rand_type;
        set_int32 (&old_state[-1],
                   (old_type == TYPE_0
                    ? TYPE_0
                    : (MAX_TYPES * (buf->rptr - old_state)) + old_type));
    }

    int type;
    if (n >= BREAK_3)
        type = n < BREAK_4 ? TYPE_3 : TYPE_4;
    else if (n < BREAK_1)
    {
        if (n < BREAK_0)
            goto fail;

        type = TYPE_0;
    }
    else
        type = n < BREAK_2 ? TYPE_1 : TYPE_2;

    int degree = random_poly_info.degrees[type];
    int separation = random_poly_info.seps[type];

    buf->rand_type = type;
    buf->rand_sep = separation;
    buf->rand_deg = degree;
    int32_t *state = &((int32_t *) arg_state)[1]; /* First location.  */
    /* Must set END_PTR before srandom.  */
    buf->end_ptr = &state[degree];

    buf->state = state;

    __srandom_r (seed, buf);

    set_int32 (&state[-1],
               type == TYPE_0 ? TYPE_0 : (buf->rptr - state) * MAX_TYPES + type);

    return 0;

    fail:
    __set_errno (EINVAL);
    return -1;
}

inline int
initstate_r (unsigned int seed, char *arg_state, size_t n,
               struct random_data *buf)
{
    return __initstate_r(seed, arg_state, n, buf);
}

/* Restore the state from the given state array.
   Note: It is important that we also remember the locations of the pointers
   in the current state information, and restore the locations of the pointers
   from the old state information.  This is done by multiplexing the pointer
   location into the zeroth word of the state information. Note that due
   to the order in which things are done, it is OK to call setstate with the
   same state as the current state
   Returns 0 on success, non-zero on failure.  */
int
__setstate_r (char *arg_state, struct random_data *buf)
{
    int32_t *new_state = 1 + (int32_t *) arg_state;
    int type;
    int old_type;
    int32_t *old_state;
    int degree;
    int separation;

    if (arg_state == NULL || buf == NULL)
        goto fail;

    old_type = buf->rand_type;
    old_state = buf->state;
    set_int32 (&old_state[-1],
               (old_type == TYPE_0
                ? TYPE_0
                : (MAX_TYPES * (buf->rptr - old_state)) + old_type));

    type = get_int32 (&new_state[-1]) % MAX_TYPES;
    if (type < TYPE_0 || type > TYPE_4)
        goto fail;

    buf->rand_deg = degree = random_poly_info.degrees[type];
    buf->rand_sep = separation = random_poly_info.seps[type];
    buf->rand_type = type;

    if (type != TYPE_0)
    {
        int rear = get_int32 (&new_state[-1]) / MAX_TYPES;
        buf->rptr = &new_state[rear];
        buf->fptr = &new_state[(rear + separation) % degree];
    }
    buf->state = new_state;
    /* Set end_ptr too.  */
    buf->end_ptr = &new_state[degree];

    return 0;

    fail:
    __set_errno (EINVAL);
    return -1;
}

inline int setstate_r (char *arg_state, struct random_data *buf)
{
    return __setstate_r(arg_state, buf);
}

/* If we are using the trivial TYPE_0 R.N.G., just do the old linear
   congruential bit.  Otherwise, we do our fancy trinomial stuff, which is the
   same in all the other cases due to all the global variables that have been
   set up.  The basic operation is to add the number at the rear pointer into
   the one at the front pointer.  Then both pointers are advanced to the next
   location cyclically in the table.  The value returned is the sum generated,
   reduced to 31 bits by throwing away the "least random" low bit.
   Note: The code takes advantage of the fact that both the front and
   rear pointers can't wrap on the same call by not testing the rear
   pointer if the front one has wrapped.  Returns a 31-bit random number.  */

int
__random_r (struct random_data *buf, int32_t *result)
{
    int32_t *state;

    if (buf == NULL || result == NULL)
        goto fail;

    state = buf->state;

    if (buf->rand_type == TYPE_0)
    {
        int32_t val = (((get_int32 (&state[0]) * 1103515245U) + 12345U)
                       & 0x7fffffff);
        set_int32 (&state[0], val);
        *result = val;
    }
    else
    {
        int32_t *fptr = buf->fptr;
        int32_t *rptr = buf->rptr;
        int32_t *end_ptr = buf->end_ptr;
        /* F and R are unsigned int, not uint32_t, to avoid undefined
           overflow behavior on platforms where INT_MAX == UINT32_MAX.  */
        unsigned int f = get_int32 (fptr);
        unsigned int r = get_int32 (rptr);
        uint32_t val = f + r;
        set_int32 (fptr, val);
        /* Chucking least random bit.  */
        *result = val >> 1;
        ++fptr;
        if (fptr >= end_ptr)
        {
            fptr = state;
            ++rptr;
        }
        else
        {
            ++rptr;
            if (rptr >= end_ptr)
                rptr = state;
        }
        buf->fptr = fptr;
        buf->rptr = rptr;
    }
    return 0;

    fail:
    __set_errno (EINVAL);
    return -1;
}

inline int
random_r (struct random_data *buf, int32_t *result)
{
    return __random_r(buf, result);
}

#endif

#if !defined(HAVE_GETCWD)
char *
getcwd(path, sz)
     char *path;
     int sz;
{
	return getwd(path);
}
#endif

/* force posix signals */
void (*
bmake_signal(int s, void (*a)(int)))(int)
{
    struct sigaction sa, osa;

    sa.sa_handler = a;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(s, &sa, &osa) == -1)
	return SIG_ERR;
    else
	return osa.sa_handler;
}

#if !defined(HAVE_VSNPRINTF) || !defined(HAVE_VASPRINTF)
#include <stdarg.h>
#endif

#if !defined(HAVE_VSNPRINTF)
#if !defined(__osf__)
#ifdef _IOSTRG
#define STRFLAG	(_IOSTRG|_IOWRT)	/* no _IOWRT: avoid stdio bug */
#else
#if 0
#define STRFLAG	(_IOREAD)		/* XXX: Assume svr4 stdio */
#endif
#endif /* _IOSTRG */
#endif /* __osf__ */

int
vsnprintf(char *s, size_t n, const char *fmt, va_list args)
{
#ifdef STRFLAG
	FILE fakebuf;

	fakebuf._flag = STRFLAG;
	/*
	 * Some os's are char * _ptr, others are unsigned char *_ptr...
	 * We cast to void * to make everyone happy.
	 */
	fakebuf._ptr = (void *)s;
	fakebuf._cnt = n-1;
	fakebuf._file = -1;
	_doprnt(fmt, args, &fakebuf);
	fakebuf._cnt++;
	putc('\0', &fakebuf);
	if (fakebuf._cnt<0)
	    fakebuf._cnt = 0;
	return (n-fakebuf._cnt-1);
#else
#ifndef _PATH_DEVNULL
# define _PATH_DEVNULL "/dev/null"
#endif
	/*
	 * Rats... we don't want to clobber anything...
	 * do a printf to /dev/null to see how much space we need.
	 */
	static FILE *nullfp;
	int need = 0;			/* XXX what's a useful error return? */

	if (!nullfp)
		nullfp = fopen(_PATH_DEVNULL, "w");
	if (nullfp) {
		need = vfprintf(nullfp, fmt, args);
		if (need < n)
			(void)vsprintf(s, fmt, args);
	}
	return need;
#endif
}
#endif

#if !defined(HAVE_SNPRINTF)
int
snprintf(char *s, size_t n, const char *fmt, ...)
{
	va_list ap;
	int rv;

	va_start(ap, fmt);
	rv = vsnprintf(s, n, fmt, ap);
	va_end(ap);
	return rv;
}
#endif
		
#if !defined(HAVE_STRFTIME)
size_t
strftime(char *buf, size_t len, const char *fmt, const struct tm *tm)
{
	static char months[][4] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	size_t s;
	char *b = buf;

	while (*fmt) {
		if (len == 0)
			return buf - b;
		if (*fmt != '%') {
			*buf++ = *fmt++;
			len--;
			continue;
		}
		switch (*fmt++) {
		case '%':
			*buf++ = '%';
			len--;
			if (len == 0) return buf - b;
			/*FALLTHROUGH*/
		case '\0':
			*buf = '%';
			s = 1;
			break;
		case 'k':
			s = snprintf(buf, len, "%d", tm->tm_hour);
			break;
		case 'M':
			s = snprintf(buf, len, "%02d", tm->tm_min);
			break;
		case 'S':
			s = snprintf(buf, len, "%02d", tm->tm_sec);
			break;
		case 'b':
			if (tm->tm_mon >= 12)
				return buf - b;
			s = snprintf(buf, len, "%s", months[tm->tm_mon]);
			break;
		case 'd':
			s = snprintf(buf, len, "%02d", tm->tm_mday);
			break;
		case 'Y':
			s = snprintf(buf, len, "%d", 1900 + tm->tm_year);
			break;
		default:
			s = snprintf(buf, len, "Unsupported format %c",
			    fmt[-1]);
			break;
		}
		buf += s;
		len -= s;
	}
}
#endif

#if !defined(HAVE_STRERROR)
extern int errno, sys_nerr;
extern char *sys_errlist[];

char *
strerror(int e)
{
    static char buf[100];
    if (e < 0 || e >= sys_nerr) {
        snprintf(buf, sizeof(buf), "Unknown error %d", e);
        return buf;
    }
    else
        return sys_errlist[e];
}
#endif

#if !defined(HAVE_KILLPG)
#if !defined(__hpux__) && !defined(__hpux)
int
killpg(int pid, int sig)
{
    return kill(-pid, sig);
}
#endif
#endif

#if !defined(HAVE_WARNX)
static void
vwarnx(const char *fmt, va_list args)
{
	fprintf(stderr, "%s: ", progname);
	if ((fmt)) {
		vfprintf(stderr, fmt, args);
		fprintf(stderr, ": ");
	}
}
#endif

#if !defined(HAVE_WARN)
static void
vwarn(const char *fmt, va_list args)
{
	vwarnx(fmt, args);
	fprintf(stderr, "%s\n", strerror(errno));
}
#endif

#if !defined(HAVE_ERR)
static void
verr(int eval, const char *fmt, va_list args)
{
	vwarn(fmt, args);
	exit(eval);
}
#endif

#if !defined(HAVE_ERRX)
static void
verrx(int eval, const char *fmt, va_list args)
{
	vwarnx(fmt, args);
	exit(eval);
}
#endif

#if !defined(HAVE_ERR)
void
err(int eval, const char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        verr(eval, fmt, ap);
        va_end(ap);
}
#endif

#if !defined(HAVE_ERRX)
void
errx(int eval, const char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        verrx(eval, fmt, ap);
        va_end(ap);
}
#endif

#if !defined(HAVE_KILL) && (defined _WIN32 && ! defined __CYGWIN__)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>

/* The return value of spawnvp() is really a process handle as returned
   by CreateProcess().  Therefore we can kill it using TerminateProcess.  */
int kill(pid_t pid, int sig)
{
    return TerminateProcess ((HANDLE) (pid), sig);
}
#endif

#if (defined _WIN32 && ! defined __CYGWIN__) //poll function
/* Emulation for poll(2)
   Contributed by Paolo Bonzini.

   Copyright 2001-2003, 2006-2018 Free Software Foundation, Inc.

   This file is part of gnulib.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <https://www.gnu.org/licenses/>.  */

/* Tell gcc not to warn about the (nfd < 0) tests, below.  */
#if (__GNUC__ == 4 && 3 <= __GNUC_MINOR__) || 4 < __GNUC__
# pragma GCC diagnostic ignored "-Wtype-limits"
#endif

//#include <alloca.h>

#include <sys/types.h>

/* Specification.  */
#include "headers-mingw/poll.h"

#include <errno.h>
#include <limits.h>

#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <stdio.h>
#include <conio.h>
#if GNULIB_MSVC_NOTHROW
# include "msvc-nothrow.h"
#else
# include <io.h>
#endif

//#include <sys/select.h>
//#include <sys/socket.h>

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#include <time.h>

//#include "assure.h"

#ifndef INFTIM
# define INFTIM (-1)
#endif

/* BeOS does not have MSG_PEEK.  */
#ifndef MSG_PEEK
# define MSG_PEEK 0
#endif

/* Avoid warnings from gcc -Wcast-function-type.  */
# define GetProcAddress \
   (void *) GetProcAddress

static BOOL IsConsoleHandle (HANDLE h)
{
  DWORD mode;
  return GetConsoleMode (h, &mode) != 0;
}

static BOOL
IsSocketHandle (HANDLE h)
{
  WSANETWORKEVENTS ev;

  if (IsConsoleHandle (h))
    return FALSE;

  /* Under Wine, it seems that getsockopt returns 0 for pipes too.
     WSAEnumNetworkEvents instead distinguishes the two correctly.  */
  ev.lNetworkEvents = 0xDEADBEEF;
  WSAEnumNetworkEvents ((SOCKET) h, NULL, &ev);
  return ev.lNetworkEvents != 0xDEADBEEF;
}

typedef DWORD (WINAPI *PNtQueryInformationFile)
         (HANDLE, IO_STATUS_BLOCK *, VOID *, ULONG, FILE_INFORMATION_CLASS);

# ifndef PIPE_BUF
#  define PIPE_BUF      512
# endif

/* Compute revents values for file handle H.  If some events cannot happen
   for the handle, eliminate them from *P_SOUGHT.  */

static int
windows_compute_revents (HANDLE h, int *p_sought)
{
  int i, ret, happened;
  INPUT_RECORD *irbuffer;
  DWORD avail, nbuffer;
  BOOL bRet;
  IO_STATUS_BLOCK iosb;
  FILE_PIPE_LOCAL_INFORMATION fpli;
  static PNtQueryInformationFile NtQueryInformationFile;
  static BOOL once_only;

  switch (GetFileType (h))
    {
    case FILE_TYPE_PIPE:
      if (!once_only)
        {
          NtQueryInformationFile = (PNtQueryInformationFile)
            GetProcAddress (GetModuleHandle ("ntdll.dll"),
                            "NtQueryInformationFile");
          once_only = TRUE;
        }

      happened = 0;
      if (PeekNamedPipe (h, NULL, 0, NULL, &avail, NULL) != 0)
        {
          if (avail)
            happened |= *p_sought & (POLLIN | POLLRDNORM);
        }
      else if (GetLastError () == ERROR_BROKEN_PIPE)
        happened |= POLLHUP;

      else
        {
          /* It was the write-end of the pipe.  Check if it is writable.
             If NtQueryInformationFile fails, optimistically assume the pipe is
             writable.  This could happen on Windows 9x, where
             NtQueryInformationFile is not available, or if we inherit a pipe
             that doesn't permit FILE_READ_ATTRIBUTES access on the write end
             (I think this should not happen since Windows XP SP2; WINE seems
             fine too).  Otherwise, ensure that enough space is available for
             atomic writes.  */
          memset (&iosb, 0, sizeof (iosb));
          memset (&fpli, 0, sizeof (fpli));

          if (!NtQueryInformationFile
              || NtQueryInformationFile (h, &iosb, &fpli, sizeof (fpli),
                                         FilePipeLocalInformation)
              || fpli.WriteQuotaAvailable >= PIPE_BUF
              || (fpli.OutboundQuota < PIPE_BUF &&
                  fpli.WriteQuotaAvailable == fpli.OutboundQuota))
            happened |= *p_sought & (POLLOUT | POLLWRNORM | POLLWRBAND);
        }
      return happened;

    case FILE_TYPE_CHAR:
      ret = WaitForSingleObject (h, 0);
      if (!IsConsoleHandle (h))
        return ret == WAIT_OBJECT_0 ? *p_sought & ~(POLLPRI | POLLRDBAND) : 0;

      nbuffer = avail = 0;
      bRet = GetNumberOfConsoleInputEvents (h, &nbuffer);
      if (bRet)
        {
          /* Input buffer.  */
          *p_sought &= POLLIN | POLLRDNORM;
          if (nbuffer == 0)
            return POLLHUP;
          if (!*p_sought)
            return 0;

          irbuffer = (INPUT_RECORD *) alloca (nbuffer * sizeof (INPUT_RECORD));
          bRet = PeekConsoleInput (h, irbuffer, nbuffer, &avail);
          if (!bRet || avail == 0)
            return POLLHUP;

          for (i = 0; i < avail; i++)
            if (irbuffer[i].EventType == KEY_EVENT)
              return *p_sought;
          return 0;
        }
      else
        {
          /* Screen buffer.  */
          *p_sought &= POLLOUT | POLLWRNORM | POLLWRBAND;
          return *p_sought;
        }

    default:
      ret = WaitForSingleObject (h, 0);
      if (ret == WAIT_OBJECT_0)
        return *p_sought & ~(POLLPRI | POLLRDBAND);

      return *p_sought & (POLLOUT | POLLWRNORM | POLLWRBAND);
    }
}

/* Convert fd_sets returned by select into revents values.  */

static int
windows_compute_revents_socket (SOCKET h, int sought, long lNetworkEvents)
{
  int happened = 0;

  if ((lNetworkEvents & (FD_READ | FD_ACCEPT | FD_CLOSE)) == FD_ACCEPT)
    happened |= (POLLIN | POLLRDNORM) & sought;

  else if (lNetworkEvents & (FD_READ | FD_ACCEPT | FD_CLOSE))
    {
      int r, error;

      char data[64];
      WSASetLastError (0);
      r = recv (h, data, sizeof (data), MSG_PEEK);
      error = WSAGetLastError ();
      WSASetLastError (0);

      if (r > 0 || error == WSAENOTCONN)
        happened |= (POLLIN | POLLRDNORM) & sought;

      /* Distinguish hung-up sockets from other errors.  */
      else if (r == 0 || error == WSAESHUTDOWN || error == WSAECONNRESET
               || error == WSAECONNABORTED || error == WSAENETRESET)
        happened |= POLLHUP;

      else
        happened |= POLLERR;
    }

  if (lNetworkEvents & (FD_WRITE | FD_CONNECT))
    happened |= (POLLOUT | POLLWRNORM | POLLWRBAND) & sought;

  if (lNetworkEvents & FD_OOB)
    happened |= (POLLPRI | POLLRDBAND) & sought;

  return happened;
}

int
poll (struct pollfd *pfd, nfds_t nfd, int timeout)
{
  static struct timeval tv0;
  static HANDLE hEvent;
  WSANETWORKEVENTS ev;
  HANDLE h, handle_array[FD_SETSIZE + 2];
  DWORD ret, wait_timeout, nhandles;
  fd_set rfds, wfds, xfds;
  BOOL poll_again;
  MSG msg;
  int rc = 0;
  nfds_t i;

  if (nfd > INT_MAX || timeout < -1)
    {
      errno = EINVAL;
      return -1;
    }

  if (!hEvent)
    hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);

restart:
  handle_array[0] = hEvent;
  nhandles = 1;
  FD_ZERO (&rfds);
  FD_ZERO (&wfds);
  FD_ZERO (&xfds);

  /* Classify socket handles and create fd sets. */
  for (i = 0; i < nfd; i++)
    {
      int sought = pfd[i].events;
      pfd[i].revents = 0;
      if (pfd[i].fd < 0)
        continue;
      if (!(sought & (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM | POLLWRBAND
                      | POLLPRI | POLLRDBAND)))
        continue;

      h = (HANDLE) _get_osfhandle (pfd[i].fd);
      if (IsSocketHandle (h))
        {
          int requested = FD_CLOSE;

          /* see above; socket handles are mapped onto select.  */
          if (sought & (POLLIN | POLLRDNORM))
            {
              requested |= FD_READ | FD_ACCEPT;
              FD_SET ((SOCKET) h, &rfds);
            }
          if (sought & (POLLOUT | POLLWRNORM | POLLWRBAND))
            {
              requested |= FD_WRITE | FD_CONNECT;
              FD_SET ((SOCKET) h, &wfds);
            }
          if (sought & (POLLPRI | POLLRDBAND))
            {
              requested |= FD_OOB;
              FD_SET ((SOCKET) h, &xfds);
            }

          if (requested)
            WSAEventSelect ((SOCKET) h, hEvent, requested);
        }
      else
        {
          /* Poll now.  If we get an event, do not poll again.  Also,
             screen buffer handles are waitable, and they'll block until
             a character is available.  windows_compute_revents eliminates
             bits for the "wrong" direction. */
          pfd[i].revents = windows_compute_revents (h, &sought);
          if (sought)
            handle_array[nhandles++] = h;
          if (pfd[i].revents)
            timeout = 0;
        }
    }

  if (select (0, &rfds, &wfds, &xfds, &tv0) > 0)
    {
      /* Do MsgWaitForMultipleObjects anyway to dispatch messages, but
         no need to call select again.  */
      poll_again = FALSE;
      wait_timeout = 0;
    }
  else
    {
      poll_again = TRUE;
      if (timeout == INFTIM)
        wait_timeout = INFINITE;
      else
        wait_timeout = timeout;
    }

  for (;;)
    {
      ret = MsgWaitForMultipleObjects (nhandles, handle_array, FALSE,
                                       wait_timeout, QS_ALLINPUT);

      if (ret == WAIT_OBJECT_0 + nhandles)
        {
          /* new input of some other kind */
          BOOL bRet;
          while ((bRet = PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) != 0)
            {
              TranslateMessage (&msg);
              DispatchMessage (&msg);
            }
        }
      else
        break;
    }

  if (poll_again)
    select (0, &rfds, &wfds, &xfds, &tv0);

  /* Place a sentinel at the end of the array.  */
  handle_array[nhandles] = NULL;
  nhandles = 1;
  for (i = 0; i < nfd; i++)
    {
      int happened;

      if (pfd[i].fd < 0)
        continue;
      if (!(pfd[i].events & (POLLIN | POLLRDNORM |
                             POLLOUT | POLLWRNORM | POLLWRBAND)))
        continue;

      h = (HANDLE) _get_osfhandle (pfd[i].fd);
      if (h != handle_array[nhandles])
        {
          /* It's a socket.  */
          WSAEnumNetworkEvents ((SOCKET) h, NULL, &ev);
          WSAEventSelect ((SOCKET) h, 0, 0);

          /* If we're lucky, WSAEnumNetworkEvents already provided a way
             to distinguish FD_READ and FD_ACCEPT; this saves a recv later.  */
          if (FD_ISSET ((SOCKET) h, &rfds)
              && !(ev.lNetworkEvents & (FD_READ | FD_ACCEPT)))
            ev.lNetworkEvents |= FD_READ | FD_ACCEPT;
          if (FD_ISSET ((SOCKET) h, &wfds))
            ev.lNetworkEvents |= FD_WRITE | FD_CONNECT;
          if (FD_ISSET ((SOCKET) h, &xfds))
            ev.lNetworkEvents |= FD_OOB;

          happened = windows_compute_revents_socket ((SOCKET) h, pfd[i].events,
                                                     ev.lNetworkEvents);
        }
      else
        {
          /* Not a socket.  */
          int sought = pfd[i].events;
          happened = windows_compute_revents (h, &sought);
          nhandles++;
        }

       if ((pfd[i].revents |= happened) != 0)
        rc++;
    }

  if (!rc && timeout == INFTIM)
    {
      SleepEx (1, TRUE);
      goto restart;
    }

  return rc;
}

#endif //(defined _WIN32 && ! defined __CYGWIN__) (poll function)

#if !defined(HAVE_SIGEMPTYSET) || !defined(HAVE_SIGISMEMBER) || !defined(HAVE_SIGPROCMASK) || !defined(HAVE_SIGACTION)
/* POSIX compatible signal blocking.
   Copyright (C) 2006-2018 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2006.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Specification.  */
#include <signal.h>
#if (defined _WIN32 && ! defined __CYGWIN__)
#include "headers-mingw/signal.h"
#endif

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#if (defined _WIN32 && ! defined __CYGWIN__)
#include "headers-mingw/stdlib.h"
#endif

#if HAVE_MSVC_INVALID_PARAMETER_HANDLER
# include "msvc-inval.h"
#endif

/* We assume that a platform without POSIX signal blocking functions
   also does not have the POSIX sigaction() function, only the
   signal() function.  We also assume signal() has SysV semantics,
   where any handler is uninstalled prior to being invoked.  This is
   true for native Windows platforms.  */

/* We use raw signal(), but also provide a wrapper rpl_signal() so
   that applications can query or change a blocked signal.  */
#undef signal

/* Provide invalid signal numbers as fallbacks if the uncatchable
   signals are not defined.  */
#ifndef SIGKILL
# define SIGKILL (-1)
#endif
#ifndef SIGSTOP
# define SIGSTOP (-1)
#endif

/* On native Windows, as of 2008, the signal SIGABRT_COMPAT is an alias
   for the signal SIGABRT.  Only one signal handler is stored for both
   SIGABRT and SIGABRT_COMPAT.  SIGABRT_COMPAT is not a signal of its own.  */
#if defined _WIN32 && ! defined __CYGWIN__
# undef SIGABRT_COMPAT
# define SIGABRT_COMPAT 6
#endif
#ifdef SIGABRT_COMPAT
# define SIGABRT_COMPAT_MASK (1U << SIGABRT_COMPAT)
#else
# define SIGABRT_COMPAT_MASK 0
#endif

typedef void (*handler_t) (int);

#if HAVE_MSVC_INVALID_PARAMETER_HANDLER
static handler_t
signal_nothrow (int sig, handler_t handler)
{
  handler_t result;

  TRY_MSVC_INVAL
    {
      result = signal (sig, handler);
    }
  CATCH_MSVC_INVAL
    {
      result = SIG_ERR;
      errno = EINVAL;
    }
  DONE_MSVC_INVAL;

  return result;
}
# define signal signal_nothrow
#endif

/* Handling of gnulib defined signals.  */

#if GNULIB_defined_SIGPIPE
static handler_t SIGPIPE_handler = SIG_DFL;
#endif

#if GNULIB_defined_SIGPIPE
static handler_t
ext_signal (int sig, handler_t handler)
{
  switch (sig)
    {
    case SIGPIPE:
      {
        handler_t old_handler = SIGPIPE_handler;
        SIGPIPE_handler = handler;
        return old_handler;
      }
    default: /* System defined signal */
      return signal (sig, handler);
    }
}
# undef signal
# define signal ext_signal
#endif

int
sigismember (const sigset_t *set, int sig)
{
    if (sig >= 0 && sig < NSIG)
    {
#ifdef SIGABRT_COMPAT
        if (sig == SIGABRT_COMPAT)
        sig = SIGABRT;
#endif

        return (*set >> sig) & 1;
    }
    else
        return 0;
}

int
sigemptyset (sigset_t *set)
{
    *set = 0;
    return 0;
}

int
sigaddset (sigset_t *set, int sig)
{
    if (sig >= 0 && sig < NSIG)
    {
#ifdef SIGABRT_COMPAT
        if (sig == SIGABRT_COMPAT)
        sig = SIGABRT;
#endif

        *set |= 1U << sig;
        return 0;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}

int
sigdelset (sigset_t *set, int sig)
{
    if (sig >= 0 && sig < NSIG)
    {
#ifdef SIGABRT_COMPAT
        if (sig == SIGABRT_COMPAT)
        sig = SIGABRT;
#endif

        *set &= ~(1U << sig);
        return 0;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
}


int
sigfillset (sigset_t *set)
{
    *set = ((2U << (NSIG - 1)) - 1) & ~ SIGABRT_COMPAT_MASK;
    return 0;
}

/* Set of currently blocked signals.  */
static volatile sigset_t blocked_set /* = 0 */;

/* Set of currently blocked and pending signals.  */
static volatile sig_atomic_t pending_array[NSIG] /* = { 0 } */;

/* Signal handler that is installed for blocked signals.  */
static void
blocked_handler (int sig)
{
    /* Reinstall the handler, in case the signal occurs multiple times
       while blocked.  There is an inherent race where an asynchronous
       signal in between when the kernel uninstalled the handler and
       when we reinstall it will trigger the default handler; oh
       well.  */
    signal (sig, blocked_handler);
    if (sig >= 0 && sig < NSIG)
        pending_array[sig] = 1;
}

int
sigpending (sigset_t *set)
{
    sigset_t pending = 0;
    int sig;

    for (sig = 0; sig < NSIG; sig++)
        if (pending_array[sig])
            pending |= 1U << sig;
    *set = pending;
    return 0;
}

/* The previous signal handlers.
   Only the array elements corresponding to blocked signals are relevant.  */
static volatile handler_t old_handlers[NSIG];

int
sigprocmask (int operation, const sigset_t *set, sigset_t *old_set)
{
    if (old_set != NULL)
        *old_set = blocked_set;

    if (set != NULL)
    {
        sigset_t new_blocked_set;
        sigset_t to_unblock;
        sigset_t to_block;

        switch (operation)
        {
            case SIG_BLOCK:
                new_blocked_set = blocked_set | *set;
                break;
            case SIG_SETMASK:
                new_blocked_set = *set;
                break;
            case SIG_UNBLOCK:
                new_blocked_set = blocked_set & ~*set;
                break;
            default:
                errno = EINVAL;
                return -1;
        }
        to_unblock = blocked_set & ~new_blocked_set;
        to_block = new_blocked_set & ~blocked_set;

        if (to_block != 0)
        {
            int sig;

            for (sig = 0; sig < NSIG; sig++)
                if ((to_block >> sig) & 1)
                {
                    pending_array[sig] = 0;
                    if ((old_handlers[sig] = signal (sig, blocked_handler)) != SIG_ERR)
                        blocked_set |= 1U << sig;
                }
        }

        if (to_unblock != 0)
        {
            sig_atomic_t received[NSIG];
            int sig;

            for (sig = 0; sig < NSIG; sig++)
                if ((to_unblock >> sig) & 1)
                {
//                    if (signal (sig, old_handlers[sig]) != blocked_handler)
//                        /* The application changed a signal handler while the signal
//                           was blocked, bypassing our rpl_signal replacement.
//                           We don't support this.  */
//                        abort ();
                    received[sig] = pending_array[sig];
                    blocked_set &= ~(1U << sig);
                    pending_array[sig] = 0;
                }
                else
                    received[sig] = 0;

            for (sig = 0; sig < NSIG; sig++)
                if (received[sig])
                    raise (sig);
        }
    }
    return 0;
}

/* Install the handler FUNC for signal SIG, and return the previous
   handler.  */
handler_t
rpl_signal (int sig, handler_t handler)
{
    /* We must provide a wrapper, so that a user can query what handler
       they installed even if that signal is currently blocked.  */
    if (sig >= 0 && sig < NSIG && sig != SIGKILL && sig != SIGSTOP
        && handler != SIG_ERR)
    {
#ifdef SIGABRT_COMPAT
        if (sig == SIGABRT_COMPAT)
        sig = SIGABRT;
#endif

        if (blocked_set & (1U << sig))
        {
            /* POSIX states that sigprocmask and signal are both
               async-signal-safe.  This is not true of our
               implementation - there is a slight data race where an
               asynchronous interrupt on signal A can occur after we
               install blocked_handler but before we have updated
               old_handlers for signal B, such that handler A can see
               stale information if it calls signal(B).  Oh well -
               signal handlers really shouldn't try to manipulate the
               installed handlers of unrelated signals.  */
            handler_t result = old_handlers[sig];
            old_handlers[sig] = handler;
            return result;
        }
        else
            return signal (sig, handler);
    }
    else
    {
        errno = EINVAL;
        return SIG_ERR;
    }
}

/* This implementation of sigaction is tailored to native Windows behavior:
   signal() has SysV semantics (ie. the handler is uninstalled before
   it is invoked).  This is an inherent data race if an asynchronous
   signal is sent twice in a row before we can reinstall our handler,
   but there's nothing we can do about it.  Meanwhile, sigprocmask()
   is not present, and while we can use the gnulib replacement to
   provide critical sections, it too suffers from potential data races
   in the face of an ill-timed asynchronous signal.  And we compound
   the situation by reading static storage in a signal handler, which
   POSIX warns is not generically async-signal-safe.  Oh well.

   Additionally:
     - We don't implement SA_NOCLDSTOP or SA_NOCLDWAIT, because SIGCHLD
       is not defined.
     - We don't implement SA_ONSTACK, because sigaltstack() is not present.
     - We ignore SA_RESTART, because blocking native Windows API calls are
       not interrupted anyway when an asynchronous signal occurs, and the
       MSVCRT runtime never sets errno to EINTR.
     - We don't implement SA_SIGINFO because it is impossible to do so
       portably.

   POSIX states that an application should not mix signal() and
   sigaction().  We support the use of signal() within the gnulib
   sigprocmask() substitute, but all other application code linked
   with this module should stick with only sigaction().  */

/* Set of current actions.  If sa_handler for an entry is NULL, then
   that signal is not currently handled by the sigaction handler.  */
static struct sigaction volatile action_array[NSIG] /* = 0 */;

/* Signal handler that is installed for signals.  */
static void
sigaction_handler (int sig)
{
    handler_t handler;
    sigset_t mask;
    sigset_t oldmask;
    int saved_errno = errno;
    if (sig < 0 || NSIG <= sig || !action_array[sig].sa_handler)
    {
        /* Unexpected situation; be careful to avoid recursive abort.  */
        if (sig == SIGABRT)
            signal (SIGABRT, SIG_DFL);
        abort ();
    }

    /* Reinstall the signal handler when required; otherwise update the
       bookkeeping so that the user's handler may call sigaction and get
       accurate results.  We know the signal isn't currently blocked, or
       we wouldn't be in its handler, therefore we know that we are not
       interrupting a sigaction() call.  There is a race where any
       asynchronous instance of the same signal occurring before we
       reinstall the handler will trigger the default handler; oh
       well.  */
    handler = action_array[sig].sa_handler;
    if ((action_array[sig].sa_flags & SA_RESETHAND) == 0)
        signal (sig, sigaction_handler);
    else
        action_array[sig].sa_handler = NULL;

    /* Block appropriate signals.  */
    mask = action_array[sig].sa_mask;
    if ((action_array[sig].sa_flags & SA_NODEFER) == 0)
        sigaddset (&mask, sig);
    sigprocmask (SIG_BLOCK, &mask, &oldmask);

    /* Invoke the user's handler, then restore prior mask.  */
    errno = saved_errno;
    handler (sig);
    saved_errno = errno;
    sigprocmask (SIG_SETMASK, &oldmask, NULL);
    errno = saved_errno;
}

/* Change and/or query the action that will be taken on delivery of
   signal SIG.  If not NULL, ACT describes the new behavior.  If not
   NULL, OACT is set to the prior behavior.  Return 0 on success, or
   set errno and return -1 on failure.  */
int
sigaction (int sig, const struct sigaction *restrict act,
           struct sigaction *restrict oact)
{
    sigset_t mask;
    sigset_t oldmask;
    int saved_errno;

    if (sig < 0 || NSIG <= sig || sig == SIGKILL || sig == SIGSTOP
        || (act && act->sa_handler == SIG_ERR))
    {
        errno = EINVAL;
        return -1;
    }

#ifdef SIGABRT_COMPAT
    if (sig == SIGABRT_COMPAT)
    sig = SIGABRT;
#endif

    /* POSIX requires sigaction() to be async-signal-safe.  In other
       words, if an asynchronous signal can occur while we are anywhere
       inside this function, the user's handler could then call
       sigaction() recursively and expect consistent results.  We meet
       this rule by using sigprocmask to block all signals before
       modifying any data structure that could be read from a signal
       handler; this works since we know that the gnulib sigprocmask
       replacement does not try to use sigaction() from its handler.  */
    if (!act && !oact)
        return 0;
    sigfillset (&mask);
    sigprocmask (SIG_BLOCK, &mask, &oldmask);
    if (oact)
    {
        if (action_array[sig].sa_handler)
            *oact = action_array[sig];
        else
        {
            /* Safe to change the handler at will here, since all
               signals are currently blocked.  */
            oact->sa_handler = signal (sig, SIG_DFL);
            if (oact->sa_handler == SIG_ERR)
                goto failure;
            signal (sig, oact->sa_handler);
            oact->sa_flags = SA_RESETHAND | SA_NODEFER;
            sigemptyset (&oact->sa_mask);
        }
    }

    if (act)
    {
        /* Safe to install the handler before updating action_array,
           since all signals are currently blocked.  */
        if (act->sa_handler == SIG_DFL || act->sa_handler == SIG_IGN)
        {
            if (signal (sig, act->sa_handler) == SIG_ERR)
                goto failure;
            action_array[sig].sa_handler = NULL;
        }
        else
        {
            if (signal (sig, sigaction_handler) == SIG_ERR)
                goto failure;
            action_array[sig] = *act;
        }
    }
    sigprocmask (SIG_SETMASK, &oldmask, NULL);
    return 0;

    failure:
    saved_errno = errno;
    sigprocmask (SIG_SETMASK, &oldmask, NULL);
    errno = saved_errno;
    return -1;
}

#endif

#if !defined(HAVE_UNAME)
/* uname replacement.
   Copyright (C) 2009-2018 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Specification.  */
#if (defined _WIN32 && ! defined __CYGWIN__)
#include "headers-mingw/sys_utsname.h"
#else
#include <sys/utsname.h>
#endif


/* This file provides an implementation only for the native Windows API.  */

#include <stdio.h>
#include <stdlib.h>
#if (defined _WIN32 && ! defined __CYGWIN__)
#include "headers-mingw/stdlib.h"
#endif
#include <string.h>
#include <unistd.h>
#if (defined _WIN32 && ! defined __CYGWIN__)
#include "headers-mingw/unistd.h"
#endif
#include <windows.h>

/* Mingw headers don't have all the platform codes.  */
#ifndef VER_PLATFORM_WIN32_CE
# define VER_PLATFORM_WIN32_CE 3
#endif

/* Some headers don't have all the processor architecture codes.  */
#ifndef PROCESSOR_ARCHITECTURE_AMD64
# define PROCESSOR_ARCHITECTURE_AMD64 9
#endif
#ifndef PROCESSOR_ARCHITECTURE_IA32_ON_WIN64
# define PROCESSOR_ARCHITECTURE_IA32_ON_WIN64 10
#endif

/* Mingw headers don't have the latest processor codes.  */
#ifndef PROCESSOR_AMD_X8664
# define PROCESSOR_AMD_X8664 8664
#endif

int
uname (struct utsname *buf)
{
  OSVERSIONINFO version;
  OSVERSIONINFOEX versionex;
  BOOL have_versionex; /* indicates whether versionex is filled */
  const char *super_version;

  /* Preparation: Fill version and, if possible, also versionex.
     But try to call GetVersionEx only once in the common case.  */
  versionex.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEX);
  have_versionex = GetVersionEx ((OSVERSIONINFO *) &versionex);
  if (have_versionex)
    {
      /* We know that OSVERSIONINFO is a subset of OSVERSIONINFOEX.  */
      memcpy (&version, &versionex, sizeof (OSVERSIONINFO));
    }
  else
    {
      version.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
      if (!GetVersionEx (&version))
        abort ();
    }

  /* Fill in nodename.  */
  if (gethostname (buf->nodename, sizeof (buf->nodename)) < 0)
    strcpy (buf->nodename, "localhost");

  /* Determine major-major Windows version.  */
  if (version.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
      /* Windows NT or newer.  */
      super_version = "NT";
    }
  else if (version.dwPlatformId == VER_PLATFORM_WIN32_CE)
    {
      /* Windows CE or Embedded CE.  */
      super_version = "CE";
    }
  else if (version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
    {
      /* Windows 95/98/ME.  */
      switch (version.dwMinorVersion)
        {
        case 0:
          super_version = "95";
          break;
        case 10:
          super_version = "98";
          break;
        case 90:
          super_version = "ME";
          break;
        default:
          super_version = "";
          break;
        }
    }
  else
    super_version = "";

  /* Fill in sysname.  */
#ifdef __MINGW32__
  /* Returns a string compatible with the MSYS uname.exe program,
     so that no further changes are needed to GNU config.guess.
     For example,
       $ ./uname.exe -s      => MINGW32_NT-5.1
   */
  sprintf (buf->sysname, "MINGW32_%s-%u.%u", super_version,
           (unsigned int) version.dwMajorVersion,
           (unsigned int) version.dwMinorVersion);
#else
  sprintf (buf->sysname, "Windows%s", super_version);
#endif

  /* Fill in release, version.  */
  /* The MSYS uname.exe programs uses strings from a modified Cygwin runtime:
       $ ./uname.exe -r      => 1.0.11(0.46/3/2)
       $ ./uname.exe -v      => 2008-08-25 23:40
     There is no point in imitating this behaviour.  */
  if (version.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
      /* Windows NT or newer.  */
      struct windows_version
        {
          int major;
          int minor;
          unsigned int server_offset;
          const char *name;
        };

      /* Storing the workstation and server version names in a single
         stream does not waste memory when they are the same.  These
         macros abstract the representation.  VERSION1 is used if
         version.wProductType does not matter, VERSION2 if it does.  */
      #define VERSION1(major, minor, name) \
        { major, minor, 0, name }
      #define VERSION2(major, minor, workstation, server) \
        { major, minor, sizeof workstation, workstation "\0" server }
      static const struct windows_version versions[] =
        {
          VERSION2 (3, -1, "Windows NT Workstation", "Windows NT Server"),
          VERSION2 (4, -1, "Windows NT Workstation", "Windows NT Server"),
          VERSION1 (5, 0, "Windows 2000"),
          VERSION1 (5, 1, "Windows XP"),
          VERSION1 (5, 2, "Windows Server 2003"),
          VERSION2 (6, 0, "Windows Vista", "Windows Server 2008"),
          VERSION2 (6, 1, "Windows 7", "Windows Server 2008 R2"),
          VERSION2 (-1, -1, "Windows", "Windows Server")
        };
      const char *base;
      const struct windows_version *v = versions;

      /* Find a version that matches ours.  The last element is a
         wildcard that always ends the loop.  */
      while ((v->major != version.dwMajorVersion && v->major != -1)
             || (v->minor != version.dwMinorVersion && v->minor != -1))
        v++;

      if (have_versionex && versionex.wProductType != VER_NT_WORKSTATION)
        base = v->name + v->server_offset;
      else
        base = v->name;
      if (v->major == -1 || v->minor == -1)
        sprintf (buf->release, "%s %u.%u",
                 base,
                 (unsigned int) version.dwMajorVersion,
                 (unsigned int) version.dwMinorVersion);
      else
        strcpy (buf->release, base);
    }
  else if (version.dwPlatformId == VER_PLATFORM_WIN32_CE)
    {
      /* Windows CE or Embedded CE.  */
      sprintf (buf->release, "Windows CE %u.%u",
               (unsigned int) version.dwMajorVersion,
               (unsigned int) version.dwMinorVersion);
    }
  else
    {
      /* Windows 95/98/ME.  */
      sprintf (buf->release, "Windows %s", super_version);
    }
  strcpy (buf->version, version.szCSDVersion);

  /* Fill in machine.  */
  {
    SYSTEM_INFO info;

    GetSystemInfo (&info);
    /* Check for Windows NT or CE, since the info.wProcessorLevel is
       garbage on Windows 95. */
    if (version.dwPlatformId == VER_PLATFORM_WIN32_NT
        || version.dwPlatformId == VER_PLATFORM_WIN32_CE)
      {
        /* Windows NT or newer, or Windows CE or Embedded CE.  */
        switch (info.wProcessorArchitecture)
          {
          case PROCESSOR_ARCHITECTURE_AMD64:
            strcpy (buf->machine, "x86_64");
            break;
          case PROCESSOR_ARCHITECTURE_IA64:
            strcpy (buf->machine, "ia64");
            break;
          case PROCESSOR_ARCHITECTURE_INTEL:
            strcpy (buf->machine, "i386");
            if (info.wProcessorLevel >= 3)
              buf->machine[1] =
                '0' + (info.wProcessorLevel <= 6 ? info.wProcessorLevel : 6);
            break;
          case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:
            strcpy (buf->machine, "i686");
            break;
          case PROCESSOR_ARCHITECTURE_MIPS:
            strcpy (buf->machine, "mips");
            break;
          case PROCESSOR_ARCHITECTURE_ALPHA:
          case PROCESSOR_ARCHITECTURE_ALPHA64:
            strcpy (buf->machine, "alpha");
            break;
          case PROCESSOR_ARCHITECTURE_PPC:
            strcpy (buf->machine, "powerpc");
            break;
          case PROCESSOR_ARCHITECTURE_SHX:
            strcpy (buf->machine, "sh");
            break;
          case PROCESSOR_ARCHITECTURE_ARM:
            strcpy (buf->machine, "arm");
            break;
          default:
            strcpy (buf->machine, "unknown");
            break;
          }
      }
    else
      {
        /* Windows 95/98/ME.  */
        switch (info.dwProcessorType)
          {
          case PROCESSOR_AMD_X8664:
            strcpy (buf->machine, "x86_64");
            break;
          case PROCESSOR_INTEL_IA64:
            strcpy (buf->machine, "ia64");
            break;
          default:
            if (info.dwProcessorType % 100 == 86)
              sprintf (buf->machine, "i%u",
                       (unsigned int) info.dwProcessorType);
            else
              strcpy (buf->machine, "unknown");
            break;
          }
      }
  }

  return 0;
}

#endif

#if !defined(HAVE_WARN)
void
warn(const char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        vwarn(fmt, ap);
        va_end(ap);
}
#endif

#if !defined(HAVE_WARNX)
void
warnx(const char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        vwarnx(fmt, ap);
        va_end(ap);
}
#endif

#if !defined(HAVE_WAIT)
/* Specification.  */
#include "wait.h"
pid_t
wait (int *statusp)
{
    return waitpid((pid_t) -1, statusp, WUNTRACED);
}
#endif

#if !defined(HAVE_WAITPID)
/* Wait for process state change.
   Copyright (C) 2001-2003, 2005-2018 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <https://www.gnu.org/licenses/>.  */

/* Specification.  */
#include "wait.h"

/* Implementation for native Windows systems.  */

#include <process.h> /* for _cwait, WAIT_CHILD */

pid_t
waitpid (pid_t pid, int *statusp, int options)
{
    return _cwait (statusp, pid, WAIT_CHILD);
}
#endif

#if (defined _WIN32 && ! defined __CYGWIN__)
char *
getShellLaunchPrefix()
{
    const char *sysRootPath = getenv("SYSROOTWINDOWSPATH");
    const char *msystemPrefix = getenv("MSYSTEM");

    if (sysRootPath == NULL || msystemPrefix == NULL) {
        return NULL; /*we can't find mingw shell installed on this system */
    }
    else {
        /* we will return something like
         * C:\msys64\usr\bin\env MSYSTEM=MINGW64 /usr/bin/bash -lc
         */
        return str_concat(
                   str_concat(
                       str_concat(sysRootPath, "usr\\bin\\env.exe MSYSTEM=", 0),
                       msystemPrefix, 0),
                   "/usr/bin/bash -lc", STR_ADDSPACE);
    }
}

char * getUnixPathCmd(const char *path)
{
    return str_concat(
            "cygpath -u", str_concat("\"",
                            str_concat(path, "\"", 0),
                            0),
                            STR_ADDSPACE);
}

#endif

#include <stdint.h>
#include <stdlib.h>
#if (defined _WIN32 && ! defined __CYGWIN__)
#include "headers-mingw/stdlib.h"
#endif
#include <string.h>

size_t str_escape(char *dst, const char *src, size_t dstLen)
{
    const char complexCharMap[] = "abtnvfr";

    size_t i;
    size_t srcLen = strlen(src);
    size_t dstIdx = 0;

    // If caller wants to determine required length (supplying NULL for dst)
    // then we set dstLen to SIZE_MAX and pretend the buffer is the largest
    // possible, but we never write to it. Caller can also provide dstLen
    // as 0 if no limit is wanted.
    if (dst == NULL || dstLen == 0) dstLen = SIZE_MAX;

    for (i = 0; i < srcLen && dstIdx < dstLen; i++)
    {
        size_t complexIdx = 0;

        switch (src[i])
        {
            case '\'':
            case '\"':
            case '\\':
                if (dst && dstIdx <= dstLen - 2)
                {
                    dst[dstIdx++] = '\\';
                    dst[dstIdx++] = src[i];
                }
                else dstIdx += 2;
                break;

            case '\r': complexIdx++;
            case '\f': complexIdx++;
            case '\v': complexIdx++;
            case '\n': complexIdx++;
            case '\t': complexIdx++;
            case '\b': complexIdx++;
            case '\a':
                if (dst && dstIdx <= dstLen - 2)
                {
                    dst[dstIdx++] = '\\';
                    dst[dstIdx++] = complexCharMap[complexIdx];
                }
                else dstIdx += 2;
                break;

            default:
                if (isprint(src[i]))
                {
                    // simply copy the character
                    if (dst)
                        dst[dstIdx++] = src[i];
                    else
                        dstIdx++;
                }
                else
                {
                    // produce octal escape sequence
                    if (dst && dstIdx <= dstLen - 4)
                    {
                        dst[dstIdx++] = '\\';
                        dst[dstIdx++] = ((src[i] & 0300) >> 6) + '0';
                        dst[dstIdx++] = ((src[i] & 0070) >> 3) + '0';
                        dst[dstIdx++] = ((src[i] & 0007) >> 0) + '0';
                    }
                    else
                    {
                        dstIdx += 4;
                    }
                }
        }
    }

    if (dst && dstIdx <= dstLen)
        dst[dstIdx] = '\0';

    return dstIdx;
}

size_t str_escape_dblquote(char *dst, const char *src, size_t dstLen)
{

    size_t i;
    size_t srcLen = strlen(src);
    size_t dstIdx = 0;

    // If caller wants to determine required length (supplying NULL for dst)
    // then we set dstLen to SIZE_MAX and pretend the buffer is the largest
    // possible, but we never write to it. Caller can also provide dstLen
    // as 0 if no limit is wanted.
    if (dst == NULL || dstLen == 0) dstLen = SIZE_MAX;

    for (i = 0; i < srcLen && dstIdx < dstLen; i++)
    {
        size_t complexIdx = 0;

        switch (src[i])
        {
            case '\"':
                if (dst && dstIdx <= dstLen - 2)
                {
                    dst[dstIdx++] = '\\';
                    dst[dstIdx++] = src[i];
                }
                else dstIdx += 2;
                break;
            case '\\':
                if (dst && dstIdx <= dstLen - 2)
                {
                    dst[dstIdx++] = '\\';
                    dst[dstIdx++] = src[i];
                }
                else dstIdx += 2;
                break;
            default:
//                if (isprint(src[i]) || src[i] == '\n' || src[i] == '\r')
//                {
                    // simply copy the character
                    if (dst)
                        dst[dstIdx++] = src[i];
                    else
                        dstIdx++;
//                }
//                else {
//                    dst[dstIdx++] = ' ';
//                }
        }
    }

    if (dst && dstIdx <= dstLen)
        dst[dstIdx] = '\0';

    return dstIdx;
}

#if (defined _WIN32 && ! defined __CYGWIN__)
#include <windows.h>
//#include <stdio.h>
//#include <malloc.h>

#define bool        _Bool
#define true        1
#define false        0

#define null ((void*)0)

typedef struct system_np_s {
    HANDLE child_stdout_read;
    HANDLE child_stderr_read;
    HANDLE reader;
    PROCESS_INFORMATION pi;
    const char* command;
    char* stdout_data;
    int   stdout_data_size;
    char* stderr_data;
    int   stderr_data_size;
    int*  exit_code;
    int   timeout; // timeout in milliseconds or -1 for INIFINTE
} system_np_t;

static int peek_pipe(HANDLE pipe, char* data, int size) {
    char buffer[4 * 1024];
    DWORD read = 0;
    DWORD available = 0;
    bool b = PeekNamedPipe(pipe, null, sizeof(buffer), null, &available, null);
    if (!b) {
        return -1;
    } else if (available > 0) {
        int bytes = min(sizeof(buffer), available);
        b = ReadFile(pipe, buffer, bytes, &read, null);
        if (!b) {
            return -1;
        }
        if (data != null && size > 0) {
            int n = min(size - 1, (int)read);
            memcpy(data, buffer, n);
            data[n + 1] = 0; // always zero terminated
            return n;
        }
    }
    return 0;
}

static DWORD WINAPI read_from_all_pipes_fully(void* p) {
    system_np_t* system = (system_np_t*)p;
    unsigned long long milliseconds = GetTickCount64(); // since boot time
    char* out = system->stdout_data != null && system->stdout_data_size > 0 ? system->stdout_data : null;
    char* err = system->stderr_data != null && system->stderr_data_size > 0 ? system->stderr_data : null;
    int out_bytes = system->stdout_data != null && system->stdout_data_size > 0 ? system->stdout_data_size - 1 : 0;
    int err_bytes = system->stderr_data != null && system->stderr_data_size > 0 ? system->stderr_data_size - 1 : 0;
    for (;;) {
        int read_stdout = peek_pipe(system->child_stdout_read, out, out_bytes);
        if (read_stdout > 0 && out != null) { out += read_stdout; out_bytes -= read_stdout; }
        int read_stderr = peek_pipe(system->child_stderr_read, err, err_bytes);
        if (read_stderr > 0 && err != null) { err += read_stderr; err_bytes -= read_stderr; }
        if (read_stdout < 0 && read_stderr < 0) { break; } // both pipes are closed
        unsigned long long time_spent_in_milliseconds = GetTickCount64() - milliseconds;
        if (system->timeout > 0 && time_spent_in_milliseconds > system->timeout) { break; }
        if (read_stdout == 0 && read_stderr == 0) { // nothing has been read from both pipes
            HANDLE handles[2] = {system->child_stdout_read, system->child_stderr_read};
            WaitForMultipleObjects(2, handles, false, 1); // wait for at least 1 millisecond (more likely 16)
        }
    }
    if (out != null) { *out = 0; }
    if (err != null) { *err = 0; }
    return 0;
}

static int create_child_process(system_np_t* system) {
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = true;
    sa.lpSecurityDescriptor = null;
    HANDLE child_stdout_write = INVALID_HANDLE_VALUE;
    HANDLE child_stderr_write = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&system->child_stderr_read, &child_stderr_write, &sa, 0) ) {
        return GetLastError();
    }
    if (!SetHandleInformation(system->child_stderr_read, HANDLE_FLAG_INHERIT, 0) ){
        return GetLastError();
    }
    if (!CreatePipe(&system->child_stdout_read, &child_stdout_write, &sa, 0) ) {
        return GetLastError();
    }
    if (!SetHandleInformation(system->child_stdout_read, HANDLE_FLAG_INHERIT, 0) ){
        return GetLastError();
    }
    // Set the text I want to run
    STARTUPINFO siStartInfo = {0};
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = child_stderr_write;
    siStartInfo.hStdOutput = child_stdout_write;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    siStartInfo.wShowWindow = SW_HIDE;
    bool b = CreateProcessA(null,
                            (char*)system->command,
                            null,               // process security attributes
                            null,               // primary thread security attributes
                            true,               // handles are inherited
                            CREATE_NO_WINDOW,   // creation flags
                            null,               // use parent's environment
                            null,               // use parent's current directory
                            &siStartInfo,       // STARTUPINFO pointer
                            &system->pi);       // receives PROCESS_INFORMATION
    int err = GetLastError();
    CloseHandle(child_stderr_write);
    CloseHandle(child_stdout_write);
    if (!b) {
        CloseHandle(system->child_stdout_read); system->child_stdout_read = INVALID_HANDLE_VALUE;
        CloseHandle(system->child_stderr_read); system->child_stderr_read = INVALID_HANDLE_VALUE;
    }
    return b ? 0 : err;
}

int system_np(const char* command, int timeout_milliseconds,
              char* stdout_data, int stdout_data_size,
              char* stderr_data, int stderr_data_size, int* exit_code) {
    system_np_t system = {0};
    if (exit_code != null) { *exit_code = 0; }
    if (stdout_data != null && stdout_data_size > 0) { stdout_data[0] = 0; }
    if (stderr_data != null && stderr_data_size > 0) { stderr_data[0] = 0; }
    system.timeout = timeout_milliseconds > 0 ? timeout_milliseconds : -1;
    system.command = command;
    system.stdout_data = stdout_data;
    system.stderr_data = stderr_data;
    system.stdout_data_size = stdout_data_size;
    system.stderr_data_size = stderr_data_size;
    int r = create_child_process(&system);
    if (r == 0) {
        system.reader = CreateThread(null, 0, read_from_all_pipes_fully, &system, 0, null);
        if (system.reader == null) { // in theory should rarely happen only when system super low on resources
            r = GetLastError();
            TerminateProcess(system.pi.hProcess, ECANCELED);
        } else {
            bool thread_done  = WaitForSingleObject(system.pi.hThread, timeout_milliseconds) == 0;
            bool process_done = WaitForSingleObject(system.pi.hProcess, timeout_milliseconds) == 0;
            if (!thread_done || !process_done) {
                TerminateProcess(system.pi.hProcess, ETIME);
            }
            if (exit_code != null) {
                GetExitCodeProcess(system.pi.hProcess, (DWORD*)exit_code);
            }
            CloseHandle(system.pi.hThread);
            CloseHandle(system.pi.hProcess);
            CloseHandle(system.child_stdout_read); system.child_stdout_read = INVALID_HANDLE_VALUE;
            CloseHandle(system.child_stderr_read); system.child_stderr_read = INVALID_HANDLE_VALUE;
            WaitForSingleObject(system.reader, INFINITE); // join thread
            CloseHandle(system.reader);
        }
    }
    if (stdout_data != null && stdout_data_size > 0) { stdout_data[stdout_data_size - 1] = 0; }
    if (stderr_data != null && stderr_data_size > 0) { stderr_data[stderr_data_size - 1] = 0; }
    return r;
}
#endif

#ifndef NO_REGEX
#if (!defined(HAVE_REGCOMP) || !defined(HAVE_REGERROR) || !defined(HAVE_REGEXEC) || !defined(HAVE_REGFREE))

#if (defined _WIN32 && ! defined __CYGWIN__)
#include <windows.h>
#endif
/* Return the codeset of the current locale, if this is easily deducible.
   Otherwise, return "".  */
static char *
ctype_codeset(void) {
    static char buf[2 + 10 + 1];
    char const *locale = setlocale(LC_CTYPE, NULL);
    char *codeset = buf;
    size_t codesetlen;
    codeset[0] = '\0';

    if (locale && locale[0]) {
        /* If the locale name contains an encoding after the dot, return it.  */
        char *dot = strchr(locale, '.');

        if (dot) {
            /* Look for the possible @... trailer and remove it, if any.  */
            char *codeset_start = dot + 1;
            char const *modifier = strchr(codeset_start, '@');

            if (!modifier)
                codeset = codeset_start;
            else {
                codesetlen = modifier - codeset_start;
                if (codesetlen < sizeof buf) {
                    codeset = memcpy(buf, codeset_start, codesetlen);
                    codeset[codesetlen] = '\0';
                }
            }
        }
    }

    /* If setlocale is successful, it returns the number of the
     codepage, as a string.  Otherwise, fall back on Windows API
     GetACP, which returns the locale's codepage as a number (although
     this doesn't change according to what the 'setlocale' call specified).
     Either way, prepend "CP" to make it a valid codeset name.  */
    codesetlen = strlen(codeset);
    if (0 < codesetlen && codesetlen < sizeof buf - 2)
        memmove(buf + 2, codeset, codesetlen + 1);
    else
        sprintf(buf + 2, "%u", GetACP());
    codeset = memcpy(buf, "CP", 2);
    return codeset;
}

/* Provide nl_langinfo from scratch, either for native MS-Windows, or
   for old Unix platforms without locales, such as Linux libc5 or
   BeOS.  */

# include <time.h>

char *
nl_langinfo(nl_item item) {
    static char nlbuf[100];
    struct tm tmm = {0};

    switch (item) {
        /* nl_langinfo items of the LC_CTYPE category */
        case CODESET: {
            char *codeset = ctype_codeset();
            if (*codeset)
                return codeset;
        }
            return (char *) "ISO-8859-1";
            /* nl_langinfo items of the LC_NUMERIC category */
        case RADIXCHAR:
            return localeconv()->decimal_point;
        case THOUSEP:
            return localeconv()->thousands_sep;
            /* nl_langinfo items of the LC_TIME category.
               TODO: Really use the locale.  */
        case D_T_FMT:
        case ERA_D_T_FMT:
            return (char *) "%a %b %e %H:%M:%S %Y";
        case D_FMT:
        case ERA_D_FMT:
            return (char *) "%m/%d/%y";
        case T_FMT:
        case ERA_T_FMT:
            return (char *) "%H:%M:%S";
        case T_FMT_AMPM:
            return (char *) "%I:%M:%S %p";
        case AM_STR:
            if (!strftime(nlbuf, sizeof nlbuf, "%p", &tmm))
                return (char *) "AM";
            return nlbuf;
        case PM_STR:
            tmm.tm_hour = 12;
            if (!strftime(nlbuf, sizeof nlbuf, "%p", &tmm))
                return (char *) "PM";
            return nlbuf;
        case DAY_1:
        case DAY_2:
        case DAY_3:
        case DAY_4:
        case DAY_5:
        case DAY_6:
        case DAY_7: {
            static char const days[][sizeof "Wednesday"] = {
                    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
                    "Friday", "Saturday"
            };
            tmm.tm_wday = item - DAY_1;
            if (!strftime(nlbuf, sizeof nlbuf, "%A", &tmm))
                return (char *) days[item - DAY_1];
            return nlbuf;
        }
        case ABDAY_1:
        case ABDAY_2:
        case ABDAY_3:
        case ABDAY_4:
        case ABDAY_5:
        case ABDAY_6:
        case ABDAY_7: {
            static char const abdays[][sizeof "Sun"] = {
                    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
            };
            tmm.tm_wday = item - ABDAY_1;
            if (!strftime(nlbuf, sizeof nlbuf, "%a", &tmm))
                return (char *) abdays[item - ABDAY_1];
            return nlbuf;
        }
            {
                static char const months[][sizeof "September"] = {
                        "January", "February", "March", "April", "May", "June", "July",
                        "September", "October", "November", "December"
                };
                case MON_1:
                case MON_2:
                case MON_3:
                case MON_4:
                case MON_5:
                case MON_6:
                case MON_7:
                case MON_8:
                case MON_9:
                case MON_10:
                case MON_11:
                case MON_12:
                    tmm.tm_mon = item - MON_1;
                if (!strftime(nlbuf, sizeof nlbuf, "%B", &tmm))
                    return (char *) months[item - MON_1];
                return nlbuf;
                case ALTMON_1:
                case ALTMON_2:
                case ALTMON_3:
                case ALTMON_4:
                case ALTMON_5:
                case ALTMON_6:
                case ALTMON_7:
                case ALTMON_8:
                case ALTMON_9:
                case ALTMON_10:
                case ALTMON_11:
                case ALTMON_12:
                    tmm.tm_mon = item - ALTMON_1;
                /* The platforms without nl_langinfo() don't support strftime with %OB.
                   We don't even need to try.  */
                if (!strftime(nlbuf, sizeof nlbuf, "%B", &tmm))
                    return (char *) months[item - ALTMON_1];
                return nlbuf;
            }
        case ABMON_1:
        case ABMON_2:
        case ABMON_3:
        case ABMON_4:
        case ABMON_5:
        case ABMON_6:
        case ABMON_7:
        case ABMON_8:
        case ABMON_9:
        case ABMON_10:
        case ABMON_11:
        case ABMON_12: {
            static char const abmonths[][sizeof "Jan"] = {
                    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                    "Sep", "Oct", "Nov", "Dec"
            };
            tmm.tm_mon = item - ABMON_1;
            if (!strftime(nlbuf, sizeof nlbuf, "%b", &tmm))
                return (char *) abmonths[item - ABMON_1];
            return nlbuf;
        }
        case ERA:
            return (char *) "";
        case ALT_DIGITS:
            return (char *) "\0\0\0\0\0\0\0\0\0\0";
            /* nl_langinfo items of the LC_MONETARY category.  */
        case CRNCYSTR:
            return localeconv()->currency_symbol;
# ifdef INT_CURR_SYMBOL
            case INT_CURR_SYMBOL:
      return localeconv () ->int_curr_symbol;
    case MON_DECIMAL_POINT:
      return localeconv () ->mon_decimal_point;
    case MON_THOUSANDS_SEP:
      return localeconv () ->mon_thousands_sep;
    case MON_GROUPING:
      return localeconv () ->mon_grouping;
    case POSITIVE_SIGN:
      return localeconv () ->positive_sign;
    case NEGATIVE_SIGN:
      return localeconv () ->negative_sign;
    case FRAC_DIGITS:
      return & localeconv () ->frac_digits;
    case INT_FRAC_DIGITS:
      return & localeconv () ->int_frac_digits;
    case P_CS_PRECEDES:
      return & localeconv () ->p_cs_precedes;
    case N_CS_PRECEDES:
      return & localeconv () ->n_cs_precedes;
    case P_SEP_BY_SPACE:
      return & localeconv () ->p_sep_by_space;
    case N_SEP_BY_SPACE:
      return & localeconv () ->n_sep_by_space;
    case P_SIGN_POSN:
      return & localeconv () ->p_sign_posn;
    case N_SIGN_POSN:
      return & localeconv () ->n_sign_posn;
# endif
            /* nl_langinfo items of the LC_MESSAGES category
               TODO: Really use the locale. */
        case YESEXPR:
            return (char *) "^[yY]";
        case NOEXPR:
            return (char *) "^[nN]";
        default:
            return (char *) "";
    }
}

#endif
#endif