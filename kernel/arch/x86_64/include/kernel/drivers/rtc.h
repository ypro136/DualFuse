#include <types.h>

#ifndef RTC_H
#define RTC_H

typedef struct 
{
  unsigned char second;
  unsigned char minute;
  unsigned char hour;
  unsigned char day;
  unsigned char month;
  unsigned int  year;
} RTC;

int      read_from_CMOS(RTC *rtc);
uint64_t rtc_to_unix(RTC *rtc);
bool     is_leap_year(int year);

#endif
