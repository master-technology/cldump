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
 * $Id: cldump.h 70 2010-11-27 10:20:18Z julien $
 */

#ifndef __CLDUMP_H__
#define __CLDUMP_H__

#define CL_VERSION               "0.12"

/* File signatures */
#define CL_DATA_FILE_SIG         0x3343
#define CL_MEMO_FILE_SIG         0x334D

/* ClarionHeader.sfatr values (bitmask) */
#define CL_FILE_LOCKED           (1 << 0)
#define CL_FILE_OWNED            (1 << 1)
#define CL_RECORDS_ENCRYPTED     (1 << 2)
#define CL_MEMO_FILE_EXISTS      (1 << 3)
#define CL_FILE_COMPRESSED       (1 << 4)
#define CL_RECLAIM_DELETED       (1 << 5)
#define CL_READ_ONLY             (1 << 6)
#define CL_MAY_BE_CREATED        (1 << 7)

/* Field types */
#define CL_FIELD_LONG            1
#define CL_FIELD_REAL            2
#define CL_FIELD_STRING          3
#define CL_FIELD_STRING_PIC_TOK  4
#define CL_FIELD_BYTE            5
#define CL_FIELD_SHORT           6
#define CL_FIELD_GROUP           7
#define CL_FIELD_DECIMAL         8

/* Record types values (bitmask) */
#define CL_RECORD_NEW            (1 << 0)
#define CL_RECORD_OLD            (1 << 1)
#define CL_RECORD_REVISED        (1 << 2)
#define CL_RECORD_DELETED        (1 << 4)
#define CL_RECORD_HELD           (1 << 6)

/* Key types values */
#define CL_KEYTYPE_ERROR         0xff /* not part of the Clarion spec */
/* The following apply to the LSB */
#define CL_KEYTYPE_KEY           0
#define CL_KEYTYPE_INDEX         1
/* The following apply to the MSB (bitmask) */
#define CL_KEYTYPE_DUPSW         (1 << 0) /* duplicates */
#define CL_KEYTYPE_UPRSW         (1 << 1) /* uppercase */
#define CL_KEYTYPE_OPTSW         (1 << 2) /* padding (with spaces) */
#define CL_KEYTYPE_LOKSW         (1 << 3) /* locked */

/* Command-line options */
#define CL_OPT_DUMP_ACTIVE       (1 << 0) /* dump active records only */
#define CL_OPT_DUMP_DATA         (1 << 1) /* dump actual data (all records, regardless of their status) */
#define CL_OPT_DUMP_META         (1 << 2) /* dump meta information only */
#define CL_OPT_CSV_OUTPUT        (1 << 3) /* output data in CSV format */
#define CL_OPT_SQL_OUTPUT        (1 << 4) /* output data in SQL format */
#define CL_OPT_SCHEMA            (1 << 5) /* output schema; combined with previous option, output SQL schema */
#define CL_OPT_NO_MEMO           (1 << 6) /* do not output memo entries */
#define CL_OPT_UTF8              (1 << 7) /* convert strings to UTF-8 */
#define CL_OPT_DECRYPT           (1 << 8)
#define CL_OPT_DEFAULT           (CL_OPT_DUMP_DATA | CL_OPT_DUMP_META | CL_OPT_SCHEMA) /* default: dump everything */

/* Encryption key location */
#define CL_KEY_NUMDELS_HI        1
#define CL_KEY_RESERVED_HI       2
#define CL_KEY_RESERVED_LO       3
#define CL_KEY_RESERVED_MID      4


typedef struct {
  uint16_t filesig;
  uint16_t sfatr;
  uint8_t numbkeys;
  uint32_t numrecs;
  uint32_t numdels;
  uint16_t numflds;
  uint16_t numpics;
  uint16_t numarrs;
  uint16_t reclen;
  uint32_t offset;
  uint32_t logeof;
  uint32_t logbof;
  uint32_t freerec;
  uint8_t recname[12+1];
  uint8_t memnam[12+1];
  uint8_t filpre[3+1];
  uint8_t recpre[3+1];
  uint16_t memolen;
  uint16_t memowid;
  uint32_t reserved;
  uint32_t chgtime;
  uint32_t chgdate;
  uint16_t reserved2;
} ClarionHeader;

typedef struct {
  uint16_t piclen;
  uint8_t *picstr;
} ClarionPicDesc;

typedef struct {
  uint16_t maxdim;
  uint16_t lendim;
} ClarionArrPart;

