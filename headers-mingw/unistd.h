/* Substitute for and wrapper around <unistd.h>.
   Copyright (C) 2003-2018 Free Software Foundation, Inc.

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

#ifndef _GL_UNISTD_H

/* NetBSD 5.0 mis-defines NULL.  Also get size_t.  */
#include <stddef.h>

/* mingw doesn't define the SEEK_* or *_FILENO macros in <unistd.h>.  */
/* MSVC declares 'unlink' in <stdio.h>, not in <unistd.h>.  We must include
   it before we  #define unlink rpl_unlink.  */
/* Cygwin 1.7.1 declares symlinkat in <stdio.h>, not in <unistd.h>.  */
/* But avoid namespace pollution on glibc systems.  */
#if (!(defined SEEK_CUR && defined SEEK_END && defined SEEK_SET) \
     || (defined _WIN32 && ! defined __CYGWIN__) \
     || defined __CYGWIN__) \
    && ! defined __GLIBC__
# include <stdio.h>
#endif

/* Cygwin 1.7.1 declares unlinkat in <fcntl.h>, not in <unistd.h>.  */
/* But avoid namespace pollution on glibc systems.  */
#if defined __CYGWIN__ \
    && ! defined __GLIBC__
# include <fcntl.h>
#endif

/* mingw fails to declare _exit in <unistd.h>.  */
/* mingw, MSVC, BeOS, Haiku declare environ in <stdlib.h>, not in
   <unistd.h>.  */
/* Solaris declares getcwd not only in <unistd.h> but also in <stdlib.h>.  */
/* OSF Tru64 Unix cannot see gnulib rpl_strtod when system <stdlib.h> is
   included here.  */
/* But avoid namespace pollution on glibc systems.  */
#if !defined __GLIBC__ && !defined __osf__
# define __need_system_stdlib_h
# include <stdlib.h>
# undef __need_system_stdlib_h
#endif

/* Native Windows platforms declare chdir, getcwd, rmdir in
   <io.h> and/or <direct.h>, not in <unistd.h>.
   They also declare access(), chmod(), close(), dup(), dup2(), isatty(),
   lseek(), read(), unlink(), write() in <io.h>.  */
#if (defined _WIN32 && ! defined __CYGWIN__)
# include <io.h>     /* mingw32, mingw64 */
# include <direct.h> /* mingw64, MSVC 9 */
#endif

/* AIX and OSF/1 5.1 declare getdomainname in <netdb.h>, not in <unistd.h>.
   NonStop Kernel declares gethostname in <netdb.h>, not in <unistd.h>.  */
/* But avoid namespace pollution on glibc systems.  */
#if ((defined _AIX || defined __osf__) \
     || defined __TANDEM) \
    && !defined __GLIBC__
# include <netdb.h>
#endif

/* MSVC defines off_t in <sys/types.h>.
   May also define off_t to a 64-bit type on native Windows.  */
/* Get off_t.  */
# include <sys/types.h>
/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */

/* The definition of _GL_ARG_NONNULL is copied here.  */

/* The definition of _GL_WARN_ON_USE is copied here.  */


///* Get getopt(), optarg, optind, opterr, optopt.  */
//#if @GNULIB_UNISTD_H_GETOPT@ && !defined _GL_SYSTEM_GETOPT
//# include <getopt-cdefs.h>
//# include <getopt-pfx-core.h>
//#endif

#ifndef _GL_UNISTD_INLINE
# define _GL_UNISTD_INLINE _GL_INLINE
#endif

