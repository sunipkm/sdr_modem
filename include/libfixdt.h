/**
 * @file libfixdt.h
 * @author Sunip K. Mukherjee (sunipkmukherjee@gmail.com)
 * @brief Fixed point data conversion library (header only)
 * @version 0.1
 * @date 2021-04-30
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef _LIB_FIXDT_H
#define _LIB_FIXDT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <math.h>

static inline int fsgn(float f)
{
    return f < 0 ? -1 : (f > 0 ? 1 : 0);
}

static inline uint32_t convert_ufixdt(float num, int tot_bits, int frac_bits)
{
    // check the obvious
    if (tot_bits == 0)
        return 0;
    if (num <= 0)
        return 0;
    // check boundaries
    int max_int_bits = tot_bits - frac_bits;
    if (!(num < pow(2.0, max_int_bits)))
        return 0xffffffff;
    uint32_t out = 0, mask = 0;
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
    // mask all bits but tot_bits
    while(tot_bits--)
    {
        mask <<= 1;
        mask |= 0x1;
    }
    return (out & mask);
}

static inline uint32_t convert_sfixdt(float num, int tot_bits, int frac_bits)
{
    if (tot_bits == 0)
        return 0;
    if (num == 0)
        return 0;
    // create unsigned versions
    int sgn = fsgn(num);
    float num_ = fabs(num);
    int max_int_bits = tot_bits - frac_bits;
    uint32_t out = 0, mask = 0;
    if (num_ >= pow(2, max_int_bits - 1))
    {
        if (sgn == -1)
            out = 0x1 << (tot_bits - 1);
        else
            out = ~(0x1 << (tot_bits - 1));
        return out;
    }
    out = convert_ufixdt(num_, tot_bits, frac_bits); // create the unsigned portion
    if (sgn == -1)
    {
        out = ~out;
        out += 1; // 2's complement
    }
    // mask all bits but tot_bits
    while(tot_bits--)
    {
        mask <<= 1;
        mask |= 0x1;
    }
    return (out & mask);
}
#ifdef __cplusplus
}
#endif
#endif