#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define eprintf(str, ...) \
    fprintf(stderr, "%s, %d: " str "\n", __func__, __LINE__, ##__VA_ARGS__); \
    fflush(stderr)

static inline int fsgn(float f)
{
    return f < 0 ? -1 : (f > 0 ? 1 : 0);
}

static inline uint32_t convert_sfixdt(float num, int tot_bits, int frac_bits)
{
    if (tot_bits == 0)
        return 0;
    if (num == 0)
        return 0;
    if (tot_bits < frac_bits)
    {
        eprintf("Total number of bits %d < fractional bits %d", tot_bits, frac_bits);
        return 0;
    }
    // check boundaries
    int max_int_bits = tot_bits - frac_bits - 1;
    while(max_int_bits < 0)
    {
        frac_bits--;
        max_int_bits = tot_bits - frac_bits - 1;
    }
    int max_chk_bits = max_int_bits < 1 ? 1 : max_int_bits;
    int sgn = fsgn(num);
    float num_ = fabs(num);
    if (num_ >= ((0x1 << (max_chk_bits - 1))))
    {
        return sgn > 0 ? (0x1 << (tot_bits - 1)) - 1 : 0x1 << (tot_bits - 1);
    }
    uint32_t out = 0;
    out |= ((int) num_) << frac_bits;
    num_ -= ((int) num_);
    for (int i = 0; frac_bits > 0; i++)
    {
        frac_bits--;
        num_ *= 2;
        if (num_ > 1)
        {
            num_ -= 1;
            out |= 0x1 << frac_bits;
        }
    }
    if (sgn == -1)
    {
        out = ~out;
        out += 1; // 2's complement
    }
    return out;
}

static inline uint32_t convert_ufixdt(float num, int tot_bits, int frac_bits)
{
    if (tot_bits == 0)
        return 0;
    if (num <= 0)
        return 0;
    if (tot_bits < frac_bits)
    {
        eprintf("Total number of bits %d < fractional bits %d", tot_bits, frac_bits);
        return 0;
    }
    // check boundaries
    int max_int_bits = tot_bits - frac_bits;
    while(max_int_bits < 0)
    {
        frac_bits--;
        max_int_bits = tot_bits - frac_bits;
    }
    int max_chk_bits = max_int_bits < 2 ? 2 : max_int_bits;
    if (num >= ((0x1 << (max_chk_bits - 1))))
    {
        return 0xffffffff;
    }
    uint32_t out = 0;
    out |= ((int) num) << frac_bits;
    num -= ((int) num);
    for (int i = 0; frac_bits > 0; i++)
    {
        frac_bits--;
        num *= 2;
        if (num > 1)
        {
            num -= 1;
            out |= 0x1 << frac_bits;
        }
    }
    return out;
}

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
        printf("Invocation: ./test_fixdt.out <Num> <Signed> <Total Bits> <Fractional Bits>\n\n");
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