///* Hide some function declarations from <winsock2.h>.  */
//
//#if @GNULIB_GETHOSTNAME@ && @UNISTD_H_HAVE_WINSOCK2_H@
//# if !defined _@GUARD_PREFIX@_SYS_SOCKET_H
//#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
//#   undef socket
//#   define socket              socket_used_without_including_sys_socket_h
//#   undef connect
//#   define connect             connect_used_without_including_sys_socket_h
//#   undef accept
//#   define accept              accept_used_without_including_sys_socket_h
//#   undef bind
//#   define bind                bind_used_without_including_sys_socket_h
//#   undef getpeername
//#   define getpeername         getpeername_used_without_including_sys_socket_h
//#   undef getsockname
//#   define getsockname         getsockname_used_without_including_sys_socket_h
//#   undef getsockopt
//#   define getsockopt          getsockopt_used_without_including_sys_socket_h
//#   undef listen
//#   define listen              listen_used_without_including_sys_socket_h
//#   undef recv
//#   define recv                recv_used_without_including_sys_socket_h
//#   undef send
//#   define send                send_used_without_including_sys_socket_h
//#   undef recvfrom
//#   define recvfrom            recvfrom_used_without_including_sys_socket_h
//#   undef sendto
//#   define sendto              sendto_used_without_including_sys_socket_h
//#   undef setsockopt
//#   define setsockopt          setsockopt_used_without_including_sys_socket_h
//#   undef shutdown
//#   define shutdown            shutdown_used_without_including_sys_socket_h
//#  else
//    _GL_WARN_ON_USE (socket,
//                     "socket() used without including <sys/socket.h>");
//    _GL_WARN_ON_USE (connect,
//                     "connect() used without including <sys/socket.h>");
//    _GL_WARN_ON_USE (accept,
//                     "accept() used without including <sys/socket.h>");
//    _GL_WARN_ON_USE (bind,
//                     "bind() used without including <sys/socket.h>");
//    _GL_WARN_ON_USE (getpeername,
//                     "getpeername() used without including <sys/socket.h>");
//    _GL_WARN_ON_USE (getsockname,
//                     "getsockname() used without including <sys/socket.h>");
//    _GL_WARN_ON_USE (getsockopt,
//                     "getsockopt() used without including <sys/socket.h>");
//    _GL_WARN_ON_USE (listen,
//                     "listen() used without including <sys/socket.h>");
//    _GL_WARN_ON_USE (recv,
//                     "recv() used without including <sys/socket.h>");
//    _GL_WARN_ON_USE (send,
//                     "send() used without including <sys/socket.h>");
//    _GL_WARN_ON_USE (recvfrom,
//                     "recvfrom() used without including <sys/socket.h>");
//    _GL_WARN_ON_USE (sendto,
//                     "sendto() used without including <sys/socket.h>");
//    _GL_WARN_ON_USE (setsockopt,
//                     "setsockopt() used without including <sys/socket.h>");
//    _GL_WARN_ON_USE (shutdown,
//                     "shutdown() used without including <sys/socket.h>");
//#  endif
//# endif
//# if !defined _@GUARD_PREFIX@_SYS_SELECT_H
//#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
//#   undef select
//#   define select              select_used_without_including_sys_select_h
//#  else
//    _GL_WARN_ON_USE (select,
//                     "select() used without including <sys/select.h>");
//#  endif
//# endif
//#endif


/* OS/2 EMX lacks these macros.  */
#ifndef STDIN_FILENO
# define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
# define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
# define STDERR_FILENO 2
#endif

/* Ensure *_OK macros exist.  */
#ifndef F_OK
# define F_OK 0
# define X_OK 1
# define W_OK 2
# define R_OK 4
#endif

#ifndef MAXSYMLINKS
# ifdef SYMLOOP_MAX
#  define MAXSYMLINKS SYMLOOP_MAX
# else
#  define MAXSYMLINKS 20
# endif
#endif


/* Declare overridden functions.  */

/* Change the owner of FILE to UID (if UID is not -1) and the group of FILE
   to GID (if GID is not -1).  Follow symbolic links.
   Return 0 if successful, otherwise -1 and errno set.
   See the POSIX:2008 specification
   <http://pubs.opengroup.org/onlinepubs/9699919799/functions/chown.html.  */
#  if !HAVE_CHOWN
extern int chown(const char *file, int uid, int gid);
#  endif


/* Copy the file descriptor OLDFD into file descriptor NEWFD.  Do nothing if
   NEWFD = OLDFD, otherwise close NEWFD first if it is open.
   Return newfd if successful, otherwise -1 and errno set.
   See the POSIX:2008 specification
   <http://pubs.opengroup.org/onlinepubs/9699919799/functions/dup2.html>.  */
#if !HAVE_DUP2
extern int dup2(int oldfd, int newfd);
#endif

/* Copy the file descriptor OLDFD into file descriptor NEWFD, with the
   specified flags.
   The flags are a bitmask, possibly including O_CLOEXEC (defined in <fcntl.h>)
   and O_TEXT, O_BINARY (defined in "binary-io.h").
   Close NEWFD first if it is open.
   Return newfd if successful, otherwise -1 and errno set.
   See the Linux man page at
   <https://www.kernel.org/doc/man-pages/online/pages/man2/dup3.2.html>.  */
