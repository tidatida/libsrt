/*
 * shash.c
 *
 * Buffer hashing
 *
 * Copyright (c) 2015-2016, F. Aragon. All rights reserved. Released under
 * the BSD 3-Clause License (see the doc/LICENSE file included).
 */

#include "shash.h"

/*
 * Constants
 */

#define S_CRC32_POLY	0xedb88320

/*
 * CRC-32 implementations
 */

#ifndef S_BUILD_CRC32_TABLES

#if defined(S_MINIMAL) || (defined(S_CRC32_SLC) && S_CRC32_SLC == 0)

/*
 * Compact implementation, without hash tables (one bit per loop)
 */
uint32_t sh_crc32(uint32_t crc, const void *buf, size_t buf_size)
{
	const uint8_t *p = (const uint8_t *)buf;
	size_t i, j;
	crc = ~crc;
	for (i = 0; i < buf_size; i++) {
		crc ^= p[i];
		for (j = 0; j < 8; j++) {
			int b0 = crc & 1;
			crc >>= 1;
			if (b0)
				crc ^= S_CRC32_POLY;
		}
	}
	return ~crc;
}

#else

#include "scrc32.h"

/*
 * 1, 4, 8, 12, and 16 bytes per loop using 1024 to 16384 bytes table
 */
uint32_t sh_crc32(uint32_t crc, const void *buf, size_t buf_size)
{
	size_t i = 0;
	const uint8_t *p = (const uint8_t *)buf;
	crc = ~crc;
#if S_CRC32_SLC == 4 || S_CRC32_SLC == 8 || S_CRC32_SLC == 12 || \
    S_CRC32_SLC == 16
	size_t bsX = (buf_size / S_CRC32_SLC) * S_CRC32_SLC;
	for (; i < bsX; i += S_CRC32_SLC) {
		uint32_t a = S_LD_U32(p + i) ^ S_LTOH_U32(crc)
	#if S_CRC32_SLC >= 8
		       , b = S_LD_U32(p + i + 4)
	#endif
	#if S_CRC32_SLC >= 12
		       , c = S_LD_U32(p + i + 8)
	#endif
	#if S_CRC32_SLC == 16
		       , d = S_LD_U32(p + i + 12)
	#endif
			;
	#if S_CRC32_SLC == 4
		#define SC32A a
	#elif S_CRC32_SLC == 8
		#define SC32A b
		#define SC32B a
	#elif S_CRC32_SLC == 12
		#define SC32A c
		#define SC32B b
		#define SC32C a
	#else
		#define SC32A d
		#define SC32B c
		#define SC32C b
		#define SC32D a
	#endif
		crc = crc32_tab[0][S_U32_BYTE3(SC32A)] ^
		      crc32_tab[1][S_U32_BYTE2(SC32A)] ^
		      crc32_tab[2][S_U32_BYTE1(SC32A)] ^
		      crc32_tab[3][S_U32_BYTE0(SC32A)]
        #if S_CRC32_SLC >= 8
		    ^ crc32_tab[4][S_U32_BYTE3(SC32B)] ^
		      crc32_tab[5][S_U32_BYTE2(SC32B)] ^
		      crc32_tab[6][S_U32_BYTE1(SC32B)] ^
		      crc32_tab[7][S_U32_BYTE0(SC32B)]
	#endif
	#if S_CRC32_SLC >= 12
		    ^ crc32_tab[8][S_U32_BYTE3(SC32C)] ^
		      crc32_tab[9][S_U32_BYTE2(SC32C)] ^
		      crc32_tab[10][S_U32_BYTE1(SC32C)] ^
		      crc32_tab[11][S_U32_BYTE0(SC32C)]
	#endif
        #if S_CRC32_SLC == 16
		    ^ crc32_tab[12][S_U32_BYTE3(SC32D)] ^
		      crc32_tab[13][S_U32_BYTE2(SC32D)] ^
		      crc32_tab[14][S_U32_BYTE1(SC32D)] ^
		      crc32_tab[15][S_U32_BYTE0(SC32D)]
	#endif
			;
	}
#endif
	for (; i < buf_size; i++)
		crc = crc32_tab[0][(crc ^ p[i]) & 0xff] ^ (crc >> 8);
	return ~crc;
}

