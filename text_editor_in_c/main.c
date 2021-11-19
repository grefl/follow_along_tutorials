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
#define VERSION "0.0.1"

// ----------DATA----------
enum EditorMode {
  VISUAL = 'v',
  INSERT = 'i',
};

enum editorKey {
  CURSOR_LEFT = 1000,
  CURSOR_RIGHT,
  CURSOR_UP,
  CURSOR_DOWN,
  PAGE_UP,
  PAGE_DOWN,
  DELETE_KEY,
  HOME_KEY,
  END_KEY,
};

struct editorConfig {
  // Cursor stuff
  int cursor_x;
  int cursor_y;
  // Editor stuff 
  int rows;
  int cols;
  char mode;
  // External terminal stuff - at the moment just used to turn on RAW_MODE 
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
// Same as writeRow but used for rendering chunks
// TODO:(grefl) not ideal, change later?
void render(const char *c, int len) {
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

int editorReadKey() {
  int nread;
  char c = '\0';
  while((nread = read(STDIN_FILENO, &c, 1)) != 1) {

    if (nread == -1 && errno != EAGAIN)
      die("Failed to call 'read'");
  }
  char sequence_symbol = '\x1b'; 
  // if escape sequence
  if (c == sequence_symbol) {
   char sequence[3]; 

   if (read(STDIN_FILENO, &sequence[0], 1) != 1) return sequence_symbol;
   if (read(STDIN_FILENO, &sequence[1], 1) != 1) return sequence_symbol;

   if (sequence[0] == '[') {
    if (sequence[1] >= '0' && sequence[1] <= '9') {
      if (read(STDIN_FILENO, &sequence[2], 1) != 1) return sequence_symbol;
      if (sequence[2] == '~') {
        switch(sequence[1]) {
          case '1': return HOME_KEY;
          case '3': return DELETE_KEY;
          case '4': return END_KEY;
          case '5': return PAGE_UP;
          case '6': return PAGE_DOWN;
          case '7': return HOME_KEY;
          case '8': return END_KEY;
        }
      }
    } else {
      switch(sequence[1]) {
        case 'A': return CURSOR_UP; 
        case 'B': return CURSOR_DOWN; 
        case 'C': return CURSOR_RIGHT; 
        case 'D': return CURSOR_LEFT; 
        case 'H': return HOME_KEY; 
        case 'F': return END_KEY; 
      }
    }
   //return sequence_symbol;
   } else if (sequence[0] == 'O') {
      switch(sequence[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
   }
   return sequence_symbol;
  } else {
    if (Editor.mode == VISUAL) {
      switch (c) {
          case 'j': return CURSOR_DOWN;
          case 'k': return CURSOR_UP;
          case 'h': return CURSOR_LEFT;
          case 'l': return CURSOR_RIGHT;
      }
    }
    return c;
  }
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
    if (y == Editor.rows / 2) {
      char welcome_message[80];
      int len = snprintf(welcome_message, sizeof(welcome_message),
        "Kilo editor -- version %s", VERSION);
      if (len > Editor.cols) len = Editor.cols;
      int padding = (Editor.cols - len) / 2;
      if (padding) {
        StringAdd(str, " ", 1);
        padding--;
        while (padding--) StringAdd(str, " ", 1);
      }
      StringAdd(str, welcome_message, len);
    } else if (y < Editor.rows -1) {
      StringAdd(str, "~", 1);
    }
    // clear single line to the RIGHT of the cursor
    StringAdd(str, "\x1b[K",3);

    if (y < Editor.rows - 1) { 
      StringAdd(str, "\r\n", 2);
    } else {
      // status bar
      if (Editor.mode == VISUAL) {
        StringAdd(str, "-- VISUAL --", 12);
      } else {
        StringAdd(str, "-- INSERT --", 12);
      }
    }
  }
}

void editorRefreshScreen() {
  struct String str = {NULL, 0};
  // Clear cursor before generating rows 
  // To prevent flickering
  StringAdd(&str, "\x1b[?25l", 6);
  // Set Cursor to {x: 0, y:0}
  StringAdd(&str, "\x1b[H", 3);

  editorDrawRows(&str);
  
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", Editor.cursor_y + 1, Editor.cursor_x + 1);
  // Cursor -> x,y
  StringAdd(&str, buf, strlen(buf));

  // Add cursor back
  StringAdd(&str, "\x1b[?25h", 6);

  // Render 
  render(str.b, str.len);
  StringFree(&str);
}

// ----------INPUT----------

void editorMoveCursor(int key) {
  
  switch(key) {
    case CURSOR_DOWN:
      if (Editor.cursor_y < Editor.rows - 2)
        Editor.cursor_y ++; 
      break;
    case CURSOR_UP:
      if (Editor.cursor_y > 0)
        Editor.cursor_y --;
      break;
    case CURSOR_LEFT:
      if (Editor.cursor_x > 0)
        Editor.cursor_x --;
      break;
    case CURSOR_RIGHT:
      if (Editor.cursor_x < Editor.cols - 1)
        Editor.cursor_x ++;
      break;
  }

}

void editorProcessKeypress() {
  int c = editorReadKey();

  switch (c) {
    case CTRL_Q:
      clearScreenAndPutCursorTopStart();
      exit(0);
      break;
    

    case HOME_KEY:
      Editor.cursor_x = 0;
      break;
    case END_KEY:
      Editor.cursor_x = Editor.cols -1;
      break;


    case PAGE_UP:
    case PAGE_DOWN:
      {
        int times = Editor.rows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? CURSOR_UP : CURSOR_DOWN);
      }
      break;


    case CURSOR_UP:
    case CURSOR_DOWN:
    case CURSOR_RIGHT:
    case CURSOR_LEFT:
      editorMoveCursor(c);
      break;
  }
}

// ----------INIT----------

void initEditor() {
  // CURSOR init
  Editor.cursor_x = 0;
  Editor.cursor_y = 0;
  Editor.mode = VISUAL;

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
