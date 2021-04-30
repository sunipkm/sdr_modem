#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define eprintf(str, ...) \
    fprintf(stderr, "%s, %d: " str "\n", __func__, __LINE__, ##__VA_ARGS__); \
    fflush(stderr)

#include "libfixdt.h"

char *print_bits(uint32_t num, int tot_bits)
{
    static char buf[33];
    memset(buf, 0x0, 33);
    for (int i = 0; tot_bits > 0; i++)
    {
        tot_bits--;
        buf[i] = (num >> tot_bits) & 0x1 ? '1' : '0';
    }
    return buf;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Invocation: ./fixdt.out <Num> <Signed> <Total Bits> <Fractional Bits>\n\n");
        return 0;
    }
    float val = atof(argv[1]);
    int sgn = atoi(argv[2]);
    int tot_bits = atoi(argv[3]);
    int frac_bits = atoi(argv[4]);
    uint32_t out = sgn > 0 ? convert_sfixdt(val, tot_bits, frac_bits) : convert_ufixdt(val, tot_bits, frac_bits);
    printf("Float: %f, Integer representation: %s\n", val, print_bits(out, tot_bits));
    return 0;
}