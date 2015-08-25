#ifndef __CRC_H__3SHQIQVHNB__
#define __CRC_H__3SHQIQVHNB__

#include <stdint.h>
#ifndef __SDCC
#define __code
#define __xdata
#endif

extern __xdata const uint16_t crctab[256];
/*
 * updcrc macro derived from article Copyright (C) 1986 Stephen Satchell. 
 *  NOTE: First srgument must be in range 0 to 255.
 *        Second argument is referenced twice.
 * 
 * Programmers may incorporate any or all code into their programs, 
 * giving proper credit within the source. Publication of the 
 * source routines is permitted so long as proper credit is given 
 * to Stephen Satchell, Satchell Evaluations and Chuck Forsberg, 
 * Omen Technology.
 */

#define updcrc(cp, crc) ( crctab[((crc >> 8) & 255)] ^ (crc << 8) ^ cp)
 

#endif /* __CRC_H__3SHQIQVHNB__ */
