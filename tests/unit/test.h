#ifndef TESTS_UNIT_TEST_H
#define TESTS_UNIT_TEST_H

#include <stdio.h>

/* Minimal, dependency-free test runner -- no third-party framework, no
 * test discovery magic. Each test_*.c defines its own test functions,
 * RUN()s each from main(), and returns test_report(). `test_failures`
 * is file-scope `static` so every translation unit that includes this
 * header gets its own independent counter; nothing here is shared
 * across the tests/unit/test_*.c binaries, which are each their own
 * standalone executable (see the Makefile). */

static int test_failures;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            test_failures++;                                                \
        }                                                                   \
    } while (0)

#define RUN(test_fn)                   \
    do {                               \
        printf("RUN  %s\n", #test_fn); \
        test_fn();                     \
    } while (0)

static inline int test_report(void) {
    if (test_failures > 0) {
        fprintf(stderr, "%d assertion(s) failed\n", test_failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}

#endif
