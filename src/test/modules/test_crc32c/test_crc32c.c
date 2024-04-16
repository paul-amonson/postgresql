/* select drive_crc32c(1000000, 1024); */

#include "postgres.h"

#include "fmgr.h"

#include "port/pg_crc32c.h"

PG_MODULE_MAGIC;

#define CRC_DEBUG 0
#if CRC_DEBUG == 1
#include <stdio.h>
void dump(int64 count, int64 len);
void dump(int64 count, int64 len)
{
	static const char *format = "drive_crc32c(%ld,  %ld)\n";
	FILE *fd = fopen("/home/paul/postgresql-fork/postgres.out", "a");
	fprintf(fd, format, count, len);
	fclose(fd);
}
#else
#define dump(x, y)
#endif

/*
 * drive_crc32c(count int, num int) returns bigint
 *
 * num is the number of 64-bit words to use
 */
PG_FUNCTION_INFO_V1(drive_crc32c);
Datum
drive_crc32c(PG_FUNCTION_ARGS)
{
	int64			count	= PG_GETARG_INT64(0);
	int64			num		= PG_GETARG_INT64(1);
	pg_crc32c	crc;
	const char*	data	= malloc((size_t)num);
	dump(count, num);
	INIT_CRC32C(crc);

	while(count--)
	{
        INIT_CRC32C(crc);
        pg_comp_crc32c_sse42(crc, data, num);
		FIN_CRC32C(crc);
	}

	free((void *)data);

	PG_RETURN_INT64((int64_t)crc);
}
