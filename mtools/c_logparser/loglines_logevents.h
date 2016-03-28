#include <sys/time.h>

struct logevent {
  char component;
  struct timeval ts;
  char severity;
  char* thread_name;
  char* first_tokens;
  size_t fpos;
};

void destroy_logevent(struct logevent* le);

int fill_le_from_line_str(struct logevent* le_ptr, char* line_str, size_t strl, int modes, char** err_str_ptr);

