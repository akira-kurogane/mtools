#include "loglines_logevents.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//following two defines are for strptime
#define _XOPEN_SOURCE
#define __USE_XOPEN
#include <time.h>

const char iso_dt_fmt[] = "%Y-%m-%dT%H:%M:%S";
#define iso_fmt_len 19

void destroy_logevent(struct logevent* le) { 
  free(le->thread_name);
  free(le->first_tokens);
}

//from https://gmbabar.wordpress.com/2010/12/01/mktime-slow-use-custom-function/
//Seems to be much faster than timegm(). mktime() was a lot slower again.
time_t time_to_epoch (const struct tm *ltm, int utcdiff_hrs, int utcdiff_mins) {
   static const int mon_days [] =
      {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
   long tyears, tdays, leaps;
   int i;

   tyears = ltm->tm_year - 70; // tm->tm_year is from 1900.
   leaps = (tyears + 2) / 4; // no of next two lines until year 2100.
   //i = (ltm->tm_year - 100) / 100;
   //leaps -= ( (i/4)*3 + i%4 );
   tdays = 0;
   for (i = 0; i < ltm->tm_mon; i++) tdays += mon_days[i];

   tdays += ltm->tm_mday-1; // days of month passed.
   tdays = tdays + (tyears * 365) + leaps;

   return (tdays * 86400) + ((ltm->tm_hour + utcdiff_hrs) * 3600) + ((ltm->tm_min + utcdiff_mins) * 60) + ltm->tm_sec;
}

int fill_le_from_line_str(struct logevent* le_ptr, char* line_str, size_t strl, int modes, char** err_str_ptr) {
  char* strptime_p;
  struct tm tm;
  /* Section to filter by timedate range
  if (do_filter && (strncmp(common_ts_prefix, line_str, common_ts_prefix_len) != 0 ||
      strncmp(min_ts_str, line_str, iso_fmt_len) > 0 || strncmp(max_ts_str, line_str, iso_fmt_len) < 0) {
    continue;
  }
  */

  if (strl > 19 && line_str[10] == 'T' && line_str[19] == '.') { //assume ISO datetime
    //fprintf(stdout, "%s", line_str);

    memset(&tm, 0, sizeof(struct tm));
    strptime_p = strptime(line_str, iso_dt_fmt, &tm);
    if (strptime_p != line_str + iso_fmt_len) {
      *err_str_ptr = (char*)calloc(100 + strl, sizeof(char));
      sprintf(*err_str_ptr, "bad format line (not ISO date string): %s", line_str);
      return 0;
    }

    char *p = line_str + 20; //the end of the "YYYY-mm-ddTHH:MM:SS" string, where the decimal for the milliseconds is expected to be
    le_ptr->ts.tv_usec = 1000 * strtol(p, &p, 10);
    if (*p == 'Z') {
      le_ptr->ts.tv_sec = time_to_epoch(&tm, 0, 0);
    } else {
      int utc_offset;
      utc_offset = (int)strtol(p, &p, 10);
      le_ptr->ts.tv_sec = time_to_epoch(&tm, utc_offset / 100, utc_offset % 100);
    }

    while (*p == ' ') ++p;

    if (*p >= 'A' && *p <= 'Z' && *(p + 1) == ' ') { //severity
      le_ptr->severity = *p++;
      while (*p == ' ') p++;
      le_ptr->component = *p; //dummy action - just insert first char
      while (*p == ' ') p++;
    }

    if (*p != '[') {
      *err_str_ptr = (char*)calloc(100 + strl, sizeof(char));
      sprintf(*err_str_ptr, "bad format line (no \"[threadname]\" field): %s", line_str);
      return 0;
    }
    p++;
    char* q = p + 1;
    while (*q != ']' && q != (line_str + strl)) q++;
    if (q == (line_str + strl)) {
      *err_str_ptr = (char*)calloc(100 + strl, sizeof(char));
      sprintf(*err_str_ptr, "bad format line (unfinished \"[threadname]\" field): %s", line_str);
      return 0;
    }
    le_ptr->thread_name = (char*)calloc(q - p + 1, sizeof(char));
    strncpy(le_ptr->thread_name, p, q - p);
  } else {
    *err_str_ptr = (char*)calloc(100 + strl, sizeof(char));
    sprintf(*err_str_ptr, "bad format line (no timestamp at start): %s", line_str);
    return 0;
  }
  return 1;
}

