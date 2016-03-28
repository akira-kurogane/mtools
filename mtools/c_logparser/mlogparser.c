#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "loglines_logevents.h"

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

void
parse_lines(FILE* fstr) {
  int c;
  const size_t bufsize = 256 * 1024;
  char buf[bufsize];

  c = getc(fstr);
  if (c == EOF)
    return;

  ungetc(c, fstr);
  c = 0;

  size_t fpos = 0;
  while (fgets(buf, bufsize, fstr) != NULL) {
    size_t strl = strlen(buf);
    
    struct logevent le;
    memset(&le, 0, sizeof(struct logevent));
    le.fpos = fpos;

    int modes = 0; //todo: make an overlapping set of flags of what actions to do
    char* err_str;
    int fill_res = 0;
    fill_res = fill_le_from_line_str(&le, buf, strl, modes, &err_str);

    if (fill_res) {
debug_dump_logevent(&le);
    } else {
      fprintf(stderr, "%s\n", err_str);
      free(err_str);
    }
    destroy_logevent(&le);

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

int main(int argc, char** argv) {

  if (argc < 2) {
    parse_file("-");
  } else {
    parse_file(argv[1]);
  }

  exit(EXIT_SUCCESS);
}
