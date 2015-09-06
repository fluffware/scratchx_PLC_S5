#include <mcs51reg.h>
#include <stdint.h>
#include <ext_io.h>
#include <int_io.h>
#include <adc.h>
#include <dac.h>
#include <rtc.h>
#include <serial.h>
#include <stdio.h>
#include <sys/pt.h>
#include <string.h>
#include <crc8.h>
#include <plc_protocol.h>

static volatile __data uint16_t t0_count = 0;
union int_io_word {
  uint16_t u16;
  uint8_t u8[2];
};
static __xdata union int_io_word int_output_state = {0x0000};
static __xdata union int_io_word int_input_state = {0x0000};
static __xdata uint8_t sent_int_input_state[2] = {0,0};

static void
timer0_isr(void) __interrupt(1)
{
  if (++t0_count >= 150) t0_count = 0;
  int_io_write(int_output_state.u16);
  int_input_state.u16 = int_io_read();
}

static void
serial1_isr(void) __interrupt(16);


/* Binary protocol

Short parameters
0	Command/reply byte
1-n	Parameters
n+1	CRC8

Long parameters
0	Command/reply byte
1	Number of parameter bytes
2-n	Parameters
n+1	CRC8



Command/reply byte
Bits
7-6	Number of parameter bytes (3 means next byte is number of parameters)
5	1 if reply
4-0	Command code

Codes				Parameters
0x41	Read digital input	Address (1 byte)
0xa1	Digital input reply	Address (1 byte), Result (1 byte)
0xc2	Write digital output	3,Address (1 byte), Value (1 byte), Mask(1 byte)
0xa2	Digital output reply	Address (1 byte), Result (1 byte)
0x6f	Error reply
*/


static void
timer_init(void)
{
  TMOD = (TMOD & 0xf0) | 0x0; /* Mode 1 150Hz clock */
  TL0 = 0;
  TH0 =0;
  TF0 = 0;
  ET0 = 1;
  TR0 = 1;
}

static void
wd_reset(void)
{
  P3_4 = 0;
  P3_4 = 1;
}


static __xdata uint16_t int_aout0_state = 0;

#define MAX_COMMAND_LEN 8
static __xdata uint8_t command[MAX_COMMAND_LEN];
static uint8_t command_left = 255;
static uint8_t command_len = 0;
static uint8_t crc;

#define MAX_REPLY_LEN 8
static __xdata uint8_t reply[MAX_REPLY_LEN];

static void
send_reply(uint8_t reply_len)
{
  uint8_t i;
  uint8_t scrc = 0xff;
  for (i = 0; i < reply_len; i++) {
    putchar(reply[i]);
    scrc =crc8_update(scrc, reply[i]);
  }
  putchar(scrc);
}
				   
static void
send_error(uint8_t err)
{
  uint8_t scrc = 0xff;
  putchar(PLC_REPLY_ERROR);
  scrc =crc8_update(scrc, PLC_REPLY_ERROR);
  putchar(err);
  scrc =crc8_update(scrc, err);
  putchar(scrc);
}

static void
send_int_input(uint8_t addr)
{
  if (addr == 32) {
    reply[2] = int_input_state.u8[0];
    sent_int_input_state[0] = int_input_state.u8[0];
  } else if (addr == 33) {
    reply[2] = int_input_state.u8[1];
    sent_int_input_state[1] = int_input_state.u8[1];
  } else {
    reply[2] = 0;
  }
  reply[0] = PLC_REPLY_DIGITAL_INPUT;
  reply[1] = addr;
  send_reply(3);
}

#define PLC_CMD_TEST PLC_CMD_LEN0(0xe)

