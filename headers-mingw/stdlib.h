/* A GNU-like <stdlib.h>.

   Copyright (C) 1995, 2001-2004, 2006-2018 Free Software Foundation, Inc.

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

/* Normal invocation convention.  */

#ifndef _GL_STDLIB_H

#define _GL_STDLIB_H

/* Native Windows platforms declare mktemp() in <io.h>.  */
#if (defined _WIN32 && ! defined __CYGWIN__)
# include <io.h>
#endif

/* OSF/1 5.1 declares 'struct random_data' in <random.h>, which is included
   from <stdlib.h> if _REENTRANT is defined.  Include it whenever we need
   'struct random_data'.  */
#if HAVE_RANDOM_H
# include <random.h>
#endif

#if !HAVE_RANDOM_R
# include <stdint.h>
#endif

/* Define 'struct random_data'.
   But allow multiple gnulib generated <stdlib.h> replacements to coexist.  */
#if !GNULIB_defined_struct_random_data
typedef struct
{
  int *fptr;                /* Front pointer.  */
  int *rptr;                /* Rear pointer.  */
  int *state;               /* Array of state values.  */
  int rand_type;                /* Type of random number generator.  */
  int rand_deg;                 /* Degree of random number generator.  */
  int rand_sep;                 /* Distance between front and rear.  */
  int *end_ptr;             /* Pointer behind state table.  */
} random_data;
# define GNULIB_defined_struct_random_data 1
#endif

/* The __attribute__ feature is available in gcc versions 2.5 and later.
   The attribute __pure__ was added in gcc 2.96.  */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
# define _GL_ATTRIBUTE_PURE __attribute__ ((__pure__))
#else
# define _GL_ATTRIBUTE_PURE /* empty */
#endif

/* The definition of _Noreturn is copied here.  */

/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */

/* The definition of _GL_ARG_NONNULL is copied here.  */

/* The definition of _GL_WARN_ON_USE is copied here.  */


/* Some systems do not define EXIT_*, despite otherwise supporting C89.  */
#ifndef EXIT_SUCCESS
# define EXIT_SUCCESS 0
#endif
/* Tandem/NSK and other platforms that define EXIT_FAILURE as -1 interfere
   with proper operation of xargs.  */
#ifndef EXIT_FAILURE
# define EXIT_FAILURE 1
#elif EXIT_FAILURE != 1
# undef EXIT_FAILURE
# define EXIT_FAILURE 1
#endif


/* Terminate the current process with the given return code, without running
   the 'atexit' handlers.  */
#if !HAVE__EXIT
extern void _Exit(int status);
#endif

/* Parse a signed decimal integer.
   Returns the value of the integer.  Errors are not detected.  */
#if !HAVE_ATOLL
extern long long atoll(const char *string);
#endif

#if !HAVE_CANONICALIZE_FILE_NAME
extern char * canonicalize_file_name(const char *name);
#endif

/* Store max(NELEM,3) load average numbers in LOADAVG[].
   The three numbers are the load average of the last 1 minute, the last 5
   minutes, and the last 15 minutes, respectively.
   LOADAVG is an array of NELEM numbers.  */
#if !HAVE_GETLOADAVG
extern int getloadavg(double loadavg[], int nelem);
#endif


/* Assuming *OPTIONP is a comma separated list of elements of the form
   "token" or "token=value", getsubopt parses the first of these elements.
   If the first element refers to a "token" that is member of the given
   NULL-terminated array of tokens:
     - It replaces the comma with a NUL byte, updates *OPTIONP to point past
       the first option and the comma, sets *VALUEP to the value of the
       element (or NULL if it doesn't contain an "=" sign),
     - It returns the index of the "token" in the given array of tokens.
   Otherwise it returns -1, and *OPTIONP and *VALUEP are undefined.
   For more details see the POSIX:2001 specification.
   http://www.opengroup.org/susv3xsh/getsubopt.html */
