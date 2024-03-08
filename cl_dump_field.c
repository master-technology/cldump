/*
 * cldump - Dumps Clarion databases to text, SQL and CSV formats
 *
 * Copyright (C) 2004-2006 Julien BLACHE <jb@jblache.org>
 * Copyright (C) 2006 Alasdair Craig <acraig@frogfoot.net>
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
 * $Id: cl_dump_field.c 69 2010-11-27 10:20:16Z julien $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <byteswap.h>

#include "cldump.h"

void
clarion_dump_memo_entry (ClarionRecordHeader *clrh, FILE *fp, char *plchold, char *charset)
{
  ClarionMemoEntry clme;
  char *utf;
  uint32_t curblk;

  if ((clrh->rhd & CL_RECORD_DELETED) || (clrh->rptr == 0))
    {
      if (plchold != NULL)
	fprintf(stdout, "%s", plchold);

      return;
    }

  fseek(fp, (((clrh->rptr - 1) * 256) + 6), SEEK_SET);

  curblk = clrh->rptr - 1;
  do {
    fread(&clme.nxtblk, 4, 1, fp);
    fread(&clme.memo, 1, 252, fp);
    
    clme.memo[252] = '\0';

    if (clme.nxtblk == 0)
      clarion_trim(clme.memo, 252);

    if (charset != NULL)
      {
	utf = clarion_iconv(charset, (char *)clme.memo);

	if (utf != NULL)
	  {
	    fprintf(stdout, "%s", utf);
	    free(utf);
	  }
	else
	  fprintf(stdout, "%s", clme.memo);
      }
    else
      fprintf(stdout, "%s", clme.memo);

    if (clme.nxtblk == 0)
      break;
    else if (clme.nxtblk == curblk)
      {
	fprintf(stderr, "Memo entry %08x looping back to itself\n", curblk);
	break;
      }

    fseek(fp, ((clme.nxtblk * 256) + 6), SEEK_SET);
    curblk = clme.nxtblk;
  } while (1);

  fflush(stdout);
}

void
clarion_dump_field_long (uint8_t *buf, ClarionFieldDesc *clfd, FILE *fp, char *plchold)
{
  uint32_t *lbuf = (uint32_t *) buf;

  fread(buf, clfd->length, 1, fp);

  /* Clear the starting uninitialized byte */
  if ((*lbuf >> 24) & 0x80)
    *lbuf &= 0x00ffffff;

  fprintf(stdout, "%d", *lbuf);
  fflush(stdout);
}

void
clarion_dump_field_real (uint8_t *buf, ClarionFieldDesc *clfd, FILE *fp, char *plchold)
{
  double *dbuf = (double *) buf;
  uint8_t uninit[8];

  fread(buf, clfd->length, 1, fp);

  /* Uninitialized: BO FF FF FF FF FF EF FF */
  memset(uninit, 0xff, 8);
#if BYTE_ORDER == BIG_ENDIAN
  uninit[7] = 0xb0;
  uninit[1] = 0xef;
#else
  uninit[0] = 0xb0;
  uninit[6] = 0xef;
#endif

  if (memcmp(uninit, buf, clfd->length) == 0)
    {
      if (plchold != NULL)
	fprintf(stdout, "%s", plchold);
    }
  else
    fprintf(stdout, "%*f", clfd->decdec, *dbuf);

  fflush(stdout);
}

void
clarion_dump_field_string (uint8_t *buf, ClarionFieldDesc *clfd, FILE *fp, char *plchold, char *charset)
{
  char *utf;

  fread(buf, 1, clfd->length, fp);
  buf[clfd->length] = '\0';
  clarion_trim(buf, clfd->length);

  if (strlen((char *)buf) > 0)
    {
      if (charset != NULL)
	{
	  utf = clarion_iconv(charset, (char *)buf);

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
  else if (plchold != NULL)
    fprintf(stdout, "%s", plchold);

  fflush(stdout);
}

void
clarion_dump_field_byte (uint8_t *buf, ClarionFieldDesc *clfd, FILE *fp, char *plchold)
{
  fread(buf, clfd->length, 1, fp);

  fprintf(stdout, "%d", *buf);
  fflush(stdout);
}

void
clarion_dump_field_short (uint8_t *buf, ClarionFieldDesc *clfd, FILE *fp, char *plchold)
{
  uint16_t *sbuf = (uint16_t *) buf;

  fread(buf, clfd->length, 1, fp);

  fprintf(stdout, "%d", *sbuf);
  fflush(stdout);
}

void
clarion_dump_field_decimal (uint8_t *buf, ClarionFieldDesc *clfd, FILE *fp, char *plchold)
{
  int i;
  int count;
  char *cbuf, *obuf;
  uint8_t mask;

  fread(buf, 1, clfd->length, fp);

  /*
   * BCD means two figures per byte
   * We eventually need to put a . somewhere
   * Not forgetting the '\0'
   */
  cbuf = (char *) malloc(clfd->length * 2 + 2);

  /* Odd number of figures, strip the first nibble */
  mask = (clfd->decsig % 2) ? 0x0f : 0xf0;

  i = 0;
  count = 0;
  while (i < clfd->length)
    {
      if (count == clfd->decsig - clfd->decdec)
	{
	  cbuf[count] = '.';
	  count++;
	}

      if (mask == 0x0f)
	{
	  cbuf[count] = (buf[i] & mask) + '0';
	  mask = 0xf0;
	  i++;
	}
      else
	{
	  cbuf[count] = (buf[i] >> 4) + '0';
	  mask = 0x0f;
	}

      count++;
    }
  cbuf[count] = '\0';

  /* Strip the leading zeros */  
  for (obuf = cbuf; i < strlen(cbuf); obuf++)
    {
      if (*obuf != '0')
	break;
    }

  if (*obuf == '.')
    fprintf(stdout, "0%s", obuf);
  else
    {
      if (strlen(obuf) > 0)
	fprintf(stdout, "%s", obuf);
      else if (plchold != NULL)
	fprintf(stdout, "%s", plchold);
    }

  free(cbuf);

  fflush(stdout);
}
