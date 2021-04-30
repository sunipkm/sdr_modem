#!/usr/bin/env python3
import math as m
import sys

def pull_range(loop_bw, damping):
    return 2 * m.sqrt(2) * m.pi * loop_bw * damping

def pull_range_hz(loop_bw, damping, fs):
    return pull_range(loop_bw, damping) * fs

def phase_lock_t(loop_bw, fs):
    return (1.3 / loop_bw / fs)

def freq_lock_t(loop_bw, damping, fs):
    return (32 * damping * damping / loop_bw / fs)

if __name__=='__main__':
    if (len(sys.argv) != 4):
        print("Invocation: calc_timing.py <Loop BW> <Damping Factor> <Sampling Freq>")
        sys.exit(0)
        
    loop_bw = float(sys.argv[1])
    damping = float(sys.argv[2])
    fs = float(sys.argv[3])
    print("Loop bw: ", loop_bw)
    print("Damping: ", damping)
    print("Sampling freq: %.3e Hz"%(fs))
    print("Normalized Pull In Range: %.3f"%(pull_range(loop_bw, damping)))
    print("Pull In Range: %.3e Hz"%(pull_range_hz(loop_bw, damping, fs)))
    print("Phase lock delay: %.3e s"%(phase_lock_t(loop_bw, fs)))
    print("Frequency lock delay: %.3e s"%(freq_lock_t(loop_bw, damping, fs)))

        
