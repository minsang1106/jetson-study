#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>

void print_bytes(void* ptr, size_t size) {
    unsigned char* p = (unsigned char*) ptr;

    for(size_t i = 0; i < size; i++) 
        printf("%02X", p[i]);
    
    printf("\n");
}

int main() {
    uint32_t host_value = 0x12345678;
    uint32_t network_value = htonl(host_value);

    printf("host_value 숫자값    : 0x%08X\n", host_value);
    printf("host_value 메모리    : ");
    print_bytes(&host_value, sizeof(host_value));

    printf("\n");

    printf("htonl 결과 숫자값    : 0x%08X\n", network_value);
    printf("htonl 결과 메모리    : ");
    print_bytes(&network_value, sizeof(network_value));
    
    return 0;
}