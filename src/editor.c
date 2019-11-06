#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <termios.h>
#include <sys/ioctl.h>

#define BUFFADD 50
#define MEM_ERROR -1

typedef struct str
{
	char *chars;
	int length;
} str;

struct text
{
	str *lines;
	int num;
};

struct buf
{
  char *b;
  int len;
};

struct config
{
	int width;
	int height;
	char *filename;
	int wrap;
	int numbers;
	int tabwidth;
  struct termios orig_termios;
  struct termios raw;
};

struct config E;
struct text T;

/* functions */
int init();
int init_modes();
int enable_raw_mode();
void disable_raw_mode();
int get_cursor_position(int *rows, int *cols);
int get_window_size(int *rows, int *cols);
ssize_t get_line(FILE *f, char **line, int *eofflag);
ssize_t read_file(FILE *f, str **lines);
void from_file(char *filename);
void set_wrap(int k);
void set_numbers(int k);
void set_tabwidth(int k);
ssize_t line_insert_tabs(char** line, str current);
int print_pages();
int print_page(int page, int offset);
void test();



int main(int argc, char **argv)
{
  init();
	if (argc >= 2)
  {
    from_file(argv[1]);
  }
  print_pages();
}



int init()
{
  E.tabwidth = 4;
  E.wrap = 1;
  E.numbers = 0;
  E.filename = NULL;
  get_window_size(&E.height, &E.width);
  init_modes();

  return 0;
}

int init_modes()
{
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    return -1;

  atexit(disable_raw_mode);

  E.raw = E.orig_termios;
  E.raw.c_iflag &= ~(BRKINT | IXON | ICRNL | INPCK | ISTRIP);
  E.raw.c_oflag &= ~(OPOST);
  E.raw.c_cflag |= (CS8);
  E.raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  E.raw.c_cc[VMIN] = 0;
  E.raw.c_cc[VTIME] = 1;

  return 0;
}

int enable_raw_mode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.raw) == -1) 
    return -1;
  return 0;
}

void disable_raw_mode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    printf("failed to disable raw mode\n");
}

int get_cursor_position(int *rows, int *cols)
{
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  return 0;
}

int get_window_size(int *rows, int *cols)
{
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
  {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) 
      return -1;
    return get_cursor_position(rows, cols);
  }
  else 
  {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

int buf_append(struct buf *ab, const char *s, size_t len)
{
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return MEM_ERROR;

  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;

  return 0;
}

ssize_t get_line(FILE *f, char **line, int *eofflag)
{
    char* buff = NULL;
    char *tmp = NULL;
    size_t size = 0;
    size_t mem = 0;
    int symbol = 0;

    *eofflag = 0;

    while(1)
    {
        symbol = fgetc(f);

        if (symbol == '\n' || (symbol == EOF && (*eofflag = 1))) break;

        if (size == mem)
        {
            tmp = (char*) realloc(buff, (size + BUFFADD) * sizeof(char));
            if (tmp == NULL) return MEM_ERROR;

            buff = tmp;
            mem += BUFFADD;
        }

        buff[size++] = (char)symbol;
    }

    tmp = (char*) realloc(buff, (size + 1) * sizeof(char));

    if (tmp == NULL) return MEM_ERROR;

    buff = tmp;
    buff[size] = '\0';

    *line = buff;

    return size;
}

ssize_t read_file(FILE *f, str **lines)
{
    str *buff = NULL;
    str *tmp = NULL;
    str line;
    size_t size = 0;
    size_t mem = 0;
    int eofflag;

    while (1)
    {
        line.length = get_line(f, &line.chars, &eofflag);
        if (line.length < 0) break;

        if (size == mem)
        {
            tmp = (str*) realloc(buff, (size + BUFFADD) * sizeof(str));
            if(tmp == NULL) return MEM_ERROR;

            buff = tmp;
            mem += BUFFADD;
        }

        buff[size++] = line;

        if(eofflag) break;

    }

    tmp = (str*) realloc(buff, size * sizeof(str));
    if(tmp == NULL) return MEM_ERROR;

    *lines = tmp;

    return size;
}

void from_file(char *filename)
{
  free(E.filename);
  E.filename = strdup(filename);

  FILE *fp = fopen(filename, "r");
  if (fp == NULL) 
  {
  	printf("failed to open file\n");
  	return;
  }

  T.num = read_file(fp, &T.lines);

  fclose(fp);
}

void set_wrap(int k)
{
  E.wrap = k;
}

void set_numbers(int k)
{
  E.numbers = k;
}

void set_tabwidth(int k)
{
  E.tabwidth = k;
}

ssize_t line_insert_tabs(char** line, str current)
{
  int tabs = 0;
  size_t idx = 0;
  int j;
  char *l;

  for (j = 0; j < current.length; j++)
    if (current.chars[j] == '\t') tabs++;

  l = malloc(current.length + tabs*(E.tabwidth - 1) + 1);
  if (l == NULL) return MEM_ERROR;

  for (j = 0; j < current.length; j++)
  {
    if (current.chars[j] == '\t')
    {
      l[idx++] = ' ';
      while (idx % E.tabwidth != 0) l[idx++] = ' ';
    }
    else
    {
      l[idx++] = current.chars[j];
    }
  }

  *line = l;
  return idx;
}

int print_pages() 
{
  int offset = 0;
  int page = 0;
  char c;
  int printed;

  print_page(page, offset);

  while (1) 
  {
    enable_raw_mode();
    read(STDIN_FILENO, &c, 1);
    disable_raw_mode();

    if (c == ' ')
    {
      printed = print_page(++page, offset);
      if (printed == MEM_ERROR) return MEM_ERROR;
      if (printed == 0) break;
    }
    else if (c == 'q')
    {
      break;
    }
    else if (c == '>' && E.wrap == 0)
    {
      print_page(page, ++offset);
    }
    else if (c == '<' && E.wrap == 0 && offset > 0)
    {
      print_page(page, --offset);
    }
    c = 0;
  }

  return 0;
}

int print_page(int page, int offset)
{
	int i;
  str *current;
  char *line;
  int length;
  char *towrite;
  struct buf buffer;

  buffer.b = NULL;
  buffer.len = 0;
  line = NULL;

	for(i = 0; i < E.height; i++)
	{
    current = page*E.height + i < T.num ? &T.lines[page*E.height + i] : NULL;
    if (current == NULL)
    {
      if (i == 0) return 0;
      while (i++ < E.height - 1)
      {
        printf("\n");
      }
      return i;
    }

    length = line_insert_tabs(&line, *current);
    if (length == MEM_ERROR) return MEM_ERROR;

    length -= offset;
    if (length < 0) length = 0;
    if (length > E.width) length = E.width;

    towrite = malloc(length + 1);
    if (towrite == NULL) return MEM_ERROR;

    if (length > 0) 
    {
      memmove(towrite, &line[offset], length);
    }
    towrite[length] = '\0';
    if (i != E.height - 1)
      printf("%s\n", towrite);
    else
      printf("%s", towrite);
  }

  return i;
}


