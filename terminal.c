#include "terminal.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <termbox.h>

// Ward against calling tb_shutdown twice. A common case where this would
// happen is if we receive SIGTERM while the editor is in the background.
static bool terminal_is_shut_down = false;

static void terminal_resume(void) {
  int err = tb_init();
  if (err) {
    fprintf(stderr, "tb_init() failed with error code %d\n", err);
    exit(1);
  }
  terminal_is_shut_down = false;
}

static void terminal_shutdown(void) {
  if (!terminal_is_shut_down) {
    terminal_is_shut_down = true;
    tb_shutdown();
  }
}

void terminal_suspend(void) {
  terminal_shutdown();
  // SIGTSTP is the event that is normally generated by hitting Ctrl-Z.
  raise(SIGTSTP);
  terminal_resume();
}

static void terminal_sighandler(int signum) {
  terminal_shutdown();
  raise(signum);
}

void terminal_init(void) {
  terminal_resume();
  atexit(terminal_shutdown);

// On some platforms (e.g. in the Travis linux build environment), the
// definitions of struct sigaction and related constructs are such that this
// code triggers some unfortunate warnings when building with clang and
// -Weverything. This is a bit out of our control, so let's just ignore them.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif
  struct sigaction sa;
  sa.sa_handler = terminal_sighandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESETHAND;
#ifdef __clang__
#pragma clang diagnostic pop
#endif

  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
}
