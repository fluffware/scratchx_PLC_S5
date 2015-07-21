#ifndef __EXT_IO_H__K7CNIE5OVD__
#define __EXT_IO_H__K7CNIE5OVD__

#define EIO_DATA_IN P5_2
#define EIO_DATA_OUT P5_3
#define EIO_INIT P5_4
#define EIO_LOAD P5_5
#define EIO_CLOCK P5_6
#define EIO_RUN P5_7

#define EIO_MAX_BUS_SLOTS 100

struct EIOConfig
{
  uint8_t nslots;
  uint8_t slots[EIO_MAX_BUS_SLOTS];
};

void
ext_io_init(void);

extern void
ext_io_read_config(__xdata struct EIOConfig *conf);
  
#endif /* __EXT_IO_H__K7CNIE5OVD__ */
