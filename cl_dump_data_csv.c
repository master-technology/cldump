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
 * $Id: cl_dump_data_csv.c 65 2010-11-27 10:20:08Z julien $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <byteswap.h>

#include "cldump.h"

void
clarion_dump_data_csv (ClarionHandle *cl)
{
  int nrecs, nflds;
  FILE *fp = cl->data;
  ClarionHeader *clh = cl->clm.clh;
  ClarionFieldDesc *clfd = cl->clm.clfd;
  ClarionRecordHeader clrh;
  uint8_t *buf;

  buf = (uint8_t *) malloc(clh->reclen * sizeof(uint8_t));

  for (nrecs = 0; nrecs < clh->numrecs; nrecs++)
    {      
      fread(&clrh.rhd, 1, 1, fp);
      fread(&clrh.rptr, 4, 1, fp);

      if ((clrh.rhd & CL_RECORD_DELETED) && (cl->opts & CL_OPT_DUMP_ACTIVE))
	{
	  fseek(cl->data, (clh->reclen - 5), SEEK_CUR);
	  continue;
	}

      for (nflds = 0; nflds < clh->numflds; nflds++)
	{
	  /*
	   * A field with type CL_FIELD_GROUP is a pseudo-field
	   * used to indicate that the next clfd[i].length fields
	   * are grouped together.
	   */
	  if (clfd[nflds].fldtype == CL_FIELD_GROUP)
	    {
	      continue;
	    }

	  if (nflds > 0)
	    fprintf(stdout, "%c", cl->fsep);

	  switch (clfd[nflds].fldtype)
	    {
	      case CL_FIELD_LONG:
		clarion_dump_field_long(buf, &clfd[nflds], cl->data, NULL);
		break;
	      case CL_FIELD_REAL:
		clarion_dump_field_real(buf, &clfd[nflds], cl->data, NULL);
		break;
	      case CL_FIELD_STRING:
	      case CL_FIELD_STRING_PIC_TOK:
		clarion_dump_field_string(buf, &clfd[nflds], cl->data, NULL, cl->charset);
		break;
	      case CL_FIELD_BYTE:
		clarion_dump_field_byte(buf, &clfd[nflds], cl->data, NULL);
		break;
	      case CL_FIELD_SHORT:
		clarion_dump_field_short(buf, &clfd[nflds], cl->data, NULL);
		break;
	      case CL_FIELD_DECIMAL:
		clarion_dump_field_decimal(buf, &clfd[nflds], cl->data, NULL);
		break;
	      default:
	      fprintf(stderr, "Unknown field type %d\n", clfd[nflds].fldtype);
	      break;
	    }
	}

      if ((clh->sfatr & CL_MEMO_FILE_EXISTS) && (!(cl->opts & CL_OPT_NO_MEMO)))
	{
	  fprintf(stdout, "%c", cl->fsep);
	  clarion_dump_memo_entry(&clrh, cl->memo, NULL, cl->charset);
	}

      fprintf(stdout, "\n");

      fflush(stdout);
    }

  free(buf);
}
