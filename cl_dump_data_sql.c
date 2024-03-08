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
 * $Id: cl_dump_data_sql.c 68 2010-11-27 10:20:15Z julien $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <endian.h>
#include <byteswap.h>

#include "cldump.h"

char tempBuff[32768];

static void
clarion_dump_field_string_sql (uint8_t *buf, ClarionFieldDesc *clfd, FILE *fp, char *charset)
{
  char *utf;
  int i=0,j=0, len;

  fread(buf, 1, clfd->length, fp);
  buf[clfd->length] = '\0';
  clarion_trim(buf, clfd->length);

  len = strlen((char *)buf);
  if (len > 0)
    {
      // Dirty hack to deal with single Quotes...
      for (i=0;i<=len;i++) {
	tempBuff[i+j] = buf[i];
	if (buf[i] == '\'') { j++; tempBuff[i+j] = '\''; }
	if (buf[i] == '\\') { j++; tempBuff[i+j] = '\\'; }
      }

      if (charset != NULL)
	{
	  utf = clarion_iconv(charset, (char *)tempBuff);

	  if (utf != NULL)
	    {
	      fprintf(stdout, "'%s'", utf);
	      free(utf);
	    }
	  else {
	    fprintf(stdout, "'%s'", tempBuff);
	  }
	}
      else
	fprintf(stdout, "'%s'", tempBuff);
    }
  else
    fprintf(stdout, "NULL");

  fflush(stdout);
}

static void
clarion_dump_memo_entry_sql (ClarionRecordHeader *clrh, FILE *fp, char *charset)
{
  int i, j;
  ClarionMemoEntry clme;
  char buf[512];
  char *utf;

  if ((clrh->rhd & CL_RECORD_DELETED) || (clrh->rptr == 0))
    {
      fprintf(stdout, "NULL");

      return;
    }

  fseek(fp, (((clrh->rptr - 1) * 256) + 6), SEEK_SET);

  fprintf(stdout, "'");

  do {
    memset(buf, 0, 512);
    fread(&clme.nxtblk, 4, 1, fp);
    fread(&clme.memo, 1, 252, fp);
    
    clme.memo[252] = '\0';

    if (clme.nxtblk == 0)
      {
	clarion_trim(clme.memo, 252);

	clarion_singlespace((char *)clme.memo);

	/* Sanitize the \r\n mess */
	for (i = 0, j = 0; clme.memo[i] != '\0'; i++, j++)
	  {
	    switch(clme.memo[i])
	      {
	        case '\n':
		  buf[j++] = '\\';
		  buf[j] = 'n';
		  break;
	        case '\r':
		  j--;
		  break;
	        case '\'':
		  buf[j++] = '\'';
		  buf[j] = '\'';
		  break;
	        default:
		  buf[j] = clme.memo[i];
	      }
	  }
      }

    if (strlen(buf) > 0)
      {
	if (charset != NULL)
	  {
	    utf = clarion_iconv(charset, buf);

	    if (utf != NULL)
	      {
		fprintf(stdout, "%s", utf);
		free(utf);
	      }
	    else
	      fprintf(stdout, "%s", buf);
	  }
	else
	  fprintf(stdout, "%s", buf);
      }

    if (clme.nxtblk == 0)
      break;
    else
      fseek(fp, ((clme.nxtblk * 256) + 6), SEEK_SET);
  } while (1);

  fprintf(stdout, "'");

  fflush(stdout);
}

void
clarion_dump_data_sql (ClarionHandle *cl)
{
  int i;
  int nrecs, nflds;
  FILE *fp = cl->data;
  ClarionHeader *clh = cl->clm.clh;
  ClarionFieldDesc *clfd = cl->clm.clfd;
  ClarionRecordHeader clrh;
  uint8_t *buf;
  char *cbuf, *tblname;
  char *rhd[8] = {
    "NEW RECORD",
    "OLD RECORD",
    "REVISED RECORD",
    "*** UNDEFINED (3) ***",
    "DELETED RECORD",
    "*** UNDEFINED (5) ***",
    "RECORD HELD",
    "*** UNDEFINED (7) ***"
  };

  buf = (uint8_t *) malloc(clh->reclen * sizeof(uint8_t));

  cbuf = strdup(cl->datfile);
  cbuf[strlen(cbuf) - 4] = '\0';
  tblname = strrchr(cbuf, '/');

  if (tblname != NULL)
    tblname++;
  else
    tblname = cbuf;

  for (i = 0; i < strlen(tblname); i++)
    {
      tblname[i] = tolower(tblname[i]);
    }

  for (nrecs = 0; nrecs < clh->numrecs; nrecs++)
    {      
      fread(&clrh.rhd, 1, 1, fp);
      fread(&clrh.rptr, 4, 1, fp);

      if (clrh.rhd & CL_RECORD_DELETED)
	{
	  if (cl->opts & CL_OPT_DUMP_ACTIVE)
	    {
	      fseek(cl->data, (clh->reclen - 5), SEEK_CUR);
	      continue;
	    }
	  else
	    {
	      fprintf(stdout, "-- Record attributes:");

	      for (i = 0; i < 8; i++)
		{
		  if ((clrh.rhd >> i) & 0x01)
		    fprintf(stdout, " [%s]", rhd[i]);
		}
	      fprintf(stdout, "\n");
	    }
	}

      fprintf(stdout, "INSERT INTO %c%s%c VALUES(", cl->sql_quote_begin, tblname, cl->sql_quote_end);

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

	  switch (clfd[nflds].fldtype)
	    {
	      case CL_FIELD_LONG:
		clarion_dump_field_long(buf, &clfd[nflds], cl->data, "NULL");
		break;
	      case CL_FIELD_REAL:
		clarion_dump_field_real(buf, &clfd[nflds], cl->data, "NULL");
		break;
	      case CL_FIELD_STRING:
	      case CL_FIELD_STRING_PIC_TOK:
		clarion_dump_field_string_sql(buf, &clfd[nflds], cl->data, cl->charset);
		break;
	      case CL_FIELD_BYTE:
		clarion_dump_field_byte(buf, &clfd[nflds], cl->data, "NULL");
		break;
	      case CL_FIELD_SHORT:
		clarion_dump_field_short(buf, &clfd[nflds], cl->data, "NULL");
		break;
	      case CL_FIELD_DECIMAL:
		clarion_dump_field_decimal(buf, &clfd[nflds], cl->data, "NULL");
		break;
	      default:
	      fprintf(stderr, "Unknown field type %d\n", clfd[nflds].fldtype);
	      break;
	    }

	  if (nflds < clh->numflds - 1)
	    fprintf(stdout, ", ");
	}

      if ((clh->sfatr & CL_MEMO_FILE_EXISTS) && (!(cl->opts & CL_OPT_NO_MEMO)))
	{
	  fprintf(stdout, ", ");
	  clarion_dump_memo_entry_sql(&clrh, cl->memo, cl->charset);
	}

      fprintf(stdout, ");\n");

      fflush(stdout);
    }

  free(buf);
}
