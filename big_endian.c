#include <stdint.h>
#include <stdio.h>

int big_endian() {
    uint16_t a = 0xFF00;
    uint8_t *c = (uint8_t *)&a;
    return *c == 0xFF;
}

int main() {
    if(big_endian()) {
        printf("big_endian\n");
    }
    else {
        printf("little_endian\n");
    }
}
