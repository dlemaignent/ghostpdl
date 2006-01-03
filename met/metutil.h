/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/* $Id:*/

/* metutil.h */

/* the obligatory metro utilities */

#ifndef metutil_INCLUDED
#   define metutil_INCLUDED

#include "stdpre.h" /* for bool */

/* utility for splitting up strings - destroys argument and client
   must determine supply a large enough argument list to support all
   of the parameters.  The delimeter function should return true if
   the character is a delimeter.  The C language library iswhite()
   could be used as a delimeter function */

void met_split(char *b, char **args, bool (*delimfunc(char c)));

/* strcmp(lhs, attr) || *field = rhs saves keystrokes */
int met_cmp_and_set(char **field, const char *lhs, const char *rhs, const char *attr_name);

/* nb should use a gs type but this is expedient for now.  Convert an
   rgb hex string to an rgb triple */
typedef struct rgb_s {
    int r;
    int g;
    int b;
} rgb_t;

rgb_t met_hex2rgb(char *hexstring);

#endif /* metutil_INCLUDED */
