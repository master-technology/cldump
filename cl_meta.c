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
 * $Id: cl_meta.c 65 2010-11-27 10:20:08Z julien $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <byteswap.h>

#include "cldump.h"

int
clarion_read_header (ClarionHandle *cl)
{
  ClarionHeader *clh;
  FILE *fp = cl->data;

  clh = (ClarionHeader *) malloc(sizeof(ClarionHeader));

  if (clh == NULL)
    return -1;

  fread(&clh->filesig, 2, 1, fp);

  if (clh->filesig != CL_DATA_FILE_SIG)
    {
      free(clh);
      fprintf(stderr, "Invalid data file !\n");
      return -1;
    }

  fread(&clh->sfatr, 2, 1, fp);
  fread(&clh->numbkeys, 1, 1, fp);
  fread(&clh->numrecs, 4, 1, fp);
  fread(&clh->numdels, 4, 1, fp);
  fread(&clh->numflds, 2, 1, fp);
  fread(&clh->numpics, 2, 1, fp);
  fread(&clh->numarrs, 2, 1, fp);
  fread(&clh->reclen, 2, 1, fp);
  fread(&clh->offset, 4, 1, fp);
  fread(&clh->logeof, 4, 1, fp);
  fread(&clh->logbof, 4, 1, fp);
  fread(&clh->freerec, 4, 1, fp);
  fread(clh->recname, 1, 12, fp);
  clh->recname[12] = '\0';
  fread(clh->memnam, 1, 12, fp);
  clh->memnam[12] = '\0';
  fread(clh->filpre, 1, 3, fp);
  clh->filpre[3] = '\0';
  fread(clh->recpre, 1, 3, fp);
  clh->recpre[3] = '\0';
  fread(&clh->memolen, 2, 1, fp);
  fread(&clh->memowid, 2, 1, fp);
  fread(&clh->reserved, 4, 1, fp);
  fread(&clh->chgtime, 4, 1, fp);
  fread(&clh->chgdate, 4, 1, fp);
  fread(&clh->reserved2, 2, 1, fp);

  cl->clm.clh = clh;

  return 0;
}


int
clarion_read_field_desc(ClarionHandle *cl)
{
  int i;
  int numflds = cl->clm.clh->numflds;
  FILE *fp = cl->data;
  ClarionFieldDesc *clfd;

  if (numflds == 0)
    return 0;

  clfd = (ClarionFieldDesc *) malloc(numflds * sizeof(ClarionFieldDesc));

  if (clfd == NULL)
    return -1;

  for (i = 0; i < numflds; i++)
    {
      fread(&clfd[i].fldtype, 1, 1, fp);
      fread(&clfd[i].fldname, 1, 16, fp);
      clfd[i].fldname[16] = '\0';
      fread(&clfd[i].foffset, 2, 1, fp);
      fread(&clfd[i].length, 2, 1, fp);
      fread(&clfd[i].decsig, 1, 1, fp);
      fread(&clfd[i].decdec, 1, 1, fp);
      fread(&clfd[i].arrnum, 2, 1, fp);
      fread(&clfd[i].picnum, 2, 1, fp);
      clfd[i].arr = NULL;
      clfd[i].nbarrs = 0;
    }

  cl->clm.clfd = clfd;

  return 0;
}

