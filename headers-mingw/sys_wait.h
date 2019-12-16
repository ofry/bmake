/* A POSIX-like <sys/wait.h>.
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

#ifndef _GL_SYS_WAIT_H
#define _GL_SYS_WAIT_H

/* Get pid_t.  */
#include <sys/types.h>


/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */

/* The definition of _GL_WARN_ON_USE is copied here.  */

/* Native Windows API.  */

#include <signal.h> /* for SIGTERM */

/* The following macros apply to an argument x, that is a status of a process,
   as returned by waitpid() or, equivalently, _cwait() or GetExitCodeProcess().
   This value is simply an 'int', not composed of bit fields.  */

/* When an unhandled fatal signal terminates a process, the exit code is 3.  */
#define WIFSIGNALED(x) ((x) == 3)
#define WIFEXITED(x) ((x) != 3)
#define WIFSTOPPED(x) 0

/* The signal that terminated a process is not known posthum.  */
#define WTERMSIG(x) SIGTERM

#define WEXITSTATUS(x) (x)

/* There are no stopping signals.  */
#define WSTOPSIG(x) 0

/* There are no core dumps.  */
#define WCOREDUMP(x) 0




/* Declarations of functions.  */

#if !HAVE_WAITPID
/* dummy constants */
#define WNOHANG (-1)
#define WUNTRACED (-1)
extern pid_t waitpid(pid_t pid, int *statusp, int options);
#endif


#endif /* _@GUARD_PREFIX@_SYS_WAIT_H */
