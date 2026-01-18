// #include "add.c"
#include <stdbool.h>
#include <stdio.h>

int add(int a, int b, bool sub);

int main(int argc, char *argv[]) {
  printf("Hello, world! %d\n", add(4, 5, false));
  return 0;
}
