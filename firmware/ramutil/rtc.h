#ifndef __RTC_H__57FIYOJ0N1__
#define __RTC_H__57FIYOJ0N1__

#include <stdint.h>

struct RTCTime
{
  uint8_t sec;
  uint8_t min;
  uint8_t hour;
  uint8_t day;
  uint8_t month;
  uint8_t year;
  uint8_t wday;
};

void
rtc_read(__xdata struct RTCTime *time);

void
rtc_init(void);

void
rtc_set(__xdata struct RTCTime *time);

#endif /* __RTC_H__57FIYOJ0N1__ */
