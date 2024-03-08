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
 * $Id: cldump.c 66 2010-11-27 10:20:11Z julien $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <endian.h>
#include <byteswap.h>
#include <errno.h>
#include <getopt.h>

#include "cldump.h"


int
clarion_open_memo (ClarionHandle *cl)
{
  FILE *fp;
  ClarionMemoHeader clmh;

  if (!(cl->clm.clh->sfatr & CL_MEMO_FILE_EXISTS))
    return 0;

  cl->memfile = strdup(cl->datfile);
  cl->memfile[strlen(cl->memfile) - 1] = 'M';
  cl->memfile[strlen(cl->memfile) - 2] = 'E';
  cl->memfile[strlen(cl->memfile) - 3] = 'M';

  fp = fopen(cl->memfile, "rb");

  if ((fp == NULL) && (cl->clm.clh->sfatr & CL_MEMO_FILE_EXISTS))
    {
      fprintf(stderr, "Error opening memo file (%s): %s\n", cl->memfile, strerror(errno));
      free(cl->memfile);
      cl->memfile = NULL;
      return -1;
    }

  fread(&clmh.memsig, 2, 1, fp);
  fread(&clmh.firstdel, 4, 1, fp);

  if (clmh.memsig != CL_MEMO_FILE_SIG)
    {
      fclose(fp);
      fprintf(stderr, "Invalid memo file (%s) !", cl->memfile);
      free(cl->memfile);
      cl->memfile = NULL;
      return -1;
    }

  if (cl->opts & CL_OPT_DUMP_META)
    {
      fprintf(stderr, "===== MEMO FILE HEADER =====\n\n");

      fprintf(stderr, "memsig   : 0x%04x\n", clmh.memsig);
      fprintf(stderr, "firstdel : 0x%08x\n", clmh.firstdel);

      fprintf(stderr, "===== END OF MEMO FILE HEADER\n\n");
    }

  fflush(stderr);

  cl->memo = fp;

  return 0;
}


void
clarion_free_handle (ClarionHandle *cl)
{
  int i, j;
  ClarionPicDesc *clp = cl->clm.clp;
  ClarionKeyDesc *clk = cl->clm.clk;
  ClarionFieldDesc *clfd = cl->clm.clfd;

  for (i = 0; i < cl->clm.clh->numbkeys; i++)
    {
      for (j = 0; j < clk[i].numcomps; j++)
	{
	  if (clk[i].keypart[j].subpart != NULL)
	    free(clk[i].keypart[j].subpart);
	}

      free(clk[i].keypart);
    }
  free(clk);

  if (cl->clm.clh->numpics > 0)
    {
      for (i = 0; i < cl->clm.clh->numpics; i++)
	{
	  if (clp[i].picstr)
	    free(clp[i].picstr);
	}
      free(clp);
    }

  for (i = 0; i < cl->clm.clh->numflds; i++)
    {
      if (clfd[i].arr != NULL)
	{
	  for (j = 0; j < clfd[i].nbarrs; j++)
	    {
	      free(clfd[i].arr[j].part);
	    }
	  free(clfd[i].arr);
	}
    }
  free(clfd);

  free(cl->clm.clh);

  free(cl->datfile);

  if (cl->memfile != NULL)
    free(cl->memfile);

  if (cl->charset != NULL)
    free(cl->charset);
}


