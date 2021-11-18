#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
// ----------DEFINES----------
#define CTRL_KEY(key) ((key) & 0x1f)
#define CTRL_Q (('q') & 0x1f)

// ----------DATA----------

struct editorConfig {
  int rows;
  int cols;
  struct termios orig_termios;
};

struct editorConfig Editor;
// ----------HELPERS----------

void clearScreenAndPutCursorTopStart() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  // [row;colH
  write(STDOUT_FILENO, "\x1b[H", 3);
}

void writeRow(const char *c, int len) {
  write(STDOUT_FILENO, c, len);
}

// ----------TERMINAL----------

void die(const char *s) {
  clearScreenAndPutCursorTopStart();
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &Editor.orig_termios) == -1)
    die("tcsetattr failed to disable RAW_MODE");
}
void enableRawMode() {

  if(tcgetattr(STDIN_FILENO, &Editor.orig_termios) == -1)
    die("Calling 'tscgetattr' failed`");
  atexit(disableRawMode);
  // Tilda ~ is a bitwise NOT operator: 011100 -> 100011
  struct termios raw = Editor.orig_termios;
  // turns of ctr-S and ctrl-Q
  raw.c_iflag &= ~(BRKINT| ICRNL | INPCK | ISTRIP | IXON);
  // turns of ctr-S and ctrl-Q
  raw.c_oflag &= ~(OPOST);
  // Turns off echo and Canonical mode and ctrl-c and ctrl-z
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  // not needed for modern terminals. Sets character size to 8 bits per byte
  raw.c_cflag |= (CS8);

  // minimum number of bytes for read to terminate 
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  // TCSAFLUSH -> waits for all pending output to be written and discards any input 
  // that hasn't been read.
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("Failed to set Raw_Mode");
}

char editorReadKey() {
  int nread;
  char c = '\0';
  while((nread = read(STDIN_FILENO, &c, 1)) != 1) {

    if (nread == -1 && errno != EAGAIN)
      die("Failed to call 'read'");
  }
  return c;
}

int getCursorPositionFallBack(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  // ask for the position of the cursor. The reply is in stdin (a disgusting api)
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1; 

  printf("\r\n");
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;

    i += 1;
  }
  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) !=2) return -1;
  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize WindowSize;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &WindowSize) == -1 || WindowSize.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1; 
    return getCursorPositionFallBack(rows, cols);
  } else {
    *rows = WindowSize.ws_row;
    *cols = WindowSize.ws_col;
    return 0;
  }
}
// ----------Custom Buffers----------

// is a dynamic/appendable buffer
struct String {
  char *b;
  int len;
};

void StringAdd(struct String *prev_String, const char *incoming_string, int len) {
  char *new = realloc(prev_String->b, prev_String->len + len); 


  if (new == NULL) return;
  memcpy(&new[prev_String->len], incoming_string, len);
  prev_String->b   = new;
  prev_String->len += len;

}
void StringFree(struct String *current_String) {
  free(current_String->b);
}

// ----------OUTPUT----------

void editorDrawRows(struct String *str) {

  int y = 0;
  for (; y < Editor.rows; y++) {
    if (y < Editor.rows -1) { 
      StringAdd(str, "~\r\n", 3);
    } else {
      StringAdd(str, "~", 1);
    }
  }
}

void editorRefreshScreen() {
  struct String str = {NULL, 0};

  StringAdd(&str, "\x1b[2J", 4);
  StringAdd(&str, "\x1b[H", 3);

  editorDrawRows(&str);

  StringAdd(&str, "\x1b[H", 3);
  writeRow(str.b, str.len);
  StringFree(&str);
}

// ----------INPUT----------

void editorProcessKeypress() {
  char c = editorReadKey();

  switch (c) {
    case CTRL_Q:
      clearScreenAndPutCursorTopStart();
      exit(0);
      break;
  }
}

// ----------INIT----------

void initEditor() {
  int failedToGetWindowSize = getWindowSize(&Editor.rows, &Editor.cols) == -1;

  if (failedToGetWindowSize)
    die("ERROR: failed to init editor due to getWindowSize() failing");

}
int main() {
  enableRawMode();
  initEditor();
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
