/*
 * cldump - Dumps Clarion databases to text, SQL and CSV formats
 *
 * Copyright (C) 2004-2006 Julien BLACHE <jb@jblache.org>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; version 2 of the License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: cl_dump_meta_csv.c 68 2010-11-27 10:20:15Z julien $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <byteswap.h>

#include "cldump.h"

static void
clarion_dump_field_desc_csv(ClarionHandle *cl)
{
  int i;
  uint8_t buf[17];
  uint8_t *pbuf;
  ClarionFieldDesc *clfd;

  clfd = cl->clm.clfd;

  for (i = 0; i < cl->clm.clh->numflds; i++)
    {
      /*
       * A field with type CL_FIELD_GROUP is a pseudo-field
       * used to indicate that the next clfd[i].length fields
       * are grouped together.
       */
      if (clfd[i].fldtype == CL_FIELD_GROUP)
	{
	  continue;
	}

      if (i > 0)
	fprintf(stdout, "%c", cl->fsep);

      memcpy(buf, clfd[i].fldname, 17);
      clarion_trim(buf, 16);
      pbuf = (uint8_t *)strchr((char *)buf, ':');

      fprintf(stdout, "%s", (pbuf != NULL) ? ++pbuf : buf);
    }

  if ((cl->clm.clh->sfatr & CL_MEMO_FILE_EXISTS) && (!(cl->opts & CL_OPT_NO_MEMO)))
    {
      fprintf(stdout, "%cMEMO", cl->fsep);
    }

  fprintf(stdout, "\n");

  fflush(stdout);
}

void
clarion_dump_schema_csv (ClarionHandle *cl)
{
  clarion_dump_field_desc_csv(cl);
}
