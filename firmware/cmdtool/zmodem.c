#include <sys/pt.h>
#include <zmodem.h>
#include <zmodem_defs.h>
#include <crc.h>

#ifndef __SDCC
#define __xdata
#define __sbit uint8_t
#else
#endif

#ifndef __SDCC
#define DEBUGGING 1
#else
#define DEBUGGING 0
#endif

#if DEBUGGING
#include <stdio.h>
#define DEBUG(args...) fprintf(stderr, args)
#else
#define DEBUG
#endif

#define ZHEX_ESC (ZHEX ^ 0x40)
#define ZBIN_ESC (ZBIN ^ 0x40)

static __sbit escaped = 0;
static struct pt zpackets_pt;

#undef NEXT_CH
#define NEXT_CH PT_YIELD(&zpackets_pt)

__xdata static uint8_t zrinit_header[] = {ZRINIT, 
					  (uint8_t)ZMODEM_RECEIVE_BUFFER_SIZE,
					  ZMODEM_RECEIVE_BUFFER_SIZE>>8,
					  0, 0};
__xdata static uint8_t zrpos_header[] = {ZRPOS, 0,0,0,0};
__xdata static uint8_t zferr_header[] = {ZFERR, 0,0,0,0};
__xdata static uint8_t zabort_header[] = {ZABORT, 0,0,0,0};
__xdata static uint8_t zack_header[] = {ZACK, 0,0,0,0};
__xdata static uint8_t zfin_header[] = {ZFIN, 0,0,0,0};