void
cl_usage(void)
{
  fprintf(stdout, "Usage:\n");
  fprintf(stdout, "  cldump [options] database.dat\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "Options:\n");
  fprintf(stdout, "   -d/--dump-active        Dump active entries only\n");
  fprintf(stdout, "*  -D/--dump-data          Dump the actual data (active+deleted)\n");
  fprintf(stdout, "*  -m/--dump-meta          Dump meta information\n");
  fprintf(stdout, "   -f/--field-separator    Field separator for CSV output (1 character, defaults to ';')\n");
  fprintf(stdout, "   -c/--csv                Dump data or schema in CSV format\n");
  fprintf(stdout, "   -S/--sql                Dump data or schema in SQL format\n");
  fprintf(stdout, "*  -s/--schema             Dump database schema\n");
  fprintf(stdout, "   -M/--mysql              Use MySQL specific options (backticks, ...)\n");
  fprintf(stdout, "   -n/--no-memo            Do not dump memo entries\n");
  fprintf(stdout, "   -U[charset]            Convert strings from charset to UTF-8\n");
  fprintf(stdout, "     --utf8[=charset]        Default charset: iso8859-1\n");
  fprintf(stdout, "   -x/--decrypt            Decrypt database, key location 1-4\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "By default, cldump uses a human-friendly format to dump the database.\n");
  fprintf(stdout, "Options marked with a * are the default.\n");
}


void
cl_version(void)
{
  fprintf(stdout, "cldump v%s - Clarion database extractor\n", CL_VERSION);
  fprintf(stdout, "Copyright (C) 2004-2006,2010 Julien BLACHE <jb@jblache.org>\n");
  fprintf(stdout, "This software is released under the terms of the GNU General Public License\n");
  fprintf(stdout, "version 2, as published by the Free Software Foundation.\n");
}


int
main (int argc, char **argv)
{
  ClarionHandle cl;
  int cloptind;
  int clopt;
  int ret;

  static struct option clargs[] = {
    {"dump-active", 0, NULL, 'd'},
    {"dump-data", 0, NULL, 'D'},
    {"dump-meta", 0, NULL, 'm'},
    {"field-separator", 1, NULL, 'f'},
    {"csv", 0, NULL, 'c'},
    {"sql", 0, NULL, 'S'},
    {"schema", 0, NULL, 's'},
    {"mysql", 0, NULL, 'M'},
    {"no-memo", 0, NULL, 'n'},
    {"utf8", 2, NULL, 'U'},
    {"decrypt", 1, NULL, 'x'},
    {"help", 0, NULL, 'h'},
    {"version", 0, NULL, 'v'},
    {NULL, 0, NULL, 0}
  };

  memset(&cl, 0, sizeof(ClarionHandle));
  /* Default CSV field separator */
  cl.fsep = ';';
  /* Default SQL quote characters */
  cl.sql_quote_begin = '"';
  cl.sql_quote_end = '"';

  while ((clopt = getopt_long(argc, argv, "dDmf:cSsMnU::x:hv", clargs, &cloptind)) != -1)
    {
      switch (clopt)
	{
	  case 'd':
	    cl.opts |= CL_OPT_DUMP_ACTIVE;
	    break;
	  case 'D':
	    cl.opts |= CL_OPT_DUMP_DATA;
	    break;
	  case 'm':
	    cl.opts |= CL_OPT_DUMP_META;
	    break;
	  case 'f':
	    if (strlen(optarg) > 1)
	      {
		fprintf(stderr, "cldump: Error: only one character allowed as field separator.\n");
		exit(1);
	      }

	    cl.fsep = *optarg;
	    break;
	  case 'c':
	    if (cl.opts & CL_OPT_SQL_OUTPUT)
	      {
		fprintf(stderr, "cldump: Error: SQL output (-S/--sql) already specified.\n");
		exit(1);
	      }

	    cl.opts |= CL_OPT_CSV_OUTPUT;
	    break;
	  case 'S':
	    if (cl.opts & CL_OPT_CSV_OUTPUT)
	      {
		fprintf(stderr, "cldump: Error: CSV output (-c/--csv) already specified.\n");
		exit(1);
	      }

	    cl.opts |= CL_OPT_SQL_OUTPUT;
	    break;
	  case 's':
	    cl.opts |= CL_OPT_SCHEMA;
	    break;
	  case 'M':
	    cl.sql_quote_end = '`';
	    cl.sql_quote_begin = '`';
	    break;
	  case 'n':
	    cl.opts |= CL_OPT_NO_MEMO;
	    break;
	  case 'U':
	    cl.opts |= CL_OPT_UTF8;

	    if (optarg != NULL)
	      cl.charset = strdup(optarg);
	    else
	      cl.charset = strdup("ISO8859-1");
	    break;
	  case 'x':
	    cl.decmode = atoi(optarg);

	    if ((cl.decmode < 1) || (cl.decmode > 4))
	      {
		fprintf(stderr, "cldump: Error: key location argument is 1-4.\n");
		exit(1);
	      }

	    cl.opts |= CL_OPT_DECRYPT;
	    break;
	  case 'h':
	    cl_version();
	    fprintf(stdout, "\n");
	    cl_usage();
	    exit(0);
	    break;
	  case 'v':
	    cl_version();
	    exit(0);
	    break;
	  case ':':
	    cl_version();
	    fprintf(stderr, "\ncldump: Error: missing parameter for option %s.\n\n", argv[optind]);
	    cl_usage();
	    exit(2);
	    break;
	  default:
	    cl_version();
	    fprintf(stderr, "\ncldump: Error: unrecognised option (%s), aborting.\n\n", argv[optind - 1]);
	    cl_usage();
	    exit(2);
	    break;
	}
    }

  /* No options specified on the command line */
  if (cl.opts == 0)
    cl.opts = CL_OPT_DEFAULT;

  /* Force data dump if only output modifiers have been specified */
  if (!(cl.opts & CL_OPT_DUMP_ACTIVE) && !(cl.opts & CL_OPT_DUMP_DATA))
    {
      if ((cl.opts & CL_OPT_NO_MEMO) || (cl.opts & CL_OPT_CSV_OUTPUT) ||
	  (cl.opts & CL_OPT_SQL_OUTPUT))
	{
	  if (!(cl.opts & CL_OPT_DUMP_META) && !(cl.opts & CL_OPT_SCHEMA))
	    cl.opts |= CL_OPT_DUMP_DATA;
	}
    }

  if (optind >= argc)
    {
      cl_version();
      fprintf(stderr, "\ncldump: Error: no filename specified.\n\n");
      cl_usage();
      exit(3);
    }

  cl.data = fopen(argv[optind], "rb");

  if (cl.data == NULL)
    {
      fprintf(stderr, "Couldn't open file %s !\n", argv[optind]);
      exit(1);
    }

  cl.datfile = strdup(argv[optind]);

  ret = clarion_read_header(&cl);

  if (ret != 0)
    {
      fclose (cl.data);
      fprintf(stderr, "Couldn't read header !\n");
      exit(2);
    }

  if (cl.clm.clh->sfatr & CL_RECORDS_ENCRYPTED)
    {
      if (cl.opts & CL_OPT_DECRYPT)
	{
	  clarion_decrypt_all(&cl);

	  /* NOT REACHED */
	  exit(42);
	}
      else
	{
	  fprintf(stderr, "Database is encrypted, make backups and re-run with -x\n");
	  exit(1);
	}
    }
  else if (cl.opts & CL_OPT_DECRYPT)
    {
      fprintf(stderr, "Database is not encrypted; re-run without -x\n");
      exit(0);
    }

  if (!(cl.opts & CL_OPT_NO_MEMO))
    {
      ret = clarion_open_memo(&cl);

      if (ret != 0)
	{
	  free(cl.clm.clh);
	  fclose(cl.data);
	  fprintf(stderr, "Couldn't open memo file !\n");
	  exit(3);
	}
    }

  ret = clarion_read_field_desc(&cl);

  if (ret != 0)
    {
      free(cl.clm.clh);
      fclose(cl.data);
      if (cl.memo != NULL)
	fclose(cl.memo);
      fprintf(stderr, "Couldn't read field descriptors !\n");
      exit(4);
    }

  ret = clarion_read_key_desc(&cl);

  if (ret != 0)
    {
      free(cl.clm.clh);
      free(cl.clm.clfd);
      fclose(cl.data);
      if (cl.memo != NULL)
	fclose(cl.memo);
      fprintf(stderr, "Couldn't read key descriptors !\n");
      exit(5);
    }

  ret = clarion_read_pic_desc(&cl);
  if (ret != 0)
    {
      fclose(cl.data);
      if (cl.memo != NULL)
	fclose(cl.memo);
      clarion_free_handle(&cl);
      fprintf(stderr, "Couldn't read picture descriptors !\n");
      exit(6);
    }

  clarion_read_arr_desc(&cl);

  if (cl.opts & CL_OPT_DUMP_META)
    {
      clarion_dump_meta(&cl);
    }

  if (cl.opts & CL_OPT_SCHEMA)
    {
      if (cl.opts & CL_OPT_CSV_OUTPUT)
	clarion_dump_schema_csv(&cl);
      else if (cl.opts & CL_OPT_SQL_OUTPUT)
	clarion_dump_schema_sql(&cl);
      else
	clarion_dump_schema(&cl);
    }

  if ((cl.opts & CL_OPT_DUMP_DATA) || (cl.opts & CL_OPT_DUMP_ACTIVE))
    {
      if (cl.opts & CL_OPT_CSV_OUTPUT)
	clarion_dump_data_csv(&cl);
      else if (cl.opts & CL_OPT_SQL_OUTPUT)
      	clarion_dump_data_sql(&cl);
      else
	clarion_dump_data(&cl);
    }

  fclose(cl.data);

  if (cl.memo != NULL)
    fclose(cl.memo);

  clarion_free_handle(&cl);

  return 0;
}