#if !HAVE_GETSUBOPT
extern int getsubopt(char **optionp, char *const *tokens, char **valuep);
#endif


/* Change the ownership and access permission of the slave side of the
   pseudo-terminal whose master side is specified by FD.  */
#if !HAVE_GRANTPT
extern int grantpt(int fd);
#endif


/* Create a unique temporary directory from TEMPLATE.
   The last six characters of TEMPLATE must be "XXXXXX";
   they are replaced with a string that makes the directory name unique.
   Returns TEMPLATE, or a null pointer if it cannot get a unique name.
   The directory is created mode 700.  */
#if !HAVE_MKDTEMP
extern char * mkdtemp(char *template);
#endif

/* Create a unique temporary file from TEMPLATE.
   The last six characters of TEMPLATE must be "XXXXXX";
   they are replaced with a string that makes the file name unique.
   The flags are a bitmask, possibly including O_CLOEXEC (defined in <fcntl.h>)
   and O_TEXT, O_BINARY (defined in "binary-io.h").
   The file is then created, with the specified flags, ensuring it didn't exist
   before.
   The file is created read-write (mask at least 0600 & ~umask), but it may be
   world-readable and world-writable (mask 0666 & ~umask), depending on the
   implementation.
   Returns the open file descriptor if successful, otherwise -1 and errno
   set.  */
#if !HAVE_MKOSTEMP
extern int mkostemp(char *template, int flags);
#endif

/* Create a unique temporary file from TEMPLATE.
   The last six characters of TEMPLATE before a suffix of length
   SUFFIXLEN must be "XXXXXX";
   they are replaced with a string that makes the file name unique.
   The flags are a bitmask, possibly including O_CLOEXEC (defined in <fcntl.h>)
   and O_TEXT, O_BINARY (defined in "binary-io.h").
   The file is then created, with the specified flags, ensuring it didn't exist
   before.
   The file is created read-write (mask at least 0600 & ~umask), but it may be
   world-readable and world-writable (mask 0666 & ~umask), depending on the
   implementation.
   Returns the open file descriptor if successful, otherwise -1 and errno
   set.  */
#if !HAVE_MKOSTEMPS
extern int mkostemps(char * template, int suffixlen, int flags);
#endif


/* Create a unique temporary file from TEMPLATE.
   The last six characters of TEMPLATE must be "XXXXXX";
   they are replaced with a string that makes the file name unique.
   The file is then created, ensuring it didn't exist before.
   The file is created read-write (mask at least 0600 & ~umask), but it may be
   world-readable and world-writable (mask 0666 & ~umask), depending on the
   implementation.
   Returns the open file descriptor if successful, otherwise -1 and errno
   set.  */
#if !HAVE_MKSTEMP
extern int mkstemp(char * template);
#endif


/* Create a unique temporary file from TEMPLATE.
   The last six characters of TEMPLATE prior to a suffix of length
   SUFFIXLEN must be "XXXXXX";
   they are replaced with a string that makes the file name unique.
   The file is then created, ensuring it didn't exist before.
   The file is created read-write (mask at least 0600 & ~umask), but it may be
   world-readable and world-writable (mask 0666 & ~umask), depending on the
   implementation.
   Returns the open file descriptor if successful, otherwise -1 and errno
   set.  */
#if !HAVE_MKSTEMPS
extern int mkstemps(char * template, int suffixlen);
#endif


/* Return an FD open to the master side of a pseudo-terminal.  Flags should
   include O_RDWR, and may also include O_NOCTTY.  */
# if !HAVE_POSIX_OPENPT
extern int posix_openpt(int flags);
#endif

/* Return the pathname of the pseudo-terminal slave associated with
   the master FD is open on, or NULL on errors.  */
#if !HAVE_PTSNAME
extern char * ptsname(int fd);
#endif


/* Set the pathname of the pseudo-terminal slave associated with
   the master FD is open on and return 0, or set errno and return
   non-zero on errors.  */
