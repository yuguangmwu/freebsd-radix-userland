#include <stdio.h>
#include <stdint.h>

int main() {
    printf("=== Range 16 Boundary Check ===\n");
    
    /* Check the exact boundary condition for Range 16 */
    printf("Range 16: route_id < 1048576 (0x100000)\n");
    printf("Condition: route_id < 1048575 vs route_id < 1048576\n\n");
    
    /* Check the specific failing route IDs */
    for (uint32_t route_id = 999994; route_id <= 999999; route_id++) {
        printf("Route %u:\n", route_id);
        
        if (route_id < 1048575) {  /* Current condition in code */
            uint32_t offset = route_id - 983040;
            uint32_t major = offset / 256;
            uint32_t minor = offset % 256;
            printf("  Using Range 16: offset=%u, major=%u, minor=%u\n", offset, major, minor);
            printf("  IP would be: 216.%u.%u.0\n", major, minor);
            
            /* Check if this creates a valid address */
            if (major > 255 || minor > 255) {
                printf("  ❌ INVALID: major or minor > 255\!\n");
            } else {
                printf("  ✅ Valid address\n");
            }
        } else {
            printf("  Would use fallback range (240.x)\n");
        }
        printf("\n");
    }
    
    /* Check if the condition should be <= instead of < */
    printf("Analysis: The condition 'route_id < 1048575' should probably be 'route_id < 1048576'\n");
    printf("This would allow route_id 1048575 to use Range 16 instead of falling through.\n");
    
    return 0;
}
