#
# $Id: Makefile 66 2010-11-27 10:20:11Z julien $
#

CFLAGS = -Wall -g -O2 -fPIE -fstack-protector-strong -Wformat -Werror=format-security
LDFLAGS = -fPIE -pie -Wl,-z,relro -Wl,-z,now
OBJS = cldump.o cl_utils.o \
	cl_meta.o \
	cl_dump_meta.o cl_dump_meta_csv.o cl_dump_meta_sql.o \
	cl_dump_data.o cl_dump_data_csv.o cl_dump_data_sql.o \
	cl_dump_field.o cl_decrypt.o

all: cldump

cldump: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o cldump $(OBJS)

%.c %.o: %.c cldump.h

clean:
	rm -f $(OBJS) cldump *~

