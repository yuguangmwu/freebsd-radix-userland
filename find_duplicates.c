#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

/* Simulate the exact route key generation logic */
uint32_t generate_route_addr(uint32_t route_id) {
    if (route_id < 65536) {
        return 0x0A000000 | (route_id << 8);
    } else if (route_id < 131072) {
        uint32_t offset = route_id - 65536;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xAC000000 | (major << 16) | (minor << 8);
    } else if (route_id < 196608) {
        uint32_t offset = route_id - 131072;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xC1000000 | (major << 16) | (minor << 8);
    } else if (route_id < 262144) {
        uint32_t offset = route_id - 196608;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xCC000000 | (major << 16) | (minor << 8);
    } else if (route_id < 327680) {
        uint32_t offset = route_id - 262144;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xCD000000 | (major << 16) | (minor << 8);
    } else if (route_id < 393216) {
        uint32_t offset = route_id - 327680;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xCE000000 | (major << 16) | (minor << 8);
    } else if (route_id < 458752) {
        uint32_t offset = route_id - 393216;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xCF000000 | (major << 16) | (minor << 8);
    } else if (route_id < 524288) {
        uint32_t offset = route_id - 458752;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xD0000000 | (major << 16) | (minor << 8);
    } else if (route_id < 589823) {  /* 524288 + 65536 */
        uint32_t offset = route_id - 524288;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xD1000000 | (major << 16) | (minor << 8);
    } else if (route_id < 655359) {  /* 589823 + 65536 */
        uint32_t offset = route_id - 589824;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xD2000000 | (major << 16) | (minor << 8);
    } else if (route_id < 720895) {  /* 655359 + 65536 */
        uint32_t offset = route_id - 655360;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xD3000000 | (major << 16) | (minor << 8);
    } else if (route_id < 786431) {  /* 720895 + 65536 */
        uint32_t offset = route_id - 720896;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xD4000000 | (major << 16) | (minor << 8);
    } else if (route_id < 851967) {  /* 786431 + 65536 */
        uint32_t offset = route_id - 786432;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xD5000000 | (major << 16) | (minor << 8);
    } else if (route_id < 917503) {  /* 851967 + 65536 */
        uint32_t offset = route_id - 851968;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xD6000000 | (major << 16) | (minor << 8);
    } else if (route_id < 983039) {  /* 917503 + 65536 */
        uint32_t offset = route_id - 917504;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xD7000000 | (major << 16) | (minor << 8);
    } else if (route_id < 1048575) {  /* 983039 + 65536 */
        uint32_t offset = route_id - 983040;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        return 0xD8000000 | (major << 16) | (minor << 8);
    } else {
        uint32_t range = route_id / 65536;
        uint32_t offset = route_id % 65536;
        return 0xF0000000 | ((range & 0xFF) << 16) | (offset << 8);
    }
}

int main() {
    printf("=== Duplicate Address Detection ===\n\n");
    
    /* Check for duplicates around the failure zone */
    printf("Checking addresses for route IDs around the failure point:\n");
    
    for (uint32_t route_id = 999990; route_id <= 999999; route_id++) {
        uint32_t addr = generate_route_addr(route_id);
        struct in_addr in_addr = { .s_addr = htonl(addr) };
        printf("Route %u: %s (0x%08X)\n", route_id, inet_ntoa(in_addr), addr);
    }
    
    /* Check if there are any obvious off-by-one errors in boundaries */
    printf("\n=== Boundary Check ===\n");
    
    uint32_t test_routes[] = {
        589823, 589824,   /* Range 9-10 boundary */
        655359, 655360,   /* Range 10-11 boundary */  
        720895, 720896,   /* Range 11-12 boundary */
        786431, 786432,   /* Range 12-13 boundary */
        851967, 851968,   /* Range 13-14 boundary */
        917503, 917504,   /* Range 14-15 boundary */
        983039, 983040,   /* Range 15-16 boundary */
        1048575, 1048576  /* Range 16-fallback boundary */
    };
    
    printf("Boundary addresses:\n");
    for (int i = 0; i < sizeof(test_routes)/sizeof(test_routes[0]); i++) {
        uint32_t route_id = test_routes[i];
        uint32_t addr = generate_route_addr(route_id);
        struct in_addr in_addr = { .s_addr = htonl(addr) };
        printf("Route %u: %s\n", route_id, inet_ntoa(in_addr));
    }
    
    /* Look for obvious duplicates */
    printf("\n=== Checking for Duplicates ===\n");
    
    /* Check a few critical boundaries where we might have off-by-one errors */
    struct {
        uint32_t route_id;
        const char* description;
    } critical_routes[] = {
        {589823, "Last route of Range 9"},
        {589824, "First route of Range 10"},
        {655359, "Last route of Range 10"},
        {655360, "First route of Range 11"},
        {1048574, "Near end of Range 16"},
        {1048575, "Last possible route in Range 16"},
        {1048576, "First fallback route"}
    };
    
    for (int i = 0; i < sizeof(critical_routes)/sizeof(critical_routes[0]); i++) {
        uint32_t route_id = critical_routes[i].route_id;
        uint32_t addr = generate_route_addr(route_id);
        struct in_addr in_addr = { .s_addr = htonl(addr) };
        printf("%s (ID %u): %s\n", 
               critical_routes[i].description, route_id, inet_ntoa(in_addr));
    }
    
    return 0;
}