#if !HAVE_DUP3
extern int dup3(int oldfd, int newfd, int flags);
#endif


//#if @GNULIB_ENVIRON@
//# if defined __CYGWIN__ && !defined __i386__
///* The 'environ' variable is defined in a DLL. Therefore its declaration needs
//   the '__declspec(dllimport)' attribute, but the system's <unistd.h> lacks it.
//   This leads to a link error on 64-bit Cygwin when the option
//   -Wl,--disable-auto-import is in use.  */
//_GL_EXTERN_C __declspec(dllimport) char **environ;
//# endif
//# if !@HAVE_DECL_ENVIRON@
///* Set of environment variables and values.  An array of strings of the form
//   "VARIABLE=VALUE", terminated with a NULL.  */
//#  if defined __APPLE__ && defined __MACH__
//#   include <TargetConditionals.h>
//#   if !TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
//#    define _GL_USE_CRT_EXTERNS
//#   endif
//#  endif
//#  ifdef _GL_USE_CRT_EXTERNS
//#   include <crt_externs.h>
//#   define environ (*_NSGetEnviron ())
//#  else
//#   ifdef __cplusplus
//extern "C" {
//#   endif
//extern char **environ;
//#   ifdef __cplusplus
//}
//#   endif
//#  endif
//# endif
//#elif defined GNULIB_POSIXCHECK
//# if HAVE_RAW_DECL_ENVIRON
//_GL_UNISTD_INLINE char ***
//_GL_WARN_ON_USE_ATTRIBUTE ("environ is unportable - "
//                           "use gnulib module environ for portability")
//rpl_environ (void)
//{
//  return &environ;
//}
//#  undef environ
//#  define environ (*rpl_environ ())
//# endif
//#endif


/* Like access(), except that it uses the effective user id and group id of
   the current process.  */
# if !HAVE_EUIDACCESS
extern int euidaccess(const char *filename, int mode);
#endif


#if !HAVE_FACCESSAT
extern int faccessat(int fd, char const *file, int mode, int flag);
#endif


/* Change the process' current working directory to the directory on which
   the given file descriptor is open.
   Return 0 if successful, otherwise -1 and errno set.
   See the POSIX:2008 specification
   <http://pubs.opengroup.org/onlinepubs/9699919799/functions/fchdir.html>.  */
#if !HAVE_FCHDIR
extern int fchdir(int fd);

/* Gnulib internal hooks needed to maintain the fchdir metadata.  */
extern int _gl_register_fd (int fd, const char *filename);
extern void _gl_unregister_fd (int fd);
extern int _gl_register_dup (int oldfd, int newfd);
extern const char *_gl_directory_name (int fd);
#endif


#if !HAVE_FCHOWNAT
extern int fchownat(int fd, char const *file,
                                  int owner, int group, int flag);
#endif

/* Synchronize changes to a file.
   Return 0 if successful, otherwise -1 and errno set.
   See POSIX:2008 specification
   <http://pubs.opengroup.org/onlinepubs/9699919799/functions/fdatasync.html>.  */
#if !HAVE_FDATASYNC
extern int fdatasync(int fd);
#endif

/* Synchronize changes, including metadata, to a file.
   Return 0 if successful, otherwise -1 and errno set.
   See POSIX:2008 specification
   <http://pubs.opengroup.org/onlinepubs/9699919799/functions/fsync.html>.  */
#if !HAVE_FSYNC
extern int fsync(int fd);
#endif

/* Change the size of the file to which FD is opened to become equal to LENGTH.
   Return 0 if successful, otherwise -1 and errno set.
   See the POSIX:2008 specification
   <http://pubs.opengroup.org/onlinepubs/9699919799/functions/ftruncate.html>.  */
#if !HAVE_FTRUNCATE
extern int ftruncate(int fd, off_t length);
#endif

/* Return the NIS domain name of the machine.
   WARNING! The NIS domain name is unrelated to the fully qualified host name
            of the machine.  It is also unrelated to email addresses.
   WARNING! The NIS domain name is usually the empty string or "(none)" when
            not using NIS.

   Put up to LEN bytes of the NIS domain name into NAME.
   Null terminate it if the name is shorter than LEN.
   If the NIS domain name is longer than LEN, set errno = EINVAL and return -1.
   Return 0 if successful, otherwise set errno and return -1.  */