static void
parse_command(uint8_t c)
{
  if (command_len == 0) {
    crc = 0xff;
    if ((c & 0xc0) != 0xc0) {
      command_left = (c >> 6) + 2;
    }
  } else if (command_len == 1) {
    if ((command[0] & 0xc0) == 0xc0) {
      command_left = c + 2;
    }
  }
  if (command_len < MAX_COMMAND_LEN) {
    command[command_len++] = c;
    crc = crc8_update(crc, c);
  }
  command_left--;
  
  if (command_left == 0) {
    uint8_t scrc = 0xff;
    if (crc == 0) {
      switch(command[0]) {
      case PLC_CMD_READ_DIGITAL_INPUT:
	send_int_input(command[1]);
	break;
      case PLC_CMD_WRITE_DIGITAL_OUTPUT:
	if (command[1] != 3) break;
	if (command[2] == 32) {
	  int_output_state.u8[0] = ((int_output_state.u8[0] & ~command[4]) 
				    | command[3]); 
	  reply[2] = int_output_state.u8[0];
	} else if (command[2] == 33) {
	  int_output_state.u8[1] = ((int_output_state.u8[1] & ~command[4]) 
				    | command[3]); 
	  reply[2] = int_output_state.u8[1];
	} else {
	  reply[2] = 0;
	}
	reply[0] = PLC_REPLY_DIGITAL_OUTPUT;
	reply[1] = command[2];
	send_reply(3);
	break;
      case PLC_CMD_TEST:
	{
	}
	break;
      default:
	send_error(PLC_REPLY_ERROR_UNKNOWN_COMMAND);
	break;
      }
    } else {
      send_error(PLC_REPLY_ERROR_CRC);
    }
    command_len = 0;
    command_left = 255;
  }
}

#if 0
PT_THREAD(parse_thread(char c))
{
  static uint16_t addr;
  PT_BEGIN(&parse_pt);
  while(1) {
    if 
    while(c == '\n' || c=='\r') PT_YIELD(&parse_pt); 
    SKIP_WHITE;
    PT_SPAWN(&parse_pt, &sub_parse_pt, parse_token(c));
    if (strcmp(token, "din") == 0) {
      SKIP_WHITE;
      PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      SKIP_TO_EOL;
      if (v == 32) {
	print_hex8(int_io_read() & 0xff);
	putchar('\n');
      } else if (v == 33) {
	print_hex8( int_io_read() >> 8);
	putchar('\n');
      }
    } else if (strcmp(token, "dout") == 0) {
      static uint8_t dout;
      SKIP_WHITE;
      PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      addr = v;
      SKIP_WHITE;
      PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      dout = v;
      SKIP_WHITE;
      v = 0xff; /* Default mask */
      if (c != '\n' && c != '\r') {
	PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      }
      SKIP_TO_EOL;
      if (addr == 32 || addr == 33) {
	uint8_t r;
	if (addr == 32) {
	  int_output_state = (int_output_state & ~v) | (dout & v);
	} else {
	  int_output_state = (int_output_state & ~(v<<8)) | ((dout & v)<<8);
	}
	int_io_write(int_output_state);
	if (addr == 32) {
	  r = int_output_state;
	} else {
	  r = int_output_state>>8;
	}
	print_hex8(r);
	putchar('\n');
      }
    } else if (strcmp(token, "ain") == 0) {
      SKIP_WHITE;
      PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      SKIP_TO_EOL;
      if (v < 8) {
	int16_t r = adc_get(v);
	print_hex16(r);
	putchar('\n');
      }
    } else if (strcmp(token, "aout") == 0) {
      SKIP_WHITE;
      PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      addr = v;
      SKIP_WHITE;
      if (c == '\n' || c == '\r') {
	if (addr == 0) {
	  print_hex16(int_aout0_state);
	  putchar('\n');
	}
      } else {
	PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
	SKIP_TO_EOL;
	if (addr == 0) {
	  dac_set(v);
	  int_aout0_state = v;
	  print_hex16(v);
	  putchar('\n');
	}
      }
    } else if (strcmp(token, "echo") == 0) {
      SKIP_WHITE;
      PT_SPAWN(&parse_pt, &sub_parse_pt, parse_hex(c));
      SKIP_TO_EOL;
      print_hex16(v);
      putchar('\n');
    } else {
      SKIP_TO_EOL;
      putchar('?');
      putchar('\n');
    }
    PT_YIELD(&parse_pt);
  }
  PT_END(&parse_pt);
}

static void
parse_init(void)
{
  PT_INIT(&parse_pt);
}

static void
parse_input(char c)
{
  PT_SCHEDULE(parse_thread(c));
}
#endif

int 
main()
{
  int_io_init();
  timer_init(); 
  init_serial_1();
  EAL = 1;
  while(1) {
    wd_reset();
#if 0
    if (t0_count > 150/2) {
      P1_3 = 0;
    } else {
      P1_3 = 1;
    }
#endif
    {
      int16_t c = getbyte();
      if (c >= 0) {
	parse_command(c);
      }
    }
    if (int_input_state.u8[0] != sent_int_input_state[0]) {
      send_int_input(32);
    }
    if (int_input_state.u8[1] != sent_int_input_state[1]) {
      send_int_input(33);
    }
  }
}
