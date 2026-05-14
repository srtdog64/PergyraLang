#include <stdint.h>
#include <stdio.h>

int main(void)
{
    int32_t acc = 0;
    int32_t i = 0;

    while (i < 10000000) {
        acc = acc + (i % 97);
        acc = acc + (i / 97);
        i = i + 1;
    }

    printf("%d\n", acc);
    return 0;
}