#if !HAVE_GETDOMAINNAME
extern int getdomainname(char *name, size_t len);
#endif

/* Return the maximum number of file descriptors in the current process.
   In POSIX, this is same as sysconf (_SC_OPEN_MAX).  */
#if !HAVE_GETDTABLESIZE
extern int getdtablesize(void);
#endif

/* Return the supplemental groups that the current process belongs to.
   It is unspecified whether the effective group id is in the list.
   If N is 0, return the group count; otherwise, N describes how many
   entries are available in GROUPS.  Return -1 and set errno if N is
   not 0 and not large enough.  Fails with ENOSYS on some systems.  */
#if !HAVE_GETGROUPS
extern int getgroups(int n, int *groups);
#endif

/* Return the standard host name of the machine.
   WARNING! The host name may or may not be fully qualified.

   Put up to LEN bytes of the host name into NAME.
   Null terminate it if the name is shorter than LEN.
   If the host name is longer than LEN, set errno = EINVAL and return -1.
   Return 0 if successful, otherwise set errno and return -1.  */
#if !HAVE_GETHOSTNAME && !(defined _WIN32 && ! defined __CYGWIN__)
extern int gethostname(char *name, size_t len);
#endif

/* Returns the user's login name, or NULL if it cannot be found.  Upon error,
   returns NULL with errno set.

   See <http://www.opengroup.org/susv3xsh/getlogin.html>.

   Most programs don't need to use this function, because the information is
   available through environment variables:
     ${LOGNAME-$USER}        on Unix platforms,
     $USERNAME               on native Windows platforms.
 */
# if !HAVE_GETLOGIN
extern char * getlogin(void);
#endif

/* Copies the user's login name to NAME.
   The array pointed to by NAME has room for SIZE bytes.

   Returns 0 if successful.  Upon error, an error number is returned, or -1 in
   the case that the login name cannot be found but no specific error is
   provided (this case is hopefully rare but is left open by the POSIX spec).

   See <http://www.opengroup.org/susv3xsh/getlogin.html>.

   Most programs don't need to use this function, because the information is
   available through environment variables:
     ${LOGNAME-$USER}        on Unix platforms,
     $USERNAME               on native Windows platforms.
 */
#if !HAVE_GETLOGIN_R
extern int getlogin_r(char *name, size_t size);
#endif


/* Function getpass() from module 'getpass':
     Read a password from /dev/tty or stdin.
   Function getpass() from module 'getpass-gnu':
     Read a password of arbitrary length from /dev/tty or stdin.  */
#if !HAVE_GETPASS
extern char * getpass(const char *prompt);
#endif

/* Return the next valid login shell on the system, or NULL when the end of
   the list has been reached.  */
#if !HAVE_GETUSERSHELL
extern char * getusershell(void);
#endif

/* Rewind to pointer that is advanced at each getusershell() call.  */
#if !HAVE_SETUSERSHELL
extern void setusershell(void);
#endif

/* Free the pointer that is advanced at each getusershell() call and
   associated resources.  */
#if !HAVE_ENDUSERSHELL
extern void endusershell(void);
#endif

/* Determine whether group id is in calling user's group list.  */
#if !HAVE_GROUP_MEMBER
extern int group_member(int gid);
#endif

/* Change the owner of FILE to UID (if UID is not -1) and the group of FILE
   to GID (if GID is not -1).  Do not follow symbolic links.
   Return 0 if successful, otherwise -1 and errno set.
   See the POSIX:2008 specification
   <http://pubs.opengroup.org/onlinepubs/9699919799/functions/lchown.html>.  */
#if !HAVE_LCHOWN
extern int lchown(char const *file, int owner, int group);
#endif

/* Create a new hard link for an existing file.
   Return 0 if successful, otherwise -1 and errno set.
   See POSIX:2008 specification
   <http://pubs.opengroup.org/onlinepubs/9699919799/functions/link.html>.  */
#if !HAVE_LINK
extern int link(const char *path1, const char *path2);
#endif

/* Create a new hard link for an existing file, relative to two
   directories.  FLAG controls whether symlinks are followed.
   Return 0 if successful, otherwise -1 and errno set.  */
#if !HAVE_LINKAT
extern int linkat(int fd1, const char *path1, int fd2, const char *path2,
                   int flag);
#endif

