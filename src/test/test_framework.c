/*
 * FreeBSD Routing Library Test Framework Implementation
 */

#include "test_framework.h"
#include <time.h>
#include <unistd.h>

/* Global test result tracking */
test_result_t g_test_result = {0};

/* Memory tracking (simple implementation) */
static size_t allocated_bytes = 0;
static int tracking_enabled = 0;

/* Color codes for output */
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_RESET   "\x1b[0m"

/* Check if output supports colors */
static int use_colors(void) {
    return isatty(STDOUT_FILENO);
}

/* Initialize test framework */
int test_framework_init(void) {
    memset(&g_test_result, 0, sizeof(g_test_result));
    return 0;
}

/* Cleanup test framework */
void test_framework_cleanup(void) {
    /* Nothing to clean up currently */
}

/* Run a single test suite */
int test_run_suite(const test_suite_t* suite) {
    if (!suite || !suite->tests) {
        test_log_error(__FILE__, __LINE__, __func__, "Invalid test suite");
        return -1;
    }

    int colors = use_colors();

    printf("%s=== Running Test Suite: %s ===%s\n",
           colors ? COLOR_BLUE : "", suite->name, colors ? COLOR_RESET : "");

    if (suite->description) {
        printf("Description: %s\n", suite->description);
    }
    printf("\n");

    /* Run setup if provided */
    if (suite->setup && suite->setup() != 0) {
        printf("%s[ERROR]%s Suite setup failed\n",
               colors ? COLOR_RED : "", colors ? COLOR_RESET : "");
        return -1;
    }

    /* Run each test case */
    for (int i = 0; suite->tests[i].name != NULL; i++) {
        test_case_t* test = &suite->tests[i];

        if (!test->enabled) {
            printf("%s[SKIP ]%s %s - %s\n",
                   colors ? COLOR_YELLOW : "", colors ? COLOR_RESET : "",
                   test->name, test->description);
            g_test_result.skipped_tests++;
            continue;
        }

        g_test_result.total_tests++;

        printf("Running: %s ... ", test->name);
        fflush(stdout);

        perf_timer_t timer;
        PERF_START(&timer);

        int result = test->func();

        PERF_END(&timer);

        switch (result) {
            case 0:  /* Pass */
                printf("%s[PASS]%s (%.2fms)\n",
                       colors ? COLOR_GREEN : "", colors ? COLOR_RESET : "",
                       timer.elapsed_ms);
                g_test_result.passed_tests++;
                break;
            case 1:  /* Skip */
                printf("%s[SKIP]%s (%.2fms)\n",
                       colors ? COLOR_YELLOW : "", colors ? COLOR_RESET : "",
                       timer.elapsed_ms);
                g_test_result.skipped_tests++;
                break;
            default: /* Fail */
                printf("%s[FAIL]%s (%.2fms)\n",
                       colors ? COLOR_RED : "", colors ? COLOR_RESET : "",
                       timer.elapsed_ms);
                g_test_result.failed_tests++;
                break;
        }
    }

    /* Run teardown if provided */
    if (suite->teardown && suite->teardown() != 0) {
        printf("%s[ERROR]%s Suite teardown failed\n",
               colors ? COLOR_RED : "", colors ? COLOR_RESET : "");
        return -1;
    }

    printf("\n");
    return 0;
}

/* Run all test suites */
int test_run_all_suites(test_suite_t* suites[], int num_suites) {
    test_reset_results();

    for (int i = 0; i < num_suites; i++) {
        if (test_run_suite(suites[i]) != 0) {
            return -1;
        }
    }

    return 0;
}

/* Logging functions */
void test_log_info(const char* message, ...) {
    va_list args;
    va_start(args, message);
    printf("[INFO] ");
    vprintf(message, args);
    printf("\n");
    va_end(args);
}

void test_log_error(const char* file, int line, const char* func, const char* message, ...) {
    va_list args;
    va_start(args, message);

    int colors = use_colors();
    printf("%s[ERROR]%s %s:%d in %s(): ",
           colors ? COLOR_RED : "", colors ? COLOR_RESET : "",
           file, line, func);
    vprintf(message, args);
    printf("\n");

    va_end(args);
}

void test_log_skip(const char* file, int line, const char* func, const char* message, ...) {
    va_list args;
    va_start(args, message);

    int colors = use_colors();
    printf("%s[SKIP]%s %s:%d in %s(): ",
           colors ? COLOR_YELLOW : "", colors ? COLOR_RESET : "",
           file, line, func);
    vprintf(message, args);
    printf("\n");

    va_end(args);
}

/* Print test summary */
void test_print_summary(void) {
    int colors = use_colors();

    printf("\n");
    printf("%s=== Test Summary ===%s\n",
           colors ? COLOR_BLUE : "", colors ? COLOR_RESET : "");
    printf("Total tests:  %d\n", g_test_result.total_tests);
    printf("%sPassed tests: %d%s\n",
           colors ? COLOR_GREEN : "", g_test_result.passed_tests, colors ? COLOR_RESET : "");

    if (g_test_result.failed_tests > 0) {
        printf("%sFailed tests: %d%s\n",
               colors ? COLOR_RED : "", g_test_result.failed_tests, colors ? COLOR_RESET : "");
    } else {
        printf("Failed tests: %d\n", g_test_result.failed_tests);
    }

    if (g_test_result.skipped_tests > 0) {
        printf("%sSkipped tests: %d%s\n",
               colors ? COLOR_YELLOW : "", g_test_result.skipped_tests, colors ? COLOR_RESET : "");
    }

    double pass_rate = g_test_result.total_tests > 0 ?
        (double)g_test_result.passed_tests / g_test_result.total_tests * 100.0 : 0.0;

    printf("Pass rate: %.1f%%\n", pass_rate);

    if (g_test_result.failed_tests == 0) {
        printf("%sAll tests passed!%s\n",
               colors ? COLOR_GREEN : "", colors ? COLOR_RESET : "");
    }
    printf("\n");
}

/* Reset test results */
void test_reset_results(void) {
    memset(&g_test_result, 0, sizeof(g_test_result));
}

/* Memory tracking functions */
void test_memory_start_tracking(void) {
    allocated_bytes = 0;
    tracking_enabled = 1;
}

void test_memory_stop_tracking(void) {
    tracking_enabled = 0;
}

int test_memory_check_leaks(void) {
    if (allocated_bytes != 0) {
        test_log_error(__FILE__, __LINE__, __func__,
                      "Memory leak detected: %zu bytes not freed", allocated_bytes);
        return -1;
    }
    return 0;
}