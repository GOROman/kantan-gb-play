#!/usr/bin/env python3
"""Synthesize original kick/snare/crash one-shots and encode them as
OKI 4-bit ADPCM (MSM6258 style, 15.6 kHz) into src/adpcm_smp.c/h."""
import math
import random

import adpcm

RATE = 15625


def kick():
    out = []
    phase = 0.0
    for i in range(int(RATE * 0.14)):
        t = i / RATE
        f = 120.0 * math.exp(-t * 18) + 45.0          # pitch sweep down
        phase += 2 * math.pi * f / RATE
        env = math.exp(-t * 28)
        out.append(int(2000 * env * math.sin(phase)))
    return out


def snare(seed=1):
    rnd = random.Random(seed)
    out = []
    phase = 0.0
    for i in range(int(RATE * 0.15)):
        t = i / RATE
        phase += 2 * math.pi * 190.0 / RATE
        tone = math.sin(phase) * math.exp(-t * 40)
        noise = rnd.uniform(-1, 1) * math.exp(-t * 22)
        out.append(int(1800 * (0.4 * tone + 0.8 * noise)))
    return out


def crash(seed=2):
    rnd = random.Random(seed)
    out = []
    hp = 0.0
    for i in range(int(RATE * 0.45)):
        t = i / RATE
        n = rnd.uniform(-1, 1)
        hp = 0.7 * hp + n - 0.7 * n                   # crude high-pass shimmer
        v = (0.6 * n + 0.6 * hp) * math.exp(-t * 7)
        out.append(int(1700 * v))
    return out


adpcm.emit_c("src/adpcm_smp.c", "src/adpcm_smp.h", [
    ("adpcm_kick", adpcm.encode(kick())),
    ("adpcm_snare", adpcm.encode(snare())),
    ("adpcm_crash", adpcm.encode(crash())),
])
print("ok")
