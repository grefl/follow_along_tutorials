#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
// ----------DEFINES----------
#define CTRL_KEY(key) ((key) & 0x1f)
#define CTRL_Q (('q') & 0x1f)

// ----------DATA----------

struct termios orig_termios;

// ----------TERMINAL----------

void die(const char *s) {
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr failed to disable RAW_MODE");
}
void enableRawMode() {

  if(tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    die("Calling 'tscgetattr' failed`");
  atexit(disableRawMode);
  // Tilda ~ is a bitwise NOT operator: 011100 -> 100011
  struct termios raw = orig_termios;
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

// ----------INIT----------

int main() {
  enableRawMode();
  while (1) {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
      die("Failed to call 'read'");
    // iscontrl checks for non printable control characters
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n", c, c);
    }
    if (c == CTRL_Q) break;
  }
  return 0;
}
