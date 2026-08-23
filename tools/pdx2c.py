#!/usr/bin/env python3
"""Extract ADPCM samples from an MDX PDX file into src/adpcm_smp.c/h.

Usage: pdx2c.py FILE.PDX KICK_NOTE SNARE_NOTE CRASH_NOTE
(note = 0-95 sample slot, as used by the MDX PCM channel)

The PDX header is 96 (offset, length) uint32 big-endian pairs; the data
is already OKI 4-bit ADPCM, so it is embedded as-is. Run this locally
against your own PDX file — extracted sample data is game audio and
should not be committed to a public repository.
"""
import struct
import sys

import adpcm

fn, k, s, c = sys.argv[1], *(int(x) for x in sys.argv[2:5])
data = open(fn, "rb").read()


def sample(note):
    off, ln = struct.unpack_from(">II", data, note * 8)
    return data[off:off + ln]


adpcm.emit_c("src/adpcm_smp.c", "src/adpcm_smp.h", [
    ("adpcm_kick", sample(k)),
    ("adpcm_snare", sample(s)),
    ("adpcm_crash", sample(c)),
])
print("ok (local use only - do not commit extracted data)")
