/* A GNU-like <signal.h>.

   Copyright (C) 2006-2018 Free Software Foundation, Inc.

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

#ifndef _GL_SIGNAL_H
#define _GL_SIGNAL_H

/* Get pid_t.  */
#include <sys/types.h>

/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */

/* The definition of _GL_ARG_NONNULL is copied here.  */

/* The definition of _GL_WARN_ON_USE is copied here.  */

/* A set or mask of signals.  */
#if !GNULIB_defined_sigset_t
typedef unsigned int sigset_t;
# define GNULIB_defined_sigset_t 1
#endif


/* Define sighandler_t, the type of signal handlers.  A GNU extension.  */
#ifdef __cplusplus
extern "C" {
#endif
#if !GNULIB_defined_sighandler_t
typedef void (*sighandler_t) (int);
# define GNULIB_defined_sighandler_t 1
#endif
#ifdef __cplusplus
}
#endif



#ifndef SIGPIPE
/* Define SIGPIPE to a value that does not overlap with other signals.  */
#  define SIGPIPE 13
#  define GNULIB_defined_SIGPIPE 1
/* To actually use SIGPIPE, you also need the gnulib modules 'sigprocmask',
   'write', 'stdio'.  */
#endif



/* Maximum signal number + 1.  */
#ifndef NSIG
# if defined __TANDEM
#  define NSIG 32
# endif
#endif


#if !(HAVE_PTHREAD_SIGMASK || defined pthread_sigmask)
extern int pthread_sigmask(int how, const sigset_t *new_mask, sigset_t *old_mask);
#endif


#if !HAVE_RAISE
extern int raise(int sig);
#endif


#ifndef GNULIB_defined_signal_blocking
# define GNULIB_defined_signal_blocking 1
#endif

/* Maximum signal number + 1.  */
#ifndef NSIG
# define NSIG 32
#endif

/* This code supports only 32 signals.  */
#if !GNULIB_defined_verify_NSIG_constraint
typedef int verify_NSIG_constraint[NSIG <= 32 ? 1 : -1];
# define GNULIB_defined_verify_NSIG_constraint 1
#endif


/* When also using extern inline, suppress the use of static inline in
   standard headers of problematic Apple configurations, as Libc at
   least through Libc-825.26 (2013-04-09) mishandles it; see, e.g.,
   <https://lists.gnu.org/r/bug-gnulib/2012-12/msg00023.html>.
   Perhaps Apple will fix this some day.  */
#if (defined _GL_EXTERN_INLINE_IN_USE && defined __APPLE__ \
     && (defined __i386__ || defined __x86_64__))
# undef sigaddset
# undef sigdelset
# undef sigemptyset
# undef sigfillset
# undef sigismember
#endif

/* Test whether a given signal is contained in a signal set.  */
#if !HAVE_SIGISMEMBER
extern int sigismember(const sigset_t *set, int sig);
# endif

/* Initialize a signal set to the empty set.  */
#if !HAVE_SIGEMPTYSET
extern int sigemptyset(sigset_t *set);
#endif


/* Add a signal to a signal set.  */
#if !HAVE_SIGADDSET
extern int sigaddset(sigset_t *set, int sig);
#endif


/* Remove a signal from a signal set.  */
#if !HAVE_SIGDELSET
extern int sigdelset(sigset_t *set, int sig);
# endif

/* Fill a signal set with all possible signals.  */
#if !HAVE_SIGFILLSET
extern int sigfillset(sigset_t *set);
#endif


/* Return the set of those blocked signals that are pending.  */
#if !HAVE_SIGPENDING
extern int sigpending(sigset_t *set);
#endif

/* If OLD_SET is not NULL, put the current set of blocked signals in *OLD_SET.
   Then, if SET is not NULL, affect the current set of blocked signals by
   combining it with *SET as indicated in OPERATION.
   In this implementation, you are not allowed to change a signal handler
   while the signal is blocked.  */
#ifndef SIG_BLOCK
# define SIG_BLOCK   0  /* blocked_set = blocked_set | *set; */
#endif
#ifndef SIG_SETMASK
# define SIG_SETMASK 1  /* blocked_set = *set; */
#endif
#ifndef SIG_UNBLOCK
# define SIG_UNBLOCK 2  /* blocked_set = blocked_set & ~*set; */
#endif
#if !HAVE_SIGPROCMASK
extern int sigprocmask(int operation, const sigset_t *set, sigset_t *old_set);
# endif

/* Install the handler FUNC for signal SIG, and return the previous
   handler.  */
#ifdef __cplusplus
extern "C" {
#endif
#if !GNULIB_defined_function_taking_int_returning_void_t
typedef void (*_gl_function_taking_int_returning_void_t) (int);
# define GNULIB_defined_function_taking_int_returning_void_t 1
#endif
#ifdef __cplusplus
}
#endif
#if !HAVE_SIGNAL
extern _gl_function_taking_int_returning_void_t signal(int sig, _gl_function_taking_int_returning_void_t func);
#endif


extern int _gl_raise_SIGPIPE (void);

#if !HAVE_SIGACTION
# if !GNULIB_defined_siginfo_types

/* Present to allow compilation, but unsupported by gnulib.  */
union sigval
{
  int sival_int;
  void *sival_ptr;
};

/* Present to allow compilation, but unsupported by gnulib.  */
struct siginfo_t
{
  int si_signo;
  int si_code;
  int si_errno;
  pid_t si_pid;
  int si_uid;
  void *si_addr;
  int si_status;
  long si_band;
  union sigval si_value;
};
typedef struct siginfo_t siginfo_t;

#  define GNULIB_defined_siginfo_types 1
# endif


/* We assume that platforms which lack the sigaction() function also lack
   the 'struct sigaction' type, and vice versa.  */

# if !GNULIB_defined_struct_sigaction

struct sigaction
{
  union
  {
    void (*_sa_handler) (int);
    /* Present to allow compilation, but unsupported by gnulib.  POSIX
       says that implementations may, but not must, make sa_sigaction
       overlap with sa_handler, but we know of no implementation where
       they do not overlap.  */
    void (*_sa_sigaction) (int, siginfo_t *, void *);
  } _sa_func;
  sigset_t sa_mask;
  /* Not all POSIX flags are supported.  */
  int sa_flags;
};
#  define sa_handler _sa_func._sa_handler
#  define sa_sigaction _sa_func._sa_sigaction
/* Unsupported flags are not present.  */
#  define SA_RESETHAND 1
#  define SA_NODEFER 2
#  define SA_RESTART 4

#  define GNULIB_defined_struct_sigaction 1
# endif

extern int sigaction(int, const struct sigaction *restrict,
                                   struct sigaction *restrict);
#endif

/* Some systems don't have SA_NODEFER.  */
#ifndef SA_NODEFER
# define SA_NODEFER 0
#endif

/* Out-of-range substitutes make a good fallback for uncatchable
   signals.  */
#ifndef SIGKILL
# define SIGKILL (-1)
#endif
#ifndef SIGSTOP
# define SIGSTOP (-1)
#endif

#if !defined(HAVE_KILL)
extern int kill(pid_t pid, int sig);
#endif

#endif /* _@GUARD_PREFIX@_SIGNAL_H */
