/* Header for poll(2) emulation
   Contributed by Paolo Bonzini.

   Copyright 2001-2003, 2007, 2009-2018 Free Software Foundation, Inc.

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

#ifndef _GL_POLL_H

#define _GL_POLL_H


/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */

/* The definition of _GL_WARN_ON_USE is copied here.  */


#if !HAVE_POLL_H

/* fake a poll(2) environment */
# define POLLIN      0x0001      /* any readable data available   */
# define POLLPRI     0x0002      /* OOB/Urgent readable data      */
# define POLLOUT     0x0004      /* file descriptor is writable   */
# define POLLERR     0x0008      /* some poll error occurred      */
# define POLLHUP     0x0010      /* file descriptor was "hung up" */
# define POLLNVAL    0x0020      /* requested events "invalid"    */
# define POLLRDNORM  0x0040
# define POLLRDBAND  0x0080
# define POLLWRNORM  0x0100
# define POLLWRBAND  0x0200

# if !GNULIB_defined_poll_types
#  if !(defined _WIN32 && ! defined __CYGWIN__)
struct pollfd
{
  int fd;                       /* which file descriptor to poll */
  short events;                 /* events we are interested in   */
  short revents;                /* events found on return        */
};

#  else
#include <winsock2.h>
#  endif

typedef unsigned long nfds_t;

#  define GNULIB_defined_poll_types 1
# endif

/* Define INFTIM only if doing so conforms to POSIX.  */
# if !defined (_POSIX_C_SOURCE) && !defined (_XOPEN_SOURCE)
#  define INFTIM (-1)
# endif

#endif


#  if !HAVE_POLL
extern int poll (struct pollfd *pfd, nfds_t nfd, int timeout);
#endif


#endif /* _@GUARD_PREFIX@_POLL_H */