#if !HAVE_PTSNAME_R
extern int ptsname_r(int fd, char *buf, size_t len);
#endif

/* Sort an array of NMEMB elements, starting at address BASE, each element
   occupying SIZE bytes, in ascending order according to the comparison
   function COMPARE.  */
#if !HAVE_QSORT_R
extern void qsort_r(void *base, size_t nmemb, size_t size,
                                  int (*compare) (void const *, void const *,
                                                  void *),
                                  void *arg);
#endif

#ifndef RAND_MAX
# define RAND_MAX 2147483647
#endif

#if !HAVE_RANDOM
extern long random(void);
#endif

#if !HAVE_SRANDOM
extern void srandom(unsigned int seed);
#endif


#if !HAVE_INITSTATE
extern char * initstate(unsigned int seed, char *buf, size_t buf_size);
#endif

#if !HAVE_SETSTATE
extern char * setstate(char *arg_state);
#endif


#if !HAVE_RANDOM_R
extern int random_r(random_data *buf, int *result);
#endif

#  if !HAVE_SRANDOM_R
extern int srandom_r(unsigned int seed, random_data *rand_state);
#endif

#if !HAVE_INITSTATE_R
extern int initstate_r(unsigned int seed, char *buf, size_t buf_size,
    random_data *rand_state);
#endif

#if !HAVE_SETSTATE_R
extern int setstate_r(char *arg_state, random_data *rand_state);
#endif

#if !HAVE_REALLOCARRAY
extern void * reallocarray(void *ptr, size_t nmemb, size_t size);
#endif

#if !HAVE_REALPATH
extern char * realpath(const char *name, char *resolved);
#endif

/* Test a user response to a question.
   Return 1 if it is affirmative, 0 if it is negative, or -1 if not clear.  */
#if !HAVE_RPMATCH
extern int rpmatch(const char *response);
#endif

/* Look up NAME in the environment, returning 0 in insecure situations.  */
#if !HAVE_SECURE_GETENV
extern char * secure_getenv(char const *name);
#endif


/* Set NAME to VALUE in the environment.
   If REPLACE is nonzero, overwrite an existing value.  */
#if !HAVE_SETENV
extern int setenv(const char *name, const char *value, int replace);
#endif

/* Parse a double from STRING, updating ENDP if appropriate.  */
#if !HAVE_STRTOD
extern double strtod(const char *str, char **endp);
#endif

/* Parse a signed integer whose textual representation starts at STRING.
   The integer is expected to be in base BASE (2 <= BASE <= 36); if BASE == 0,
   it may be decimal or octal (with prefix "0") or hexadecimal (with prefix
   "0x").
   If ENDPTR is not NULL, the address of the first byte after the integer is
   stored in *ENDPTR.
   Upon overflow, the return value is LLONG_MAX or LLONG_MIN, and errno is set
   to ERANGE.  */
#if !HAVE_STRTOLL
extern long long strtoll(const char *string, char **endptr, int base);
#endif

/* Parse an unsigned integer whose textual representation starts at STRING.
   The integer is expected to be in base BASE (2 <= BASE <= 36); if BASE == 0,
   it may be decimal or octal (with prefix "0") or hexadecimal (with prefix
   "0x").
   If ENDPTR is not NULL, the address of the first byte after the integer is
   stored in *ENDPTR.
   Upon overflow, the return value is ULLONG_MAX, and errno is set to
   ERANGE.  */
#if !HAVE_STRTOULL
extern unsigned long long strtoull(const char *string, char **endptr, int base);
#endif


/* Unlock the slave side of the pseudo-terminal whose master side is specified
   by FD, so that it can be opened.  */
#if !HAVE_UNLOCKPT
extern int unlockpt(int fd);
#endif

/* Remove the variable NAME from the environment.  */
#if !HAVE_UNSETENV
extern int unsetenv(const char *name);
#endif

#endif /* _@GUARD_PREFIX@_STDLIB_H */

