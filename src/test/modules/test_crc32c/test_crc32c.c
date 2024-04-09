/* select drive_crc32c(1000000, 1024); */

#include "postgres.h"

#include "fmgr.h"

#include "port/pg_crc32c.h"

PG_MODULE_MAGIC;


/*
 * drive_crc32c(count int, num int) returns bigint
 *
 * num is the number of 64-bit words to use
 */
PG_FUNCTION_INFO_V1(drive_crc32c);
Datum
drive_crc32c(PG_FUNCTION_ARGS)
{
	int			count	= PG_GETARG_INT32(0);
	int			num		= PG_GETARG_INT32(1);
	pg_crc32c	crc;
	const char*	data	= malloc((size_t)num); 
	
	INIT_CRC32C(crc);

 	while(count--)
	{
        INIT_CRC32C(crc);
        COMP_CRC32C(crc, data, num);
		FIN_CRC32C(crc);
	}

	free((void *)data);

	PG_RETURN_INT64((int64_t)crc);
}