int
clarion_read_key_desc (ClarionHandle *cl)
{
  int i, j, k;
  int l, r;
  int numbkeys = cl->clm.clh->numbkeys;
  int numparts;
  FILE *fp = cl->data;
  FILE *fk;
  ClarionKeyDesc *clk;
  ClarionKeyPart *subpart;
  ClarionFieldDesc *clfd;
  char *keyfile;

  if (numbkeys == 0)
    return 0;

  clk = (ClarionKeyDesc *) malloc(numbkeys * sizeof(ClarionKeyDesc));

  if (clk == NULL)
    return -1;

  keyfile = strdup(cl->datfile);
  keyfile[strlen(keyfile) - 3] = 'K';

  clfd = cl->clm.clfd;

  for (i = 0; i < numbkeys; i++)
    {
      fread(&clk[i].numcomps, 1, 1, fp);
      fread(&clk[i].keyname, 1, 16, fp);
      fread(&clk[i].comptype, 1, 1, fp);
      fread(&clk[i].complen, 1, 1, fp);

      /* Read the keytype from the key file */
      l = (i + 1) / 16;
      r = (i + 1) % 16;
      keyfile[strlen(keyfile) - 2] = (l > 9) ? 'a' + (l - 10) : '0' + l;
      keyfile[strlen(keyfile) - 1] = (r > 9) ? 'a' + (r - 10) : '0' + r;

      fk = fopen(keyfile, "rb");

      if (fk != NULL)
	{
	  fseek(fk, 29, SEEK_SET);
	  fread(&clk[i].keytype, 1, 1, fk);
	  fclose(fk);
	}
      else
	{
	  clk[i].keytype = CL_KEYTYPE_ERROR;
	  fprintf(stderr, "Couldn't open key file %s !\n", keyfile);
	}
      /* Done */

      clk[i].keypart = (ClarionKeyPart *) malloc(clk[i].numcomps * sizeof(ClarionKeyPart));

      for (j = 0; j < clk[i].numcomps; j++)
	{
	  fread(&clk[i].keypart[j].fldtype, 1, 1, fp);
	  fread(&clk[i].keypart[j].fldnum, 2, 1, fp);
	  fread(&clk[i].keypart[j].elmoff, 2, 1, fp);
	  fread(&clk[i].keypart[j].elmlen, 1, 1, fp);

	  /*
	   * Collect subparts if this part of the key is
	   * a field of type CL_FIELD_GROUP
	   */
	  if (clk[i].keypart[j].fldtype == CL_FIELD_GROUP)
	    {
	      numparts = clfd[clk[i].keypart[j].fldnum - 1].length;
	      clk[i].keypart[j].numparts = numparts;

	      subpart = (ClarionKeyPart *) malloc(numparts * sizeof(ClarionKeyPart));
	      clk[i].keypart[j].subpart = subpart;

	      for (k = 0; k < numparts; k++)
		{
		  subpart[k].fldnum = clk[i].keypart[j].fldnum + 1 + k;
		  subpart[k].fldtype = clfd[subpart[k].fldnum - 1].fldtype;
		  subpart[k].elmoff = clfd[subpart[k].fldnum - 1].foffset;
		  subpart[k].elmlen = clfd[subpart[k].fldnum - 1].length;
		  subpart[k].numparts = 0;
		  subpart[k].subpart = NULL;
		}
	    }
	  else
	    {
	      clk[i].keypart[j].numparts = 0;
	      clk[i].keypart[j].subpart = NULL;
	    }
	}
    }

  cl->clm.clk = clk;

  free(keyfile);

  return 0;
}

int
clarion_read_pic_desc (ClarionHandle *cl)
{
  int i;
  int numpics = cl->clm.clh->numpics;
  FILE *fp = cl->data;
  ClarionPicDesc *clp;

  if (numpics == 0)
    return 0;

  clp = (ClarionPicDesc *) malloc(numpics * sizeof(ClarionPicDesc));

  if (clp == NULL)
    return -1;

  for (i = 0; i < numpics; i++)
    {
      fread(&clp[i].piclen, 2, 1, fp);

      clp[i].picstr = (uint8_t *) malloc(clp[i].piclen + 1);

      fread(clp[i].picstr, 1, clp[i].piclen, fp);
      clp[i].picstr[clp[i].piclen] = '\0';
    }

  cl->clm.clp = clp;

  return 0;
}

void
clarion_read_arr_desc (ClarionHandle *cl)
{
  int i, j, k;
  int numflds = cl->clm.clh->numflds;
  unsigned long offset = cl->clm.clh->offset;
  FILE *fp = cl->data;
  ClarionFieldDesc *clfd = cl->clm.clfd;
  uint16_t numdim, totdim;

  for (i = 0; i < numflds; i++)
    {
      if (clfd[i].arrnum != 0)
	{
	  j = 0;
	  do
	    {
	      clfd[i].arr = (ClarionArrDesc *) realloc(clfd[i].arr, (j + 1) * sizeof(ClarionArrDesc));
	      
	      fread(&clfd[i].arr[j].numdim, 2, 1, fp);
	      fread(&clfd[i].arr[j].totdim, 2, 1, fp);
	      fread(&clfd[i].arr[j].elmsiz, 2, 1, fp);

	      clfd[i].arr[j].part = (ClarionArrPart *) malloc(clfd[i].arr[j].totdim * sizeof(ClarionArrPart));

	      for (k = 0; k < clfd[i].arr[j].totdim; k++)
		{
		  fread(&clfd[i].arr[j].part[k].maxdim, 2, 1, fp);
		  fread(&clfd[i].arr[j].part[k].lendim, 2, 1, fp);
		}

	      if (ftell(fp) == offset)
		{
		  clfd[i].nbarrs = j + 1;
		  return;
		}

	      fread(&numdim, 2, 1, fp);
	      fread(&totdim, 2, 1, fp);

	      fseek(fp, (ftell(fp) - 4), SEEK_SET);

	      if (numdim != totdim)
		j++;
	      else
		{
		  clfd[i].nbarrs = j + 1;
		  break;
		}

	    } while (1);
	}
    }
}
