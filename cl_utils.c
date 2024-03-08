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
 * $Id: cl_utils.c 53 2006-09-04 12:38:59Z julien $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <byteswap.h>
#include <iconv.h>

#if BYTE_ORDER == BIG_ENDIAN
size_t cl_fread (void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t ret;
  size_t i;
  uint16_t *p = (uint16_t *) ptr;
  uint32_t *pp = (uint32_t *) ptr;
  uint64_t *ppp = (uint64_t *) ptr;

  ret = fread(ptr, size, nmemb, stream);

  switch (size)
    {
      case 2:
	for (i = 0; i < ret; i++)
	  p[i] = bswap_16(p[i]);
	break;
      case 4:
	for (i = 0; i < ret; i++)
	  pp[i] = bswap_32(pp[i]);
	break;
      case 8:
	for (i = 0; i < ret; i++)
	  ppp[i] = bswap_64(ppp[i]);
	break;
      default:
	break;
    }

  return ret;
}
#endif

/*
 * WARNING : cldump.h redefines fread() to cl_fread()
 */
#include "cldump.h"

void
clarion_trim (uint8_t *data, int length)
{
  int i;

  for (i = (length - 1); i >= 0; i--)
    {
      if (data[i] == 0x20)
	continue;
      else
	{
	  data[i + 1] = '\0';
	  return;
	}
    }
  data[0] = '\0';
}

void
clarion_singlespace (char *data)
{
  char *p = NULL;
  uint32_t offset;

  while (*data != '\0')
    {
      if (*data == 0x20)
	{
	  if (p == NULL)
	    p = data;
	}
      else
	{
	  if (p != NULL)
	    {
	      p++;
	      offset = data - p;
	      memmove(p, data, strlen(data)+1);

	      data = data - offset;

	      p = NULL;
	    }
	}

      data++;
    }
}

char *
clarion_iconv (const char *charset, char *data)
{
  iconv_t cd;
  char *buf, *tmp;
  size_t len;
  size_t buflen;
  size_t ret;

  len = strlen(data);

  if (len == 0)
    return NULL;

  cd = iconv_open("UTF-8", charset);

  if (cd == (iconv_t)(-1))
    return NULL;

  buflen = 2 * len;
  buf = (char *) malloc(buflen + 1);

  tmp = buf;
  ret = iconv(cd, &data, &len, &tmp, &buflen);

  if (ret == (size_t)(-1))
    {
      free(buf);
      return NULL;
    }

  *tmp = '\0';

  iconv_close(cd);

  return buf;
}
