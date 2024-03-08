/*
 * cldump - Dumps Clarion databases to text, SQL and CSV formats
 *
 * Copyright (C) 2004-2006,2010 Julien BLACHE <jb@jblache.org>
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
 * $Id: cl_dump_meta.c 65 2010-11-27 10:20:08Z julien $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <byteswap.h>

#include "cldump.h"

static void
clarion_dump_header (ClarionHeader *clh)
{
  int i;
  int tmp, val, year, day;
  char *sfatr[8] = {
    "FILE LOCKED",
    "FILE OWNED",
    "RECORDS ENCRYPTED",
    "MEMO FILE EXISTS",
    "FILE COMPRESSED",
    "RECLAIM DELETED RECORDS",
    "READ ONLY",
    "MAY BE CREATED"
  };
  int days_in_month[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
  };

  fprintf(stderr, "===== FILE HEADER FOLLOWS =====\n");

  fprintf(stderr, "filesig  : 0x%04x\n", clh->filesig);
  fprintf(stderr, "sfatr    : 0x%04x\n", clh->sfatr);
  fprintf(stderr, "\tAttributes set:");

  if (clh->sfatr & 0x00ff)
    {
      for (i = 0; i < 8; i++)
	{
	  if ((clh->sfatr >> i) & 0x0001)
	    fprintf(stderr, " [%s]", sfatr[i]);
	}
    }
  else
    {
      fprintf(stderr, " NONE");
    }

  fprintf(stderr, "\n");

  fprintf(stderr, "numbkeys : 0x%02x\n", clh->numbkeys);
  fprintf(stderr, "numrecs  : 0x%08x\n", clh->numrecs);
  fprintf(stderr, "numdels  : 0x%08x\n", clh->numdels);
  fprintf(stderr, "numflds  : 0x%04x\n", clh->numflds);
  fprintf(stderr, "numpics  : 0x%04x\n", clh->numpics);
  fprintf(stderr, "numarrs  : 0x%04x\n", clh->numarrs);
  fprintf(stderr, "reclen   : 0x%04x\n", clh->reclen);
  fprintf(stderr, "offset   : 0x%08x\n", clh->offset);
  fprintf(stderr, "logeof   : 0x%08x\n", clh->logeof);
  fprintf(stderr, "logbof   : 0x%08x\n", clh->logbof);
  fprintf(stderr, "freerec  : 0x%08x\n", clh->freerec);
  fprintf(stderr, "recname  : [%s]", clh->recname);
  for (i = 0; i < 12; i++)
    {
      fprintf(stderr, " 0x%02x", clh->recname[i]);
    }
  fprintf(stderr, "\n");
  fprintf(stderr, "memnam   : [%s]", clh->memnam);
  for (i = 0; i < 12; i++)
    {
      fprintf(stderr, " 0x%02x", clh->memnam[i]);
    }
  fprintf(stderr, "\n");
  fprintf(stderr, "filpre   : [%s] ", clh->filpre);
  fprintf(stderr, "0x%02x 0x%02x 0x%02x\n", clh->filpre[0], clh->filpre[1], clh->filpre[2]);
  fprintf(stderr, "recpre   : [%s] ", clh->recpre);
  fprintf(stderr, "0x%02x 0x%02x 0x%02x\n", clh->recpre[0], clh->recpre[1], clh->recpre[2]);
  fprintf(stderr, "memolen  : 0x%04x\n", clh->memolen);
  fprintf(stderr, "memowid  : 0x%04x\n", clh->memowid);
  fprintf(stderr, "reserved : 0x%08x\n", clh->reserved);

  fprintf(stderr, "chgtime  : 0x%08x [", clh->chgtime);
  if ((clh->chgtime < 1) || (clh->chgtime > 8640000))
    {
      fprintf(stderr, "INVALID");
    }
  else
    {
      tmp = (clh -> chgtime - 1);
      val = tmp / 360000;
      fprintf(stderr, "%02d:", val);
      tmp = clh->chgtime % 360000;
      val = tmp / 6000;
      fprintf(stderr, "%02d:", val);
      tmp = tmp % 6000;
      val = tmp / 100;
      fprintf(stderr, "%02d.%02d", val, (tmp % 100));
    }
  fprintf(stderr, "]\n");

  fprintf(stderr, "chgdate  : 0x%08x [", clh->chgdate);
  if ((clh->chgdate <= 3) || (clh->chgdate > 109211))
    {
      fprintf(stderr, "INVALID");
    }
  else
    {
      tmp = (clh->chgdate > 36527) ? (clh->chgdate - 3) : (clh->chgdate - 4);
      year = (1801 + (4 * (tmp / 1461)));
      tmp = tmp % 1461;
      if (tmp != 1460)
	{
	  year += (tmp / 365);
	  day = tmp % 365;
	}
      else
	{
	  year += 3;
	  day = 365;
	}
      year += (year < 100) ? 1900 : 0;
      if ((year % 4 == 0) && (year != 1900))
	days_in_month[1] = 29;
      for (i = 0; i < 12; i++)
	{
	  day -= days_in_month[i];
	  if (day < 0)
	    {
	      day += days_in_month[i] + 1;
	      break;
	    }
	}
      fprintf(stderr, "%d-%02d-%02d", year, (i + 1), day);
    }
  fprintf(stderr, "]\n");

  fprintf(stderr, "reserved2: 0x%04x\n", clh->reserved2);

  fprintf(stderr, "===== END OF FILE HEADER =====\n\n");

  fflush(stderr);
}

static void
clarion_dump_pic_desc (ClarionPicDesc *clpd)
{
  int i;

  fprintf(stderr, "  === PICTURE DESCRIPTOR ===\n\n");
  fprintf(stderr, "\tpiclen : 0x%04x\n", clpd->piclen);
  fprintf(stderr, "\tpicstr : [%s]", clpd->picstr);
  for (i = 0; i < clpd->piclen; i++)
    {
      fprintf(stderr, " 0x%02x", clpd->picstr[i]);
    }
  fprintf(stderr, "\n");

  fflush(stderr);
}

static void
clarion_dump_arr_desc (ClarionArrDesc *clad, int nbarrs)
{
  int i, j;

  for (i = 0; i < nbarrs; i++)
    {
      fprintf(stderr, "   === ARRAY DESCRIPTOR %d ===\n", (i + 1));
      fprintf(stderr, "\tnumdim : 0x%04x\n", clad[i].numdim);
      fprintf(stderr, "\ttotdim : 0x%04x\n", clad[i].totdim);
      fprintf(stderr, "\telmsiz : 0x%04x\n", clad[i].elmsiz);
      
      for (j = 0; j < clad[i].totdim; j++)
	{
	  fprintf(stderr, "     === ARRAY PART %d ===\n", (j + 1));
	  fprintf(stderr, "\tmaxdim : 0x%04x\n", clad[i].part[j].maxdim);
	  fprintf(stderr, "\tlendim : 0x%04x\n", clad[i].part[j].lendim);
	}
    }

  fflush(stderr);
}

static void
clarion_dump_field_desc(ClarionFieldDesc *clfd, uint16_t numflds, ClarionPicDesc *clpd)
{
  int i, j;
  char *fldtype[9] = {
    "*** UNDEFINED ***",
    "LONG",
    "REAL",
    "STRING",
    "STRING WITH PICTURE TOKEN",
    "BYTE",
    "SHORT",
    "GROUP",
    "DECIMAL"
  };

  fprintf(stderr, "===== FIELD DESCRIPTORS FOLLOW =====\n\n");

  for (i = 0; i < numflds; i++)
    {
      fprintf(stderr, "fldtype : 0x%02x (%s)\n", clfd[i].fldtype, fldtype[clfd[i].fldtype]);
      fprintf(stderr, "fldname : [%s]", clfd[i].fldname);
      for (j = 0; j < 16; j++)
	{
	  fprintf(stderr, " 0x%02x", clfd[i].fldname[j]);
	}
      fprintf(stderr, "\n");
      fprintf(stderr, "foffset : 0x%04x\n", clfd[i].foffset);
      fprintf(stderr, "length  : 0x%04x\n", clfd[i].length);
      fprintf(stderr, "decsig  : 0x%02x\n", clfd[i].decsig);
      fprintf(stderr, "decdec  : 0x%02x\n", clfd[i].decdec);
      fprintf(stderr, "arrnum  : 0x%04x (%d)\n", clfd[i].arrnum, clfd[i].nbarrs);

      if (clfd[i].nbarrs > 0)
	clarion_dump_arr_desc(clfd[i].arr, clfd[i].nbarrs);

      fprintf(stderr, "picnum  : 0x%04x\n", clfd[i].picnum);

      if (clfd[i].picnum != 0)
	clarion_dump_pic_desc(clpd + (clfd[i].picnum - 1));

      fprintf(stderr, "\n");
    }

  fprintf(stderr, "===== END OF FIELD DESCRIPTORS =====\n\n");

  fflush(stderr);
}

static void
clarion_dump_key_desc (ClarionKeyDesc *clk, ClarionFieldDesc *clfd, uint8_t numbkeys)
{
  int i, j, k;
  char *fldtype[9] = {
    "*** UNDEFINED ***",
    "LONG",
    "REAL",
    "STRING",
    "STRING WITH PICTURE TOKEN",
    "BYTE",
    "SHORT",
    "GROUP",
    "DECIMAL"
  };
 
  fprintf(stderr, "===== KEY DESCRIPTORS FOLLOW =====\n\n"); 

  for (i = 0; i < numbkeys; i++) 
    { 
      fprintf(stderr, "numcomps : 0x%02x\n", clk[i].numcomps);
      fprintf(stderr, "keyname  : [%s]", clk[i].keyname);
      for (j = 0; j < 16; j++)
	{
	  fprintf(stderr, " 0x%02x", clk[i].keyname[j]);
	}
      fprintf(stderr, "\n");
      fprintf(stderr, "comptype : 0x%02x\n", clk[i].comptype);
      fprintf(stderr, "complen  : 0x%02x\n", clk[i].complen);

      /* Key type from the key file */
      fprintf(stderr, "keytyp   : 0x%02x (", clk[i].keytype);

      if (clk[i].keytype != CL_KEYTYPE_ERROR)
	{
	  if ((clk[i].keytype & 0x0f) == 1)
	    fprintf(stderr, "[INDEX]");
	  else
	    fprintf(stderr, "[KEY]");

	  if (clk[i].keytype & CL_KEYTYPE_DUPSW)
	    fprintf(stderr, " [DUPLICATES]");

	  if (clk[i].keytype & CL_KEYTYPE_UPRSW)
	    fprintf(stderr, " [UPPERCASE]");

	  if (clk[i].keytype & CL_KEYTYPE_OPTSW)
	    fprintf(stderr, " [PADDED]");

	  if (clk[i].keytype & CL_KEYTYPE_LOKSW)
	    fprintf(stderr, " [LOCKED]");
	}
      else
	fprintf(stderr, "[ERROR]");

      fprintf(stderr, ")\n");
      /* Done */

      for (j = 0; j < clk[i].numcomps; j++)
	{
	  fprintf(stderr, "   === KEYPART %d ===\n", (j+1));
	  fprintf(stderr, "\tfldtype : 0x%02x (%s)\n", clk[i].keypart[j].fldtype, fldtype[clk[i].keypart[j].fldtype]); 
	  fprintf(stderr, "\tfldnum  : 0x%04x ([%s])\n", clk[i].keypart[j].fldnum, clfd[clk[i].keypart[j].fldnum - 1].fldname);
	  fprintf(stderr, "\telmoff  : 0x%04x\n", clk[i].keypart[j].elmoff);
	  fprintf(stderr, "\telmlen  : 0x%02x\n", clk[i].keypart[j].elmlen);

	  for (k = 0; k < clk[i].keypart[j].numparts; k++)
	    {
	      fprintf(stderr, "      === SUBPART %d  ===\n", (k+1));
	      fprintf(stderr, "\t\tfldtype : 0x%02x (%s)\n", clk[i].keypart[j].subpart[k].fldtype, fldtype[clk[i].keypart[j].subpart[k].fldtype]); 
	      fprintf(stderr, "\t\tfldnum  : 0x%04x ([%s])\n", clk[i].keypart[j].subpart[k].fldnum, clfd[clk[i].keypart[j].subpart[k].fldnum - 1].fldname);
	      fprintf(stderr, "\t\telmoff  : 0x%04x\n", clk[i].keypart[j].subpart[k].elmoff);
	      fprintf(stderr, "\t\telmlen  : 0x%02x\n", clk[i].keypart[j].subpart[k].elmlen);
	    }
	}

      fprintf(stderr, "\n");
    }

  fprintf(stderr, "===== END OF KEY DESCRIPTORS =====\n\n");

  fflush(stderr);
}

void
clarion_dump_meta (ClarionHandle *cl)
{
  clarion_dump_header(cl->clm.clh);
}

void
clarion_dump_schema (ClarionHandle *cl)
{
  clarion_dump_field_desc(cl->clm.clfd, cl->clm.clh->numflds, cl->clm.clp);
  clarion_dump_key_desc(cl->clm.clk, cl->clm.clfd, cl->clm.clh->numbkeys);
}
