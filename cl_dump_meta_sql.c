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
 * $Id: cl_dump_meta_sql.c 68 2010-11-27 10:20:15Z julien $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <endian.h>
#include <byteswap.h>

#include "cldump.h"

static void
clarion_dump_field_desc_sql(ClarionHandle *cl)
{
  int i, j;
  uint8_t buf[17];
  uint8_t *pbuf;
  ClarionFieldDesc *clfd;

  clfd = cl->clm.clfd;

  for (i = 0; i < cl->clm.clh->numflds; i++)
    {
      memcpy(buf, clfd[i].fldname, 17);
      clarion_trim(buf, 16);
      pbuf = (uint8_t *)strchr((char *)buf, ':');

      if (pbuf != NULL)
	pbuf++;
      else
	pbuf = buf;

      for (j = 0; j < strlen((char *)pbuf); j++)
	{
	  pbuf[j] = tolower(pbuf[j]);
	}

      /*
       * A field with type CL_FIELD_GROUP is a pseudo-field
       * used to indicate that the next clfd[i].length fields
       * are grouped together.
       */
      if (clfd[i].fldtype == CL_FIELD_GROUP)
	{
	  fprintf(stdout, "\n-- Next %d columns were part of group named '%s'", clfd[i].length, pbuf);

	  continue;
	}

      fprintf(stdout, "\n   %c%s%c ", cl->sql_quote_begin, pbuf, cl->sql_quote_end);

      switch(clfd[i].fldtype)
	{
	  case CL_FIELD_LONG:
	    fprintf(stdout, "BIGINT"); /* not really standard SQL, but industry standard */
	    break;
	  case CL_FIELD_REAL:
	    fprintf(stdout, "FLOAT"); /* should mean "double precision" to most RDBMS */
	    break;
	  case CL_FIELD_STRING:
	  case CL_FIELD_STRING_PIC_TOK:
	    fprintf(stdout, "VARCHAR(%d)", clfd[i].length);
	    break;
	  case CL_FIELD_BYTE:
	    fprintf(stdout, "SMALLINT");
	    break;
	  case CL_FIELD_SHORT:
	    fprintf(stdout, "SMALLINT");
	    break;
	  case CL_FIELD_DECIMAL:
	    fprintf(stdout, "NUMERIC(%d,%d)", clfd[i].decsig+2+clfd[i].decdec, clfd[i].decdec);
	    break;
	  default:
	    fprintf(stderr, "Unknown field type %d for field %s !!\n", clfd[i].fldtype, buf);
	    break;
	}

      if (i < cl->clm.clh->numflds - 1)
	fprintf(stdout, ",");
    }

  /*
   * FIXME: unimplemented
   * -> arrnum and picnum
   */
  
  /* Memo field */
  if ((cl->clm.clh->sfatr & CL_MEMO_FILE_EXISTS) && (!(cl->opts & CL_OPT_NO_MEMO)))
    {
      fprintf(stdout, ",\n   %cmemo%c TEXT", cl->sql_quote_begin, cl->sql_quote_end);
    }

  fflush(stdout);
}

static void
clarion_dump_key_desc_sql (ClarionHandle *cl, ClarionKeyDesc *clk, ClarionFieldDesc *clfd, uint8_t numbkeys, char *tbl)
{
  int i, j, k, l;
  ClarionKeyPart *clkp;
  int numparts;
  uint8_t buf[17];
  uint8_t *pbuf;

  fprintf(stdout, "\n");

  for (i = 0; i < numbkeys; i++)
    {
      memcpy(buf, clk[i].keyname, 17);
      clarion_trim(buf, 16);
      pbuf = (uint8_t *)strchr((char *)buf, ':');

      if (pbuf != NULL)
	pbuf++;
      else
	pbuf = buf;

      for (j = 0; j < strlen((char *)pbuf); j++)
	{
	  pbuf[j] = tolower(pbuf[j]);
	}

      if (!(clk[i].keytype & CL_KEYTYPE_DUPSW))
	{
	  fprintf(stdout, "CREATE UNIQUE INDEX %c%s_%s%c ON %c%s%c (",
		  cl->sql_quote_begin, tbl, pbuf, cl->sql_quote_end,
		  cl->sql_quote_begin, tbl, cl->sql_quote_end);
	}
      else
	{
	  fprintf(stdout, "CREATE INDEX %c%s_%s%c ON %c%s%c (",
		  cl->sql_quote_begin, tbl, pbuf, cl->sql_quote_end,
		  cl->sql_quote_begin, tbl, cl->sql_quote_end);
	}

      for (j = 0; j < clk[i].numcomps; j++)
	{
	  if (clk[i].keypart[j].fldtype == CL_FIELD_GROUP)
	    {
	      clkp = clk[i].keypart[j].subpart;
	      numparts = clk[i].keypart[j].numparts;
	    }
	  else
	    {
	      clkp = &clk[i].keypart[j];
	      numparts = 1;
	    }

	  for (k = 0; k < numparts; k++)
	    {
	      memcpy(buf, clfd[clkp[k].fldnum - 1].fldname, 17);
	      clarion_trim(buf, 16);
	      pbuf = (uint8_t *)strchr((char *)buf, ':');

	      if (pbuf != NULL)
		pbuf++;
	      else
		pbuf = buf;

	      for (l = 0; l < strlen((char *)pbuf); l++)
		{
		  pbuf[l] = tolower(pbuf[l]);
		}

	      if ((j > 0) || (k > 0))
		fprintf(stdout, ", ");

	      fprintf(stdout, "%c%s%c", cl->sql_quote_begin, pbuf, cl->sql_quote_end);
	    }
	}

      fprintf(stdout, ");\n");
    }

  fprintf(stdout, "\n");

  fflush(stdout);
}

void
clarion_dump_schema_sql (ClarionHandle *cl)
{
  int i;
  char *buf, *pbuf;

  buf = strdup(cl->datfile);
  buf[strlen(buf) - 4] = '\0';
  pbuf = strrchr(buf, '/');

  if (pbuf != NULL)
    pbuf++;
  else
    pbuf = buf;

  for (i = 0; i < strlen(pbuf); i++)
    {
      pbuf[i] = tolower(pbuf[i]);
    }

  fprintf(stdout, "CREATE TABLE %c%s%c (", cl->sql_quote_begin, pbuf, cl->sql_quote_end);

  clarion_dump_field_desc_sql(cl);
  fprintf(stdout, "\n);\n");

  clarion_dump_key_desc_sql(cl, cl->clm.clk, cl->clm.clfd, cl->clm.clh->numbkeys, pbuf);

  free(buf);
}
