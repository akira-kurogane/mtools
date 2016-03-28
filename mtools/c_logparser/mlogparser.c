#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
//following two defines are for strptime
#define _XOPEN_SOURCE
#define __USE_XOPEN
#include <time.h>

const char iso_dt_fmt[] = "%Y-%m-%dT%H:%M:%S";
#define iso_fmt_len 19
static char min_ts_str[iso_fmt_len + 1];
static char max_ts_str[iso_fmt_len + 1];
static char common_ts_prefix[iso_fmt_len + 1];
static size_t common_ts_prefix_len;

struct logevent {
  char component;
  struct timeval ts;
  char severity;
  char* thread_name;
  char* first_tokens;
  size_t fpos;
};

void destroy_logevent(struct logevent* le) { 
  free(le->thread_name);
  free(le->first_tokens);
}

void debug_dump_logevent(struct logevent* le) {
  fprintf(stdout, "fpos=%zu,comp=%c,ts=%d.%6d", le->fpos, le->component, le->ts.tv_sec, le->ts.tv_usec);
  if (le->severity) {
    fprintf(stdout, ",sev=%c", le->severity);
  }
  if (le->thread_name) {
    fprintf(stdout, ",tn=%s", le->thread_name);
  }
  fprintf(stdout, "\n");
}

//from https://gmbabar.wordpress.com/2010/12/01/mktime-slow-use-custom-function/
//Seems to be much faster than timegm(). mktime() was a lot slower again.
time_t time_to_epoch (const struct tm *ltm, int utcdiff_hrs, int utcdiff_mins) {
   const int mon_days [] =
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

void
parse_lines(FILE* fstr) {
  int c;
  const size_t bufsize = 256 * 1024;
  char buf[bufsize];
  char* strptime_p;

  struct tm tm;

  c = getc(fstr);
  if (c == EOF)
    return;

  ungetc(c, fstr);
  c = 0;

  size_t fpos = 0;
  while (fgets(buf, bufsize, fstr) != NULL) {
    size_t strl = strlen(buf);

    /* Section to filter by timedate range
    if (do_filter && (strncmp(common_ts_prefix, buf, common_ts_prefix_len) != 0 ||
        strncmp(min_ts_str, buf, iso_fmt_len) > 0 || strncmp(max_ts_str, buf, iso_fmt_len) < 0) {
      continue;
    }
    */

    if (strl > 19 && buf[10] == 'T' && buf[19] == '.') { //assume ISO datetime
      //fprintf(stdout, "%s", buf);
      struct logevent le;
      memset(&le, 0, sizeof(struct logevent));
      le.fpos = fpos;

      memset(&tm, 0, sizeof(struct tm));
      strptime_p = strptime(buf, iso_dt_fmt, &tm);
      if (strptime_p != buf + iso_fmt_len) {
        fprintf(stderr, "bad format line (not ISO date string): %s", buf);
        destroy_logevent(&le);
        continue;
      }

      char *p = buf + 20; //the end of the "YYYY-mm-ddTHH:MM:SS" string, where the decimal for the milliseconds is expected to be
      le.ts.tv_usec = 1000 * strtol(p, &p, 10);
      if (*p == 'Z') {
        le.ts.tv_sec = time_to_epoch(&tm, 0, 0);
      } else {
        int utc_offset;
        utc_offset = (int)strtol(p, &p, 10);
        le.ts.tv_sec = time_to_epoch(&tm, utc_offset / 100, utc_offset % 100);
      }

      while (*p == ' ') ++p;

      if (*p >= 'A' && *p <= 'Z' && *(p + 1) == ' ') { //severity
        le.severity = *p++;
        while (*p == ' ') p++;
        le.component = *p; //dummy action - just insert first char
        while (*p == ' ') p++;
      }

      if (*p != '[') {
        fprintf(stderr, "bad format line (no \"[threadname]\" field): %s", buf);
        destroy_logevent(&le);
        continue;
      }
      p++;
      char* q = p + 1;
      while (*q != ']' && q != (buf + strl)) q++;
      if (q == (buf + strl)) {
        fprintf(stderr, "bad format line (unfinished \"[threadname]\" field): %s", buf);
        destroy_logevent(&le);
        continue;
      }
      le.thread_name = (char*)calloc(q - p + 1, sizeof(char));
      strncpy(le.thread_name, p, q - p);

debug_dump_logevent(&le);
      destroy_logevent(&le);
    } else {
      fprintf(stderr, "bad format line: %s", buf);
      continue;
    }

    fpos += strl; //the fpos of the file data of the next iteration
  }
}

void parse_file(char const *filepath) {
  FILE *fstr;

  if (strcmp(filepath, "-") == 0) {
    fstr = stdin;
  } else {
    fstr = fopen(filepath, "r");
    if (fstr == NULL) {
      fprintf(stderr, "fopen(\"%s\") failed with error code %d\n", filepath, errno);
      exit(EXIT_FAILURE);
    }
  }

  posix_fadvise(fileno(fstr), 0, 0, POSIX_FADV_SEQUENTIAL);

  parse_lines(fstr);

  if (ferror(fstr)) {
    fprintf(stderr, "reading stream of \"%s\" failed with error code %d\n", filepath, errno);
    fclose(fstr);
    exit(EXIT_FAILURE);
  }
  fclose(fstr);
}

void auto_set_min_max_ts_strings() {
  time_t t;
  struct tm* tms;
  size_t i;
  t = time(NULL);
  t -= 365 * 86440; //go back ~1 year
  tms = gmtime(&t);
  strftime(min_ts_str, iso_fmt_len + 1, iso_dt_fmt, tms);
  t += 365 * 86440 + 86440; //return back, add 1 day into future
  tms = localtime(&t);
  strftime(max_ts_str, iso_fmt_len + 1, iso_dt_fmt, tms);
  i = 0;
  while (i < iso_fmt_len + 1 && min_ts_str[i] == max_ts_str[i]) {
    common_ts_prefix[i] = min_ts_str[i];
	++i;
  }
  common_ts_prefix_len = i;
  common_ts_prefix[common_ts_prefix_len] = '\0';
}

int main(int argc, char** argv) {

  auto_set_min_max_ts_strings();
  /*char junk[100];
  strncpy(junk,                       min_ts_str, iso_fmt_len);
  strncpy(junk + iso_fmt_len,         " - ",      3);
  strncpy(junk + iso_fmt_len + 3,     max_ts_str, iso_fmt_len);
  strncpy(junk + (2 * iso_fmt_len) + 3, "\n\0",     2);
  fprintf(stdout, "%s", junk);
  fprintf(stdout, "Common prefix = %s\n", common_ts_prefix);
exit(EXIT_SUCCESS);*/

  if (argc < 2) {
    parse_file("-");
  } else {
    parse_file(argv[1]);
  }

  exit(EXIT_SUCCESS);
}