static void
send_hex(uint8_t v)
{
  static uint8_t hex_chars[] = 
    {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
  zmodem_send(hex_chars[v >> 4]);
  zmodem_send(hex_chars[v & 0x0f]);
}

static
void send_hex_header(const __xdata uint8_t *header)
{
  uint8_t i;
  uint16_t crc = 0;
  zmodem_send(ZPAD);
  zmodem_send(ZPAD);
  zmodem_send(ZDLE);
  zmodem_send(ZHEX);
  for (i = 0; i < 5; i++) {
    uint8_t d = header[i];
    send_hex(d);
    crc = updcrc(d, crc);
  }
  crc = updcrc(0, crc);
  crc = updcrc(0, crc);
  send_hex(crc>>8);
  send_hex(crc);
  zmodem_send('\r');
  zmodem_send(0x8a);
}

static void
send_pos(uint16_t pos)
{
  zrpos_header[1] = pos;
  zrpos_header[2] = pos>>8;
  send_hex_header(zrpos_header);
}

static void
send_zack(uint16_t pos)
{
  zack_header[1] = pos;
  zack_header[2] = pos>>8;
  send_hex_header(zack_header);
}

static void
send_abort(void)
{
  uint8_t i;
  for(i = 0; i < 8; i++) {
    zmodem_send(ZDLE);
  }
  for(i = 0; i < 8; i++) {
    zmodem_send(0x08);
  }
}

static uint16_t file_pos;

static uint8_t header[7];
PT_THREAD(zpackets(uint8_t c))
{
  static __xdata uint16_t crc;
  static uint8_t i;
  PT_BEGIN(&zpackets_pt);
  while(1) {
  decode_header:
    if (c != ZPAD) {
      NEXT_CH;
      goto decode_header;
    }
    NEXT_CH;
    if (!escaped) goto decode_header;
    if (c == ZHEX_ESC) {
      for (i = 0; i < 7; i++) {
	NEXT_CH;
	if (c >= '0' && c <= '9') {
	  header[i] = (c - '0') << 4;
	} else  if (c >= 'a' && c <= 'f') {
	header[i] = (c - 'a') << 4;
	} else goto decode_header;
	NEXT_CH;
	
	if (c >= '0' && c <= '9') {
	  header[i] |= c - '0';
	} else  if (c >= 'a' && c <= 'f') {
	header[i] |= c - 'a';
	} else goto decode_header;
      }
    } else if (c == ZBIN_ESC) {
      for (i = 0; i < 7; i++) {
	NEXT_CH;
	header[i] = c;
      }
  } else goto decode_header;
    DEBUG("Got header %02x\n",header[0]);
    if (header[0] == ZRQINIT) {
      send_hex_header(zrinit_header);
    } else if(header[0] == ZFILE) {
      crc = 0;
      NEXT_CH;
      while(!escaped || (c != ZCRCW)) { /* Skip file data */
	crc = updcrc(c, crc);
	NEXT_CH;
      }
      crc = updcrc(c, crc);
      NEXT_CH;
      crc = updcrc(c, crc);
      NEXT_CH;
      crc = updcrc(c, crc);
      DEBUG("ZFILE CRC %04x\n", crc);
      zmodem_file_start();
      file_pos = 0;
      send_pos(file_pos);
    } else if (header[0] == ZDATA) {
      static __xdata uint8_t end_type;
      if ((header[1]+(header[2]<<8)) != file_pos) {
	send_pos(file_pos);
	goto decode_header;
      }
    start_frame:
      crc = 0;
      NEXT_CH;
      while(!escaped || c < ZCRCE || c > ZCRCW) {
	crc = updcrc(c, crc);
	if (zmodem_received(c)) {
	  send_abort();
	  goto abort;
	}
	file_pos++;
	NEXT_CH;
      }
      end_type = c;
      crc = updcrc(c, crc);
      NEXT_CH;
      crc = updcrc(c, crc);
      NEXT_CH;
      crc = updcrc(c, crc);
      DEBUG("ZDATA CRC %04x %c\n", crc, end_type);
      if (crc == 0) {
	
	if (end_type == ZCRCE) {
	  /* Wait for next header */
	} else if (end_type == ZCRCG) {
	  goto start_frame;
	} else if (end_type == ZCRCQ) {
	  send_zack(file_pos);
	  goto start_frame;
	} else if (end_type == ZCRCW) {
	  send_zack(file_pos);
	}
      } else {
	send_abort();
	break;
      }
    } else if (header[0] == ZEOF) {
      DEBUG("ZEOF %04x %02x%02x\n",file_pos, header[2], header[1]);
      if ((header[1] == (uint8_t)file_pos)
	  && (header[2] == (uint8_t)(file_pos>>8))
	  && (header[3] == (uint8_t)0)
	  && (header[4] == (uint8_t)0)) {
	zmodem_file_end();
	send_hex_header(zrinit_header);
      }
    } else if (header[0] == ZFIN) {
      send_hex_header(zfin_header);
      NEXT_CH;
      while(c != 'O') { /* Find first 'O' */
	NEXT_CH;
      }
      NEXT_CH; /* Skip to second 'O' */
      break;
    }
    NEXT_CH;
  }
 abort:
  ;
  PT_END(&zpackets_pt);
}

static struct pt decode_pt;

#undef NEXT_CH
#define NEXT_CH PT_YIELD(&decode_pt)
static uint8_t header[7];
PT_THREAD(decode(uint8_t c))
{
  static __xdata uint8_t zdle_count;
  PT_BEGIN(&decode_pt);
  PT_INIT(&zpackets_pt);
  while(1) {
    if (c == ZDLE) {
      NEXT_CH;
      if ((c & 0x60) == 0x40) { /* Escaped control character */
	c ^= 0x40;
      } else if (c == 021 || c == 0221 || c== 023 || c == 0223) {
	NEXT_CH;
	continue;
      } else if (c == ZDLE) { /* Possible abort sequence */
	
	zdle_count = 0;
	do {
	  zdle_count++;
	  NEXT_CH;
	} while (c == ZDLE && zdle_count < 5); /* Look for 5 successive ZDLE */
	if (c == ZDLE) {
	  do {
	    NEXT_CH;
	  } while (c == ZDLE); /* Skip any more ZDLE */
	  break; /* Abort */
	}
	continue;
      }
      escaped = 1;
    } else {
      escaped = 0;
    }
#if DEBUGGING
    /*
    if (escaped) {
      DEBUG("Decoded: \\%02x\n",c);
    } else {
      DEBUG("Decoded: %02x\n",c);
      }*/
#endif
    if (!PT_SCHEDULE(zpackets(c))) break;
    NEXT_CH;
  }
  DEBUG("Done\n");
  ;
  PT_END(&decode_pt);
}

void
zmodem_init(void)
{
  PT_INIT(&decode_pt);
}

uint8_t
zmodem_input(uint8_t c)
{
  return PT_SCHEDULE(decode(c));
}
