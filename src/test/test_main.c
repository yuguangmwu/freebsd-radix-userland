/*
 * Main test runner for FreeBSD Routing Library
 */

#include "test_framework.h"

/* External test suite declarations */
extern test_suite_t radix_test_suite;
extern test_suite_t route_table_test_suite;
extern test_suite_t freebsd_integration_test_suite;

/* Function to count tests in a suite */
static void count_tests_in_suite(test_suite_t* suite) {
    int count = 0;
    if (suite->tests) {
        while (suite->tests[count].name != NULL) {
            count++;
        }
    }
    suite->num_tests = count;
}

int main(int argc, char* argv[]) {
    printf("FreeBSD Routing Library Test Suite\n");
    printf("==================================\n\n");

    /* Initialize test framework */
    if (test_framework_init() != 0) {
        fprintf(stderr, "Failed to initialize test framework\n");
        return 1;
    }

    /* Count tests in each suite */
    count_tests_in_suite(&radix_test_suite);
    count_tests_in_suite(&route_table_test_suite);
    count_tests_in_suite(&freebsd_integration_test_suite);

    /* Define test suites to run */
    test_suite_t* test_suites[] = {
        &radix_test_suite,
        &route_table_test_suite,
        &freebsd_integration_test_suite
        /* Add more test suites here as they're implemented */
    };

    int num_suites = sizeof(test_suites) / sizeof(test_suites[0]);

    /* Check command line arguments */
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --help, -h     Show this help message\n");
            printf("  --list         List all available tests\n");
            printf("  --verbose, -v  Enable verbose output\n");
            printf("\n");
            printf("Available test suites:\n");

            for (int i = 0; i < num_suites; i++) {
                printf("  %s: %s (%d tests)\n",
                       test_suites[i]->name,
                       test_suites[i]->description,
                       test_suites[i]->num_tests);
            }
            return 0;
        }

        if (strcmp(argv[1], "--list") == 0) {
            printf("Available tests:\n\n");
            for (int i = 0; i < num_suites; i++) {
                test_suite_t* suite = test_suites[i];
                printf("%s:\n", suite->name);

                for (int j = 0; suite->tests[j].name != NULL; j++) {
                    test_case_t* test = &suite->tests[j];
                    printf("  %s - %s %s\n",
                           test->name,
                           test->description,
                           test->enabled ? "" : "(DISABLED)");
                }
                printf("\n");
            }
            return 0;
        }
    }

    /* Run all test suites */
    int result = test_run_all_suites(test_suites, num_suites);

    /* Print summary */
    test_print_summary();

    /* Clean up test framework */
    test_framework_cleanup();

    /* Return appropriate exit code */
    if (result != 0 || g_test_result.failed_tests > 0) {
        return 1;
    }

    return 0;
}