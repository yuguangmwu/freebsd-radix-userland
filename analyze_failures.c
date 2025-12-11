#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>

/* Calculate the exact route IDs that would fail */
int main() {
    printf("=== Route ID Range Analysis ===\n\n");
    
    /* Calculate address ranges */
    printf("Address Range Allocation:\n");
    printf("Range 1:  0 to 65535      (65,536 routes) - 10.x.x.0\n");
    printf("Range 2:  65536 to 131071  (65,536 routes) - 172.x.x.0\n");
    printf("Range 3:  131072 to 196607 (65,536 routes) - 193.x.x.0\n");
    printf("Range 4:  196608 to 262143 (65,536 routes) - 204.x.x.0\n");
    printf("Range 5:  262144 to 327679 (65,536 routes) - 205.x.x.0\n");
    printf("Range 6:  327680 to 393215 (65,536 routes) - 206.x.x.0\n");
    printf("Range 7:  393216 to 458751 (65,536 routes) - 207.x.x.0\n");
    printf("Range 8:  458752 to 524287 (65,536 routes) - 208.x.x.0\n");
    printf("Range 9:  524288 to 589823 (65,536 routes) - 209.x.x.0\n");
    printf("Range 10: 589824 to 655359 (65,536 routes) - 210.x.x.0\n");
    printf("Range 11: 655360 to 720895 (65,536 routes) - 211.x.x.0\n");
    printf("Range 12: 720896 to 786431 (65,536 routes) - 212.x.x.0\n");
    printf("Range 13: 786432 to 851967 (65,536 routes) - 213.x.x.0\n");
    printf("Range 14: 851968 to 917503 (65,536 routes) - 214.x.x.0\n");
    printf("Range 15: 917504 to 983039 (65,536 routes) - 215.x.x.0\n");
    printf("Range 16: 983040 to 1048575 (65,536 routes) - 216.x.x.0\n");
    
    printf("\nTotal explicit ranges: %d routes\n", 16 * 65536);
    printf("Remaining for 1M: %d routes (would use fallback 240.x pattern)\n", 1000000 - (16 * 65536));
    
    /* The test added 999,994 routes successfully */
    printf("\n=== Failure Analysis ===\n");
    printf("Test results: 999,994/1,000,000 routes added\n");
    printf("Failed routes: 6\n");
    
    /* The failures likely happen in the last range or fallback */
    printf("\nLikely failed route IDs (last 10 of 1M):\n");
    for (int i = 999990; i < 1000000; i++) {
        if (i >= 983040) {
            if (i < 1048576) {
                uint32_t offset = i - 983040;
                uint32_t major = offset / 256;
                uint32_t minor = offset % 256;
                printf("Route %d: 216.%u.%u.0 (Range 16)\n", i, major, minor);
            } else {
                uint32_t range = i / 65536;
                uint32_t offset = i % 65536;
                printf("Route %d: 240.%u.x.0 (Fallback, range=%u, offset=%u)\n", 
                       i, range & 0xFF, range, offset);
            }
        }
    }
    
    /* Check if there's an off-by-one in the ranges */
    printf("\nChecking boundary conditions:\n");
    printf("Route 983039 (last of Range 15): should be 215.255.255.0\n");
    printf("Route 983040 (first of Range 16): should be 216.0.0.0\n");
    
    uint32_t test_id = 983040;
    uint32_t offset = test_id - 983040;
    uint32_t major = offset / 256;
    uint32_t minor = offset % 256;
    printf("Route 983040 calculation: offset=%u, major=%u, minor=%u -> 216.%u.%u.0\n", 
           offset, major, minor, major, minor);
    
    /* Check what happens at 1048576+ (the fallback range) */
    printf("\nFallback range (route IDs 1048576+):\n");
    for (int i = 1048576; i < 1048580; i++) {
        uint32_t range = i / 65536;
        uint32_t offset = i % 65536;
        printf("Route %d: range=%u, offset=%u -> 240.%u.x.0\n", 
               i, range, offset, range & 0xFF);
    }
    
    return 0;
}