#endif /* #ifdef S_MINIMAL */

#else

/*
 * gcc shash.c -DS_BUILD_CRC32_TABLES ; ./a.out >scrc32.h
 */
int main(int argc, const char **argv)
{
	/*
	 * Generation of scrc32.h content
	 *
	 * Learned from:
	 * https://web.archive.org/web/20121011093914/http://www.intel.com
	 *	          /technology/comms/perfnet/download/CRC_generators.pdf
	 * http://create.stephan-brumme.com/crc32/
	 */
	size_t i, j;
	uint32_t c32t[16][256];
	for (i = 0; i < 256; i++) {
		uint32_t crc = i;
		for (j = 0; j < 8; j++) {
			int b0 = crc & 1;
			crc >>= 1;
			if (b0)
				crc ^= S_CRC32_POLY;
		}
		c32t[0][i] = crc;
	}
	for (j = 1; j < 16; j++) {
		for (i = 0; i < 256; i++) {
			#define C32L(s) \
				c32t[s][i] = (c32t[s - 1][i] >> 8) ^ \
				c32t[0][c32t[s - 1][i] & 0xff];
			C32L(1); C32L(2); C32L(3); C32L(4); C32L(5); C32L(6);
			C32L(7); C32L(8); C32L(9); C32L(10); C32L(11); C32L(12);
			C32L(13); C32L(14); C32L(15);
		}
	}
	/*
	 * Flush table to the output
	 */
	printf("/*\n"
	       " * scrc32.h\n"
	       " *\n"
	       " * Precomputed CRC-32 for polynomial 0x%08x\n"
	       " * (gcc shash.c -DS_BUILD_CRC32_TABLES ; ./a.out >scrc32.h)\n"
	       " *\n"
	       " * Copyright (c) 2015-2016, F. Aragon. All rights reserved. "
							"Released under\n"
	       " * the BSD 3-Clause License (see the doc/LICENSE file "
							"included).\n"
	       " */\n\n"
	       "#ifndef SCRC32_H\n"
	       "#define SCRC32_H\n\n"
	       "#ifndef S_CRC32_SLC\n"
	       "#define S_CRC32_SLC 12\n"
	       "#else\n"
	       "#if S_CRC32_SLC != 0 && S_CRC32_SLC != 1 && "
						"S_CRC32_SLC != 4 && \\\n"
	       "    S_CRC32_SLC != 8 && S_CRC32_SLC != 12 && "
						"S_CRC32_SLC != 16\n"
	       "#undef S_CRC32_SLC /* if invalid slice size, default to 1 */\n"
	       "#define S_CRC32_SLC 1\n"
	       "#endif\n"
	       "#endif\n\n"
	       "#if S_CRC32_SLC > 0\n\n"
	       "static const uint32_t crc32_tab[S_CRC32_SLC][256] = {\n",
	       S_CRC32_POLY);
	int rows = 6;
	for (i = 0; i < 16; i++) {
		printf("\t{\n\t");
		for (j = 0; j < 256; j++) {
			printf("0x%08x", c32t[i][j]);
			if (j < 255) {
				if ((j % rows) == (rows - 1))
					printf(",\n\t");
				else
					printf(", ");
			}
		}
		printf("\n\t}");
		switch (i) {
		case 0: printf("\n#if S_CRC32_SLC >= 4\n\t,"); break;
		case 3: printf("\n#endif /*#if S_CRC32_SLC >= 4*/");
			printf("\n#if S_CRC32_SLC >= 8\n\t,"); break;
		case 7: printf("\n#endif /*#if S_CRC32_SLC >= 8*/");
			printf("\n#if S_CRC32_SLC >= 12\n\t,"); break;
		case 11: printf("\n#endif /*#if S_CRC32_SLC >= 12*/");
			 printf("\n#if S_CRC32_SLC >= 16\n\t,"); break;
		case 15: printf("\n#endif /*#if S_CRC32_SLC >= 16*/\n"); break;
		default: printf(",");
		}
		printf("\n");
	}
	printf("};\n\n"
	       "#endif /*#if S_CRC32_SLC > 0*/\n\n"
	       "#endif /* #ifdef SCRC32_H */\n\n");
}

#endif /* #ifndef S_BUILD_CRC32_TABLES */

