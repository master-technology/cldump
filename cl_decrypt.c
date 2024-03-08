/*
 * cldump - Dumps Clarion databases to text, SQL and CSV formats
 *
 * Copyright (C) 2010 Julien BLACHE <jb@jblache.org>
 *
 * Decryption based on the Clarion.pm Perl module by
 *  Stas Ukolov <ukoloff@cpan.org>
 *  Ilya Chelpanov <ilya@macro.ru>
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
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <endian.h>
#include <stdint.h>

#include <errno.h>

#include "cldump.h"


static uint8_t key[2];


static int
reopen_dat_sync(ClarionHandle *cl, long fpos, uint8_t *base, uint8_t *pos)
{
  long lret;
  int ret;

  fclose(cl->data);
  cl->data = NULL;

  ret = msync(base, pos - base, MS_SYNC | MS_INVALIDATE);
  if (ret != 0)
    {
      fprintf(stderr, "msync failed: %s\n", strerror(errno));
      return -1;
    }

  cl->data = fopen(cl->datfile, "rb");
  if (!cl->data)
    {
      fprintf(stderr, "Failed to reopen data file: %s\n", strerror(errno));
      return -1;
    }

  lret = fseek(cl->data, fpos, SEEK_SET);
  if (lret < 0)
    {
      fprintf(stderr, "Failed to seek: %s\n", strerror(errno));
      return -1;
    }

  return 0;
}

static void
clarion_decrypt(uint8_t *buf, int len)
{
  int i;

  /* 2-byte blocks, remainder byte left as is */
  for (i = 0; i < (len & ~1); i += 2)
    {
      buf[i] = buf[i] ^ key[0];
      buf[i + 1] = buf[i + 1] ^ key[1];
    }
}

static void
clarion_get_key(ClarionHeader *clh, int mode)
{
  uint32_t hidden;

  switch (mode)
    {
      case CL_KEY_NUMDELS_HI:
	hidden = clh->numdels;
	key[0] = hidden >> 24;
	key[1] = (hidden >> 16) & 0xff;
	break;

      case CL_KEY_RESERVED_HI:
	hidden = clh->reserved;
	key[0] = hidden >> 24;
	key[1] = (hidden >> 16) & 0xff;
	break;

      case CL_KEY_RESERVED_LO:
	hidden = clh->reserved;
	key[0] = (hidden >> 8) & 0xff;
	key[1] = hidden & 0xff;
	break;

      case CL_KEY_RESERVED_MID:
	hidden = clh->reserved;
	key[0] = (hidden >> 8) & 0xff;
	key[1] = (hidden >> 16) & 0xff;
	break;
    }
}

static int
clarion_decrypt_header(ClarionHandle *cl, uint8_t *base)
{
  int ret;

  clarion_decrypt(base + 4, 81);

  /* Re-read header */
  ret = reopen_dat_sync(cl, 0, base, base + 85);
  if (ret < 0)
    return -1;

  free(cl->clm.clh);
  cl->clm.clh = NULL;

  ret = clarion_read_header(cl);
  if (ret != 0)
    {
      fprintf(stderr, "Could not re-read header after decryption\n");

      return -1;
    }

  return 0;
}

static int
clarion_decrypt_schema(ClarionHandle *cl, uint8_t *base)
{
  ClarionFieldDesc *clfd;
  uint8_t *pos;
  long fpos;
  int numcomps;
  int numdim;
  int totdim;
  int piclen;
  int i;
  int j;
  int ret;

  /* field desc */
  fpos = ftell(cl->data);
  if (fpos < 0)
    return -1;

  pos = base + fpos;

  for (i = 0; i < cl->clm.clh->numflds; i++)
    {
      clarion_decrypt(pos, 27);

      pos += 27;
    }

  ret = reopen_dat_sync(cl, fpos, base, pos);
  if (ret < 0)
    return -1;

  ret = clarion_read_field_desc(cl);
  if (ret != 0)
    {
      fprintf(stderr, "Could not read field descriptors after decryption\n");

      return -1;
    }

  clfd = cl->clm.clfd;

  /* key desc */
  fpos = ftell(cl->data);
  if (fpos < 0)
    return -1;

  pos = base + fpos;

  for (i = 0; i < cl->clm.clh->numbkeys; i++)
    {
      clarion_decrypt(pos, 19);

      /* Get the number of components */
      numcomps = pos[0];

      pos += 19;

      for (j = 0; j < numcomps; j++)
	{
	  clarion_decrypt(pos, 6);

	  pos += 6;
	}

      /* We'd decrypt Kxx files here if we needed them */
    }

  ret = reopen_dat_sync(cl, fpos, base, pos);
  if (ret < 0)
    return -1;

  ret = clarion_read_key_desc(cl);
  if (ret != 0)
    {
      fprintf(stderr, "Could not read key descriptors after decryption\n");

      return -1;
    }

  /* pic desc */
  fpos = ftell(cl->data);
  if (fpos < 0)
    return -1;

  pos = base + fpos;

  for (i = 0; i < cl->clm.clh->numpics; i++)
    {
      piclen = (pos[1] << 8) | pos[0];

      clarion_decrypt(pos + 2, piclen);

      pos += 2 + piclen;
    }

  ret = reopen_dat_sync(cl, fpos, base, pos);
  if (ret < 0)
    return -1;

  clarion_read_pic_desc(cl);
  if (ret != 0)
    {
      fprintf(stderr, "Could not read picture descriptors after decryption\n");

      return -1;
    }

  /* arr desc - UNTESTED */
  fpos = ftell(cl->data);
  if (fpos < 0)
    return -1;

  pos = base + fpos;

  for (i = 0; i < cl->clm.clh->numflds; i++)
    {
      if (clfd[i].arrnum == 0)
	continue;

      while ((pos - base) < cl->clm.clh->offset)
	{
	  clarion_decrypt(pos, 6);

	  numdim = (pos[1] << 8) | pos[0];
	  totdim = (pos[3] << 8) | pos[2];

	  pos += 6;

	  clarion_decrypt(pos, totdim * 4);
	  pos += totdim * 4;
	}
    }

  ret = reopen_dat_sync(cl, fpos, base, pos);
  if (ret < 0)
    return -1;

  clarion_read_arr_desc(cl);

  return 0;
}