typedef struct {
  uint16_t numdim;
  uint16_t totdim;
  uint16_t elmsiz;
  ClarionArrPart *part;
} ClarionArrDesc;

typedef struct {
  uint8_t fldtype;
  uint8_t fldname[16+1];
  uint16_t foffset;
  uint16_t length;
  uint8_t decsig;
  uint8_t decdec;
  uint16_t arrnum;
  uint16_t picnum;
  ClarionArrDesc *arr;
  int nbarrs; /* this is not part of the Clarion database format */
} ClarionFieldDesc;

typedef struct cl_key_part {
  uint8_t fldtype;
  uint16_t fldnum;
  uint16_t elmoff;
  uint8_t elmlen;
  uint16_t numparts; /* the next 2 fields are not part of the Clarion database format */
  struct cl_key_part *subpart;
} ClarionKeyPart;

typedef struct {
  uint8_t numcomps;
  uint8_t keyname[16+1];
  uint8_t comptype;
  uint8_t complen;
  ClarionKeyPart *keypart;
  uint8_t keytype; /* stored in the key file header */
} ClarionKeyDesc;

typedef struct {
  ClarionHeader *clh;
  ClarionFieldDesc *clfd;
  ClarionPicDesc *clp;
  ClarionKeyDesc *clk;
} ClarionMeta;

typedef struct {
  unsigned short opts;
  unsigned char decmode;
  unsigned char fsep;
  unsigned char sql_quote_begin;
  unsigned char sql_quote_end;
  ClarionMeta clm;
  char *datfile;
  FILE *data;
  char *memfile;
  FILE *memo;
  char *charset;
} ClarionHandle;

typedef struct {
  uint8_t rhd;
  uint32_t rptr;
} ClarionRecordHeader;

typedef struct {
  uint16_t memsig;
  uint32_t firstdel;
} ClarionMemoHeader;

typedef struct {
  uint32_t nxtblk;
  uint8_t memo[252+1];
} ClarionMemoEntry;


/* In cldump.c */
void
clarion_free_handle (ClarionHandle *cl);

int
clarion_open_memo (ClarionHandle *cl);


/* In cl_decrypt.c */
void
clarion_decrypt_all(ClarionHandle *cl);


/* In cl_utils.c */
#if BYTE_ORDER == BIG_ENDIAN
size_t cl_fread (void *ptr, size_t size, size_t nmemb, FILE *stream);
#define fread cl_fread
#endif

void
clarion_trim (uint8_t *data, int length);

void
clarion_singlespace (char *data);

char *
clarion_iconv (const char *charset, char *data);


/* In cl_meta.c */
int
clarion_read_header (ClarionHandle *cl);

int
clarion_read_field_desc(ClarionHandle *cl);

int
clarion_read_key_desc (ClarionHandle *cl);

int
clarion_read_pic_desc (ClarionHandle *cl);

void
clarion_read_arr_desc (ClarionHandle *cl);


/* In cl_dump_meta.c */
void
clarion_dump_meta (ClarionHandle *cl);

void
clarion_dump_schema (ClarionHandle *cl);


/* In cl_dump_meta_sql.c */
void
clarion_dump_schema_sql (ClarionHandle *cl);


/* In cl_dump_meta_csv.c */
void
clarion_dump_schema_csv (ClarionHandle *cl);


/* In cl_dump_field.c */
void
clarion_dump_memo_entry (ClarionRecordHeader *clrh, FILE *fp, char *plchold, char *charset);

void
clarion_dump_field_long (uint8_t *buf, ClarionFieldDesc *clfd, FILE *fp, char *plchold);

void
clarion_dump_field_real (uint8_t *buf, ClarionFieldDesc *clfd, FILE *fp, char *plchold);

void
clarion_dump_field_string (uint8_t *buf, ClarionFieldDesc *clfd, FILE *fp, char *plchold, char *charset);

void
clarion_dump_field_byte (uint8_t *buf, ClarionFieldDesc *clfd, FILE *fp, char *plchold);

void
clarion_dump_field_short (uint8_t *buf, ClarionFieldDesc *clfd, FILE *fp, char *plchold);

void
clarion_dump_field_decimal (uint8_t *buf, ClarionFieldDesc *clfd, FILE *fp, char *plchold);


/* In cl_dump_data.c */
void
clarion_dump_data (ClarionHandle *cl);


/* In cl_dump_data_csv.c */
void
clarion_dump_data_csv (ClarionHandle *cl);


/* In cl_dump_data_sql.c */
void
clarion_dump_data_sql (ClarionHandle *cl);


#endif /* !__CLDUMP_H__ */
