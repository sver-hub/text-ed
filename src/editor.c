#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <math.h>

#define BUFFADD 250
#define MEM_ERROR -1

struct buffer
{
  char *chars;
  int len;
  int mem;
};

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



struct config
{
	int width;
	int height;
	char *filename;
	int wrap;
	int numbers;
	int tabwidth;
  int blank;
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
int print_page(int start, int offset);
int insert_symbols(str *line, char* s, int pos, int num);
int replace_symbols(str *line, char *s, int pos, int num);



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
  E.wrap = 0;
  E.numbers = 1;
  E.blank = 5;
  E.filename = NULL;
  get_window_size(&E.height, &E.width);
  E.height -= 1;
  init_modes();

  return 0;
}

/* LOW LEVEL STUFF 
  ________________
*/
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

/* GETTING LINES FROM FILE
  _______________________
*/
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

/* SETTINGS 
  _________
*/
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


/* STRING OPERATIONS
  __________________
*/
int append(struct buffer *buf, const char* s, int len)
{
  char *tmp = NULL;
  int j;
  int memadd;

  if (buf->len + len > buf->mem)
  {
    memadd = BUFFADD > len ? BUFFADD : len;

    tmp = (char*)realloc(buf->chars, buf->mem + memadd);
    if (tmp == NULL) return MEM_ERROR;

    buf->chars = tmp;
    buf->mem += memadd;
  }

  for (j = 0; j < len; j++)
  {
    buf->chars[j + buf->len] = s[j];
  }

  buf->len += len;

  return 0;
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

int insert_symbols(str *line, char* s, int pos, int num)
{
  char *tmp = NULL;
  int j, i;

  if (pos < 0) pos = 0;
  if (pos > line->length) pos = line->length;

  tmp = malloc(line->length + num + 1);
  if (tmp == NULL) return MEM_ERROR;

  for (j = line->length + num - 1; j >= pos + num; j--)
  {
    tmp[j] = line->chars[j - num];
  }

  for (i = num - 1; i >= 0; i--)
  {
    tmp[j--] = s[i];
  }

  while (j >= 0)
  {
    tmp[j] = line->chars[j];
    j--;
  }

  tmp[line->length + num] = '\0';
  free(line->chars);
  line->chars = tmp;
  line->length += num;

  return 0;
}

int replace_symbols(str *line, char *s, int pos, int num)
{
  int j;

  if (line->length < pos + num - 1)
    printf("out of bounds\n");
    return -1;

  for (j = pos + num - 2; j > pos - 2; j--)
  {
    line->chars[j] = s[--num];
  }

  return 0;
}

/* PRINT PAGES 
  ____________
*/
int app_num(struct buffer *buf, int n) 
{
  char *m;
  int k;
  int t = (int)(ceil(log10(n)));
  if (n == 1) t = 1;
  else if (n == 10) t = 2;
  else if (n == 100) t = 3;
  else if (n == 1000) t = 4;

  for (k = t; k < E.blank - 1; k++)
    append(buf, " ", 1);
  m = malloc(t);
  sprintf(m, "%d", n);
  append(buf, m, t);
  free(m);
  append(buf, " ", 1);

  return 0;
}

struct pagesInfo
{
  int index;
  int offset;
  int x;
  int pindex;
  int px;
  int of;
};

int page(struct pagesInfo *I)
{
  int width;
  int rlen;
  char *rend;

  int j;
  int k;
  int idx;
  int numrows;

  struct buffer buf;
    int rows = 0;
  
    buf.chars = NULL;
    buf.len = 0;
    buf.mem = 0;

  width = (E.numbers || E.wrap) ? E.width - E.blank : E.width;

  if (I->of)
  {
    I->index = I->pindex;
    I->x = I->px;
  }
  

  if (I->index != 0 || I->of == 1) append(&buf, "\x1b[H", 3); //move to 1,1
  I->pindex = I->index;
  I->px = I->x;
  I->of = 0;

  while (rows < E.height)
  {
    if (I->index >= T.num) 
    {
      if (rows == 0) return 0;
      while (rows++ < E.height)
      {
        append(&buf, "\x1b[K\n", 4);
      }
      break;
    }

    if (I->x != 0)
    {
      for (k = 0; k < E.blank - 3; k++) append(&buf, " ", 1);
      append(&buf, "-> ", 3);
    }
    else if (E.numbers) app_num(&buf, I->index + 1);
    else if (E.wrap) for (k = 0; k < E.blank; k++) append(&buf, " ", 1);


    if (!E.wrap)
    {
      rlen = line_insert_tabs(&rend, T.lines[I->index]);
      if (rlen == MEM_ERROR) return MEM_ERROR;
      
      rlen -= I->offset;
      if (rlen < 0) rlen = 0;
      if (rlen > width) rlen = width;

      append(&buf, &rend[I->offset], rlen);

      free(rend);
    }
    else
    {
      idx = 5;
      numrows = 1;
      j = 0;

      if (I->x > 0)
      {
        j = I->x;
        I->x = 0;
      }

      while (j < T.lines[I->index].length)
      {
        if (idx == numrows*(width + 6) - 1)
        {
          rows++;
          if (rows == E.height)
          {
            I->x = j - 1;
            I->index--;
            break;
          }

          append(&buf, "\n", 1);
          for (k = 0; k < E.blank - 3; k++)
            append(&buf, " ", 1);
          append(&buf, "-> ", 3);
          idx += E.blank + 1;
          numrows++;
          
        }
        else if (T.lines[I->index].chars[j] == '\t')
        {
          do 
          {
            append(&buf, " ", 1);
            idx++;
            if (idx - numrows*(width + 6) > width)
            {
              j++;
              break;
            }
          } while (idx % E.tabwidth != 0);
          j++;
        }
        else
        {
          append(&buf, &T.lines[I->index].chars[j++], 1);
          idx++;
        } 
      }
    }

    append(&buf, "\x1b[K\n", 4);
    I->index++;
    rows++;
  }

  write(STDIN_FILENO, buf.chars, buf.len);
  free(buf.chars);
  return 1;
}


int print_pages() 
{
  char c;
  int printed;
  int j;

  struct pagesInfo I;

  I.index = 0;
  I.offset = 0;
  I.x = 0;
  I.pindex = 0;
  I.px = 0;
  I.of = 0;

  page(&I);

  while (1) 
  {
    enable_raw_mode();
    read(STDIN_FILENO, &c, 1);
    disable_raw_mode();

    if (c == ' ')
    {
      printed = page(&I);
      if (printed == MEM_ERROR) return MEM_ERROR;
      if (printed == 0) 
      {
        write(STDOUT_FILENO, "\x1b[H", 3);
        for (j = 0; j < E.height; j++)
          write(STDOUT_FILENO, "\x1b[K\n", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        break;
      }
    }
    else if (c == 'q')
    {
      write(STDOUT_FILENO, "\x1b[H", 3);
      for (j = 0; j < E.height; j++)
        write(STDOUT_FILENO, "\x1b[K\n", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      break;
    }
    else if (c == '>' && E.wrap == 0)
    {
      I.offset++;
      I.of = 1;
      page(&I);
    }
    else if (c == '<' && E.wrap == 0 && I.offset > 0)
    {
      I.offset--;
      I.of = 1;
      page(&I);
    }
    c = 0;
  }

  return 0;
}


// int next_line(struct buffer *buf, int index, int offset)
// {
//   int width;
//   int rlen;
//   char *rend;

//   int j;
//   int k;
//   int idx = 0;
//   int numrows = 0;


//   width = (E.numbers || E.wrap) ? E.width - E.blank : E.width;

//   if (index >= T.num) return 0;

//   if (E.numbers) app_num(buf, index + 1);
//   else if (E.wrap)
//   {
//     for (k = 0; k < E.blank; k++) append(buf, " ", 1);
//   }


//   if (!E.wrap)
//   {
//     rlen = line_insert_tabs(&rend, T.lines[index]);
//     if (rlen == MEM_ERROR) return MEM_ERROR;
    
//     rlen -= offset;
//     if (rlen < 0) rlen = 0;
//     if (rlen > width) rlen = width;

//     append(buf, &rend[offset], rlen);
//     append(buf, "\x1b[K", 3);

//     free(rend);
//   }
//   else
//   {
//     for (j = 0; j < T.lines[index].length;)
//     {
//       if (idx >= width + numrows*(width + 6))
//       {
//         append(buf, "\n", 1);
//         for (k = 0; k < E.blank - 3; k++)
//           append(buf, " ", 1);
//         append(buf, "-> ", 3);
//         idx += E.blank + 1;
//         numrows++;
//       }
//       else if (T.lines[index].chars[j] == '\t')
//       {
//         do 
//         {
//           append(buf, " ", 1);
//           idx++;
//           if (idx - numrows*(width + 6) > width)
//             break;
//         }
//         while (idx % E.tabwidth != 0);
//       }
//       else
//       {
//         append(buf, &T.lines[index].chars[j++], 1);
//         idx++;
//       }
//       append(buf, "\x1b[K", 3);
//     }
//   }

//   return numrows + 1;
// }

// int print_page(int start, int offset) 
// {
//   struct buffer buf;
//   int rows = 0;
//   int i = start;
//   int added;

//   buf.chars = NULL;
//   buf.len = 0;
//   buf.mem = 0;

//   append(&buf, "\x1b[H", 3); //move to 1,1

//   while (rows < E.height)
//   {
//     added = next_line(&buf, i++, offset);
//     if (added == MEM_ERROR) return MEM_ERROR;
//     if (added == 0) 
//     {
//       while (rows++ < E.height)
//       {
//         append(&buf, "\x1b[K\n", 4);
//       }
//       break;
//     }

//     rows += added;
//     append(&buf, "\n", 1);
//   }

//   write(STDOUT_FILENO, buf.chars, buf.len);
  
//   free(buf.chars);

//   return added;
// }

// int print_pages() 
// {
//   int offset = 0;
//   int start = 0;
//   char c;
//   int printed;

//   print_page(start, offset);

//   while (1) 
//   {
//     enable_raw_mode();
//     read(STDIN_FILENO, &c, 1);
//     disable_raw_mode();

//     if (c == ' ')
//     {
//       start += E.height;
//       printed = print_page(start, offset);
//       if (printed == MEM_ERROR) return MEM_ERROR;
//       if (printed == 0) break;
//     }
//     else if (c == 'q')
//     {
//       break;
//     }
//     else if (c == '>' && E.wrap == 0)
//     {
//       print_page(start, ++offset);
//     }
//     else if (c == '<' && E.wrap == 0 && offset > 0)
//     {
//       print_page(start, --offset);
//     }
//     c = 0;
//   }

//   return 0;
// }



// int print_page(struct buffer *buf, int index, int offset)
// {
// 	int i = 0;
//   int j;
//   int k;
//   int idx;
//   int rowswr = 0;
//   int numrows;
//   //struct buffer buf;
//   str *current;
//   char *line = NULL;
//   int length;
//   int width;
  

//   // buf.chars = NULL;
//   // buf.len = 0;
//   // buf.mem = 0;

//   //append(&buf, "\x1b[H", 3);    //move to 1,1

//   width = (E.numbers || E.wrap) ? E.width - E.blank : E.width;

// 	while (rowswr < E.height)
// 	{
//     current = start + i < T.num ? &T.lines[start + i] : NULL;
//     if (current == NULL)
//     {
//       if (i == 0) return 0;
//       while (i++ < E.height)
//       {
//         if (E.numbers) 
//         {
//           app_num(&buf, start + i + 1);
//         }
//         append(&buf, "\x1b[K\n", 4);
//       }
//       return i;
//     }

//     if (E.numbers) app_num(&buf, start + i + 1);
//     else if (E.wrap)
//     {
//       for (k = 0; k < E.blank; k++) append(&buf, " ", 1);
//     }

//     length = line_insert_tabs(&line, *current);
//     if (length == MEM_ERROR) return MEM_ERROR;

//     if (!E.wrap)
//     {
//       length -= offset;
//       if (length < 0) length = 0;
//       if (length > width) length = width;

//       append(&buf, &line[offset], length);
//       append(&buf, "\x1b[K", 3);
//     }
//     else
//     {
//       idx = 0;
//       numrows = 0;
//       for (j = 0; j < current->length;)
//       {
//         if (idx >= width + numrows*(width + 6))
//         {
//           append(&buf, "\n", 1);
//           for (k = 0; k < E.blank - 3; k++)
//             append(&buf, " ", 1);
//           append(&buf, "-> ", 3);
//           idx += E.blank + 1;
//           numrows++;
//           rowswr++;
//         }
//         else if (current->chars[j] == '\t')
//         {
//           do 
//           {
//             append(&buf, " ", 1);
//             idx++;
//             if (idx - numrows*(width + 6) > width)
//               break;
//           }
//           while (idx % E.tabwidth != 0);
//         }
//         else
//           append(&buf, &current->chars[j++], 1);
//           append(&buf, "\x1b[K", 3);
//           idx++;
//       }
//     }

//     // append(&buf, "\0", 1);

//     // if (E.numbers) printf("%*d ", E.blank - 1, start + i + 1);
//     // else if (E.wrap) printf("     ");
//     // printf("%s\n", buf.chars);
//     append(&buf, "\n", 1);
//     rowswr++;
//     i++;

//     free(line);
//   }
//   write(STDOUT_FILENO, buf.chars, buf.len);
  
//   free(buf.chars);
//   buf.chars = NULL;
//   buf.len = 0;
//   buf.mem = 0;

//   return i;
// }




