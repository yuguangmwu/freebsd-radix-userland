/*
 * Ultra-Minimal Radix Test - Phase 1 Proof of Concept
 *
 * This is the absolute simplest test to verify our kernel compatibility
 * layer is working. We'll build up from here.
 */

#include "test_framework.h"
#include "../kernel_compat/compat_shim.h"

/* Test that our compatibility layer works */
static int test_compat_basic(void) {
    void* ptr;

    /* Test memory allocation */
    ptr = bsd_malloc(100, M_RTABLE, M_WAITOK);
    TEST_ASSERT_NOT_NULL(ptr, "Should allocate memory with bsd_malloc");

    bsd_free(ptr, M_RTABLE);

    /* Test zero allocation */
    ptr = bsd_malloc(50, M_RTABLE, M_WAITOK | M_ZERO);
    TEST_ASSERT_NOT_NULL(ptr, "Should allocate zeroed memory");

    /* Check that it's actually zeroed */
    char* cptr = (char*)ptr;
    for (int i = 0; i < 50; i++) {
        TEST_ASSERT_EQ(0, cptr[i], "Memory should be zeroed at position %d", i);
    }

    bsd_free(ptr, M_RTABLE);

    TEST_PASS();
}

static int test_compat_types(void) {
    /* Test that our type definitions work */
    u_char uc = 255;
    u_int ui = 0xFFFFFFFF;
    u_long ul = 0xFFFFFFFF;
    caddr_t ca = (caddr_t)"test";

    TEST_ASSERT_EQ(255, uc, "u_char should work");
    TEST_ASSERT_EQ(0xFFFFFFFF, ui, "u_int should work");
    TEST_ASSERT_EQ(0xFFFFFFFF, ul, "u_long should work");
    TEST_ASSERT_NOT_NULL(ca, "caddr_t should work");

    TEST_PASS();
}

static int test_compat_kernel_calls(void) {
    /* Test kernel compatibility functions */
    kernel_compat_init();

    /* Test KASSERT (should not crash) */
    KASSERT(1 == 1, ("Basic assertion should pass"));

    /* Test no-op operations don't crash */
    struct rmlock lock;
    struct rm_priotracker tracker;

    rm_init_flags(&lock, "test", RM_DUPOK);
    rm_rlock(&lock, &tracker);
    rm_runlock(&lock, &tracker);
    rm_destroy(&lock);

    TEST_PASS();
}

static int test_socket_address(void) {
    struct sockaddr_in addr1, addr2;

    /* Set up identical addresses */
    memset(&addr1, 0, sizeof(addr1));
    memset(&addr2, 0, sizeof(addr2));

    addr1.sin_family = AF_INET;
    addr1.sin_len = sizeof(addr1);
    inet_pton(AF_INET, "192.168.1.1", &addr1.sin_addr);

    addr2.sin_family = AF_INET;
    addr2.sin_len = sizeof(addr2);
    inet_pton(AF_INET, "192.168.1.1", &addr2.sin_addr);

    /* Test socket address comparison */
    int equal = sa_equal((struct sockaddr*)&addr1, (struct sockaddr*)&addr2);
    TEST_ASSERT_NE(0, equal, "Identical socket addresses should be equal");

    /* Test different addresses */
    inet_pton(AF_INET, "192.168.1.2", &addr2.sin_addr);
    equal = sa_equal((struct sockaddr*)&addr1, (struct sockaddr*)&addr2);
    TEST_ASSERT_EQ(0, equal, "Different socket addresses should not be equal");

    TEST_PASS();
}

/* Test suite definition */
static test_case_t compat_tests[] = {
    TEST_CASE(compat_basic,
              "Test basic memory allocation compatibility",
              test_compat_basic),

    TEST_CASE(compat_types,
              "Test FreeBSD type compatibility",
              test_compat_types),

    TEST_CASE(compat_kernel_calls,
              "Test kernel function compatibility",
              test_compat_kernel_calls),

    TEST_CASE(socket_address,
              "Test socket address operations",
              test_socket_address),

    TEST_SUITE_END()
};

test_suite_t compat_test_suite = {
    "Kernel Compatibility Tests",
    "Test suite for FreeBSD kernel compatibility layer",
    compat_tests,
    0,  /* num_tests calculated at runtime */
    NULL, /* setup */
    NULL  /* teardown */
};

/* Main test runner */
int main(int argc, char* argv[]) {
    printf("FreeBSD Kernel Compatibility Test Suite\n");
    printf("=======================================\n\n");

    /* Initialize test framework */
    if (test_framework_init() != 0) {
        fprintf(stderr, "Failed to initialize test framework\n");
        return 1;
    }

    /* Count tests */
    int count = 0;
    while (compat_tests[count].name != NULL) {
        count++;
    }
    compat_test_suite.num_tests = count;

    /* Handle command line arguments */
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --help, -h     Show this help message\n");
            printf("  --verbose, -v  Enable verbose output\n");
            printf("\n");
            printf("This tests the kernel compatibility layer.\n");
            return 0;
        }
    }

    /* Run the test suite */
    int result = test_run_suite(&compat_test_suite);

    /* Print summary */
    test_print_summary();

    /* Clean up test framework */
    test_framework_cleanup();

    /* Return appropriate exit code */
    if (result != 0 || g_test_result.failed_tests > 0) {
        return 1;
    }

    printf("\n✅ Kernel compatibility layer is working!\n");
    printf("Next step: Integrate with FreeBSD radix tree code.\n");

    return 0;
}