static int
clarion_decrypt_data(ClarionHandle *cl, uint8_t *base, off_t size)
{
  ClarionHeader *clh;
  uint8_t *pos;
  long fpos;
  int i;

  clh = cl->clm.clh;

  fpos = ftell(cl->data);
  if (fpos < 0)
    return -1;

  if (fpos > clh->offset)
    {
      fprintf(stderr, "Error: starting data decryption past start of data (%lx / %x)\n", fpos, clh->offset);
      exit(1);
    }

  pos = base + fpos;

  for (i = 0; i < clh->numrecs; i++)
    {
      if (pos + clh->reclen > base + size)
	{
	  fprintf(stderr, "EOF reached for DAT file\n");
	  break;
	}

      clarion_decrypt(pos + 5, clh->reclen - 5);

      pos += clh->reclen;
    }

  return 0;
}

static int
clarion_decrypt_memo(ClarionHandle *cl)
{
  struct stat st;
  uint8_t *pos;
  void *mapbase;
  int fd;
  int ret;

  fd = open(cl->memfile, O_RDWR);
  if (fd < 0)
    {
      fprintf(stderr, "Could not open %s: %s\n", cl->memfile, strerror(errno));

      return -1;
    }

  ret = fstat(fd, &st);
  if (ret < 0)
    {
      fprintf(stderr, "fstat failed: %s\n", strerror(errno));

      close(fd);
      return -1;
    }

  mapbase = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapbase == MAP_FAILED)
    {
      fprintf(stderr, "mmap failed: %s\n", strerror(errno));
      close(fd);

      return -1;
    }

  pos = (uint8_t *)mapbase;
  /* Skip file header */
  pos += 6;

  while ((pos - (uint8_t *)mapbase) < st.st_size)
    {
      if (pos + 256 > (uint8_t *)mapbase + st.st_size)
	{
	  fprintf(stderr, "EOF reached for MEM file\n");
	  break;
	}

      clarion_decrypt(pos + 4, 252);

      pos += 256;
    }

  munmap(mapbase, st.st_size);
  close(fd);

  return 0;
}

void
clarion_decrypt_all(ClarionHandle *cl)
{
  struct stat st;
  ClarionHeader *clh;
  void *mapbase;
  int fd;
  int ret;

  clh = cl->clm.clh;

  fd = open(cl->datfile, O_RDWR);
  if (fd < 0)
    {
      fprintf(stderr, "Could not open %s: %s\n", cl->datfile, strerror(errno));

      exit(1);
    }

  ret = fstat(fd, &st);
  if (ret < 0)
    {
      fprintf(stderr, "fstat failed: %s\n", strerror(errno));

      close(fd);
      exit(1);
    }

  mapbase = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapbase == MAP_FAILED)
    {
      fprintf(stderr, "mmap failed: %s\n", strerror(errno));
      close(fd);

      exit(1);
    }

  /* Clear encrypted and owned flag */
  clh->sfatr &= ~(CL_FILE_OWNED | CL_RECORDS_ENCRYPTED);
  clh->sfatr = htole16(clh->sfatr);
  memcpy(mapbase + 2, &clh->sfatr, 2);

  clarion_get_key(clh, cl->decmode);

  ret = clarion_decrypt_header(cl, (uint8_t *)mapbase);
  if (ret < 0)
    goto cleanup;

  ret = clarion_decrypt_schema(cl, (uint8_t *)mapbase);
  if (ret < 0)
    goto cleanup;

  ret = clarion_decrypt_data(cl, (uint8_t *)mapbase, st.st_size);
  if (ret < 0)
    goto cleanup;

  if (cl->clm.clh->sfatr & CL_MEMO_FILE_EXISTS)
    {
      ret = clarion_open_memo(cl);
      if (ret < 0)
	goto cleanup;

      ret = clarion_decrypt_memo(cl);
      if (ret < 0)
	goto cleanup;
    }

  ret = 0;

 cleanup:
  munmap(mapbase, st.st_size);
  close(fd);

  if (cl->memo)
    fclose(cl->memo);
  if (cl->data)
    fclose(cl->data);

  if (ret == 0)
    {
      clarion_free_handle(cl);
      exit(0);
    }
  else
    {
      if (cl->clm.clk)
	clarion_free_handle(cl);
      else
	{
	  if (cl->clm.clh)
	    free(cl->clm.clh);
	  if (cl->clm.clfd)
	    free(cl->clm.clfd);
	}

      exit(1);
    }
}
