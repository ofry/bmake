/* Substitute for and wrapper around <sys/ioctl.h>.
   Copyright (C) 2008-2018 Free Software Foundation, Inc.

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

#ifndef _GL_SYS_IOCTL_H

#define _GL_SYS_IOCTL_H

/* AIX 5.1 and Solaris 10 declare ioctl() in <unistd.h> and in <stropts.h>,
   but not in <sys/ioctl.h>.
   Haiku declares ioctl() in <unistd.h>, but not in <sys/ioctl.h>.
   But avoid namespace pollution on glibc systems.  */
#ifndef __GLIBC__
# include <unistd.h>
#endif

/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */

/* The definition of _GL_WARN_ON_USE is copied here.  */


/* Declare overridden functions.  */

#endif /* _@GUARD_PREFIX@_SYS_IOCTL_H */
