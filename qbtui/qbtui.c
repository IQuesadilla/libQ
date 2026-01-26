#include <qb.h>

#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>

#include <ncurses.h>

#include <apr.h>
#include <apr_getopt.h>

apr_getopt_option_t opts[] = {
    {"help", 'h', false},
    {0, 0, 0},
};

int main(int argc, const char *argv[]) {
  qbuild_init();

  apr_pool_t *pool;
  apr_pool_create_unmanaged(&pool);
  apr_getopt_t *os;
  apr_getopt_init(&os, pool, argc, argv);
  os->interleave = 1;

  struct {
    uint help : 1;
  } flags = {0};

  int optch;
  const char *arg;
  apr_status_t s;
  while ((s = apr_getopt_long(os, opts, &optch, &arg)) == APR_SUCCESS) {
    switch (optch) {
    case 'h':
      flags.help = 1;
      break;
    }
  }
  if (s != APR_EOF) {
    return 1;
  }

  if (flags.help || os->ind == os->argc) {
    printf("Usage: %s\n", argv[0]);
    return 0;
  }

  initscr();
  noecho();
  cbreak();
  keypad(stdscr, true);
  curs_set(1);

  if (has_colors() == false) {
    endwin();
    fprintf(stderr, "Your terminal doesn't support colors.\n");
    return 1;
  }

  start_color();
  init_pair(1, COLOR_WHITE, COLOR_BLUE);

  attron(COLOR_PAIR(1));

  int y, x;
  getmaxyx(stdscr, y, x);
  int hy = y / 2;
  int hx = (x / 2) - 12; // Half of below string
  mvwprintw(stdscr, hy, hx, "Constructing build tree");
  refresh();

  node_t *node;
  int rc = setjmp(*(jmp_buf *)root_node_create(&node, NULL, os->argc - os->ind,
                                               &os->argv[os->ind]));
  if (rc == 0) {
    node_include_subdir(node, ".", node);
    while (!qbuild_is_done(node)) {
      qbuild_update_once(node);
      clear();
      mvwprintw(stdscr, 2, 2, "Running build");
      refresh();
    }
    clear();
    mvwprintw(stdscr, 2, 2, "Build passed, cleaning up");
    refresh();
  } else {
    clear();
    mvwprintw(stdscr, 2, 2, "Build failed, cleaning up");
    refresh();
  }
  node_destroy(node);

  attroff(COLOR_PAIR(1));

  getch();

  endwin();

  qbuild_quit();
  return rc;
}
