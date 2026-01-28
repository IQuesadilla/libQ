// #include "add.c"
#include <qtest.h>
#include <stdbool.h>
#include <stdio.h>

int add(int a, int b, bool sub);

int main(int argc, char *argv[]) {
  printf("Hello, world! %d\n", add(4, 5, false));
  return 0;
}

test(add_test) {
  printf("Running test add: %d\n", add(4, 5, false));
  return;
}
