#include <mcs51reg.h>
#include <stdint.h>
#include <ext_io.h>

#define SHIFT_BIT				\
  rrc a						\
  mov _P5_3,c					\
    setb _P5_6					\
    mov c,_P5_2					\
    clr _P5_6

static uint8_t
send_bits(uint8_t b) __naked
{
  (void)b;
  __asm
    mov a,dpl
    clr c
    SHIFT_BIT
    SHIFT_BIT
    SHIFT_BIT
    SHIFT_BIT
    SHIFT_BIT
    rrc a
    swap a
    rl a
    mov dpl,a
    ret
  __endasm;
}

static void
send_latch_in(void)
{
  EIO_CLOCK = 1;
  EIO_LOAD = 1;
  EIO_CLOCK = 0;
  EIO_LOAD = 0;
}

static void
send_latch_out(void)
{
  EIO_CLOCK = 0;
  EIO_LOAD = 1;
  EIO_CLOCK = 1;
  EIO_LOAD = 0;
}

void
ext_io_init(void)
{
  EIO_DATA_OUT = 0;
  EIO_INIT = 0;
  EIO_LOAD = 0;
  EIO_CLOCK = 0;
  EIO_RUN = 1;
}
void
ext_io_read_config(__xdata struct EIOConfig *conf)
{
  uint8_t n;
  EIO_RUN = 1;
  EIO_RUN = 0;
  EIO_INIT = 1;
  send_bits(0x10);
  for (n = 3; n < EIO_MAX_BUS_SLOTS; n++) {
    uint8_t d = send_bits(0x00);
    if (d == 0x10) break;
  }
  if (n == EIO_MAX_BUS_SLOTS) {
    conf->nslots = 0;
  } else {
    uint8_t l;
    conf->nslots = n;
    send_latch_in();
    conf->slots[0] = send_bits(0x15);
    conf->slots[1] = send_bits(0x08);
    for (l = 2; l < n; l++) {
       conf->slots[l] = send_bits(0x10);
    }
    EIO_INIT = 0;
  }
}