/* Create a pipe, defaulting to O_BINARY mode.
   Store the read-end as fd[0] and the write-end as fd[1].
   Return 0 upon success, or -1 with errno set upon failure.  */
#if !HAVE_PIPE
extern int pipe(int fd[2]);
#endif

/* Create a pipe, applying the given flags when opening the read-end of the
   pipe and the write-end of the pipe.
   The flags are a bitmask, possibly including O_CLOEXEC (defined in <fcntl.h>)
   and O_TEXT, O_BINARY (defined in "binary-io.h").
   Store the read-end as fd[0] and the write-end as fd[1].
   Return 0 upon success, or -1 with errno set upon failure.
   See also the Linux man page at
   <https://www.kernel.org/doc/man-pages/online/pages/man2/pipe2.2.html>.  */
#if !HAVE_PIPE2
extern int pipe2(int fd[2], int flags);
#endif

/* Read at most BUFSIZE bytes from FD into BUF, starting at OFFSET.
   Return the number of bytes placed into BUF if successful, otherwise
   set errno and return -1.  0 indicates EOF.
   See the POSIX:2008 specification
   <http://pubs.opengroup.org/onlinepubs/9699919799/functions/pread.html>.  */
#if !HAVE_PREAD
extern ssize_t pread(int fd, void *buf, size_t bufsize, off_t offset);
#endif

/* Write at most BUFSIZE bytes from BUF into FD, starting at OFFSET.
   Return the number of bytes written if successful, otherwise
   set errno and return -1.  0 indicates nothing written.  See the
   POSIX:2008 specification
   <http://pubs.opengroup.org/onlinepubs/9699919799/functions/pwrite.html>.  */
#if !HAVE_PWRITE
extern ssize_t pwrite(int fd, const void *buf, size_t bufsize, off_t offset);
#endif

/* Read the contents of the symbolic link FILE and place the first BUFSIZE
   bytes of it into BUF.  Return the number of bytes placed into BUF if
   successful, otherwise -1 and errno set.
   See the POSIX:2008 specification
   <http://pubs.opengroup.org/onlinepubs/9699919799/functions/readlink.html>.  */
#if !HAVE_READLINK
extern ssize_t readlink(const char *file, char *buf, size_t bufsize);
#endif

#if !HAVE_READLINKAT
extern ssize_t readlinkat(int fd, char const *file, char *buf, size_t len);
#endif

/* Set the host name of the machine.
   The host name may or may not be fully qualified.

   Put LEN bytes of NAME into the host name.
   Return 0 if successful, otherwise, set errno and return -1.

   Platforms with no ability to set the hostname return -1 and set
   errno = ENOSYS.  */
#if !HAVE_SETHOSTNAME
extern int sethostname(const char *name, size_t len);
#endif

/* Pause the execution of the current thread for N seconds.
   Returns the number of seconds left to sleep.
   See the POSIX:2008 specification
   <http://pubs.opengroup.org/onlinepubs/9699919799/functions/sleep.html>.  */
#if !HAVE_SLEEP
extern unsigned int sleep(unsigned int n);
#endif

#if !HAVE_SYMLINK
extern int symlink(char const *contents, char const *file);
#endif

#if !HAVE_SYMLINKAT
extern int symlinkat(char const *contents, int fd, char const *file);
#endif

/* Change the size of the file designated by FILENAME to become equal to LENGTH.
   Return 0 if successful, otherwise -1 and errno set.
   See the POSIX:2008 specification
   <http://pubs.opengroup.org/onlinepubs/9699919799/functions/truncate.html>.  */
#if !HAVE_TRUNCATE
extern int truncate(const char *filename, off_t length);
#endif

/* Store at most BUFLEN characters of the pathname of the terminal FD is
   open on in BUF.  Return 0 on success, otherwise an error number.  */
#if !HAVE_TTYNAME_R
extern int ttyname_r(int fd, char *buf, size_t buflen);
#endif

#if !HAVE_UNLINKAT
extern int unlinkat(int fd, char const *file, int flag);
#endif

/* Pause the execution of the current thread for N microseconds.
   Returns 0 on completion, or -1 on range error.
   See the POSIX:2001 specification
   <http://www.opengroup.org/susv3xsh/usleep.html>.  */
#if !HAVE_USLEEP
extern int usleep(useconds_t n);
#endif

#endif /* _@GUARD_PREFIX@_UNISTD_H */

