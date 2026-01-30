#ifndef LIBQ_QTEST_H
#define LIBQ_QTEST_H

typedef void (*test_func_t)(void);

#ifdef ENABLE_TESTS

#define test(name)                                                             \
  void name(void);                                                             \
  __attribute__((section("test_array"), used)) test_func_t name##_ptr = name;  \
  void name(void)

extern test_func_t __start_test_array;
extern test_func_t __stop_test_array;
__attribute__((used)) static void *__test_array_dummy[] = {&__start_test_array,
                                                           &__stop_test_array};

__attribute__((section("test_array"), used)) static test_func_t sentinel =
    (void *)0;

#else

#define test(name) static void name(void)

#endif

#endif
