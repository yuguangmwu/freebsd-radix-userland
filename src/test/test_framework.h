/*
 * FreeBSD Routing Library Test Framework
 */

#ifndef _TEST_FRAMEWORK_H_
#define _TEST_FRAMEWORK_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Test result tracking */
typedef struct test_result {
    int total_tests;
    int passed_tests;
    int failed_tests;
    int skipped_tests;
} test_result_t;

/* Test function prototype */
typedef int (*test_func_t)(void);

/* Test case structure */
typedef struct test_case {
    const char* name;
    const char* description;
    test_func_t func;
    int enabled;
} test_case_t;

/* Test suite structure */
typedef struct test_suite {
    const char* name;
    const char* description;
    test_case_t* tests;
    int num_tests;
    int (*setup)(void);      /* Optional setup function */
    int (*teardown)(void);   /* Optional teardown function */
} test_suite_t;

/* Global test result */
extern test_result_t g_test_result;

/* Test macros */
#define TEST_ASSERT(condition, message, ...) \
    do { \
        if (!(condition)) { \
            test_log_error(__FILE__, __LINE__, __func__, message, ##__VA_ARGS__); \
            return -1; \
        } \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual, message, ...) \
    TEST_ASSERT((expected) == (actual), message, ##__VA_ARGS__)

#define TEST_ASSERT_NE(not_expected, actual, message, ...) \
    TEST_ASSERT((not_expected) != (actual), message, ##__VA_ARGS__)

#define TEST_ASSERT_NULL(ptr, message, ...) \
    TEST_ASSERT((ptr) == NULL, message, ##__VA_ARGS__)

#define TEST_ASSERT_NOT_NULL(ptr, message, ...) \
    TEST_ASSERT((ptr) != NULL, message, ##__VA_ARGS__)

#define TEST_ASSERT_STR_EQ(expected, actual, message, ...) \
    TEST_ASSERT(strcmp(expected, actual) == 0, message, ##__VA_ARGS__)

#define TEST_PASS() return 0
#define TEST_FAIL(message, ...) \
    do { \
        test_log_error(__FILE__, __LINE__, __func__, message, ##__VA_ARGS__); \
        return -1; \
    } while (0)

#define TEST_SKIP(message, ...) \
    do { \
        test_log_skip(__FILE__, __LINE__, __func__, message, ##__VA_ARGS__); \
        return 1; \
    } while (0)

/* Test case definition helpers */
#define TEST_CASE(name, desc, func) \
    { #name, desc, func, 1 }

#define TEST_CASE_DISABLED(name, desc, func) \
    { #name, desc, func, 0 }

#define TEST_SUITE_END() \
    { NULL, NULL, NULL, 0 }

/* Performance testing */
typedef struct perf_timer {
    struct timeval start;
    struct timeval end;
    double elapsed_ms;
} perf_timer_t;

#define PERF_START(timer) \
    gettimeofday(&(timer)->start, NULL)

#define PERF_END(timer) \
    do { \
        gettimeofday(&(timer)->end, NULL); \
        (timer)->elapsed_ms = ((timer)->end.tv_sec - (timer)->start.tv_sec) * 1000.0 + \
                              ((timer)->end.tv_usec - (timer)->start.tv_usec) / 1000.0; \
    } while (0)

/* Test framework functions */
int test_framework_init(void);
void test_framework_cleanup(void);

int test_run_suite(const test_suite_t* suite);
int test_run_all_suites(test_suite_t* suites[], int num_suites);

void test_log_info(const char* message, ...);
void test_log_error(const char* file, int line, const char* func, const char* message, ...);
void test_log_skip(const char* file, int line, const char* func, const char* message, ...);

void test_print_summary(void);
void test_reset_results(void);

/* Memory leak detection helpers */
void test_memory_start_tracking(void);
void test_memory_stop_tracking(void);
int test_memory_check_leaks(void);

#ifdef __cplusplus
}
#endif

#endif /* _TEST_FRAMEWORK_H_ */