
#include <zmodem.h>
#include <ihex.h>

__xdata uint8_t ihex_parse_error = IHEX_OK;

static __xdata union {
  struct {
    uint8_t len;
    uint8_t addr_high;
    uint8_t addr_low;
    uint8_t type;
  } values;
  uint8_t bytes[4];
} header;

static __xdata uint8_t *data_dest;
static __xdata uint8_t *data_end;
static __xdata uint8_t sum;
#define ROW_START 1
#define ROW_HEAD_HEX_HIGH 2
#define ROW_HEAD_HEX_LOW 3
#define ROW_DATA_HEX_HIGH 4 
#define ROW_DATA_HEX_LOW 5
#define ROW_CRC_HEX_HIGH 6 
#define ROW_CRC_HEX_LOW 7
#define ROW_DONE 10

static uint8_t row_state = ROW_START;
static uint8_t v;

void
ihex_init(void)
{
  ihex_parse_error = IHEX_OK;
  row_state = ROW_START;
}

uint8_t
zmodem_received(uint8_t c)
{
  switch(row_state) {
  case ROW_START:
    if (c == ':') {
      row_state = ROW_HEAD_HEX_HIGH;
      sum = 0;
      data_dest = header.bytes;
      data_end = data_dest + sizeof(header.bytes);
    }
    break;
  case ROW_HEAD_HEX_HIGH:
  case ROW_DATA_HEX_HIGH:
  case ROW_CRC_HEX_HIGH:
    if (c >= '0' && c <= '9') {
      v = c - '0';
    } else if (c >= 'A' && c <= 'F') {
      v = c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
      v = c - 'a' + 10;
    } else {
      ihex_parse_error = IHEX_ERROR_NOT_HEX_DIGIT;
      row_state = ROW_DONE;
      break;
    }
    v <<= 4;
    row_state++;
    break;
  case ROW_HEAD_HEX_LOW:
  case ROW_DATA_HEX_LOW:
  case ROW_CRC_HEX_LOW:
    if (c >= '0' && c <= '9') {
      v |= c - '0';
    } else if (c >= 'A' && c <= 'F') {
      v |= c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
      v |= c - 'a' + 10;
    } else {
      ihex_parse_error = IHEX_ERROR_NOT_HEX_DIGIT;
      row_state = ROW_DONE;
    }
#ifdef __SDCC
    *data_dest++ = v;
#else
    if (row_state == ROW_HEAD_HEX_LOW) {
      *data_dest++ = v;
    } else {
      data_dest++;
    }
#endif
    sum += v;
    if (data_dest == data_end) {
      switch(row_state) {
      case ROW_HEAD_HEX_LOW:
	if (header.values.type == 1) {
	  ihex_parse_error = IHEX_DONE;
	  row_state = ROW_DONE;
	  break;
	} else if (v != 0) {
	  ihex_parse_error = IHEX_ERROR_UNKNOWN_TYPE;
	  row_state = ROW_DONE;
	  break;
	}
	row_state = ROW_DATA_HEX_HIGH;
	data_dest = (__xdata uint8_t*)((header.values.addr_high<<8) 
					| header.values.addr_low);
	data_end = data_dest + header.values.len;
	break;
      case ROW_DATA_HEX_LOW:
	row_state = ROW_CRC_HEX_HIGH;
	data_dest = header.bytes;
	data_end = data_dest + 1;
	break;
      case ROW_CRC_HEX_LOW:
	if (sum == 0) {
	  row_state = ROW_START;
	  break;
	} else {
	  ihex_parse_error = IHEX_ERROR_CHECKSUM;
	}
      }
    } else {
      row_state--;
    }
  }
  if (ihex_parse_error == IHEX_DONE) return 0;
  return ihex_parse_error;
}
