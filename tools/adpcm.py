"""OKI/MSM6258-style 4-bit ADPCM encoder (shared by gen_drums.py / pdx2c.py)."""

STEP = [
    16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253,
    279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876,
    963, 1060, 1166, 1282, 1411, 1552,
]
INDEX_ADJ = [-1, -1, -1, -1, 2, 4, 6, 8]


def encode(samples):
    """samples: iterable of 16-bit signed ints -> bytes (two nibbles/byte, low first)"""
    pred, idx = 0, 0
    nibbles = []
    for s in samples:
        step = STEP[idx]
        diff = s - pred
        code = 0
        if diff < 0:
            code = 8
            diff = -diff
        if diff >= step:
            code |= 4
            diff -= step
        if diff >= step >> 1:
            code |= 2
            diff -= step >> 1
        if diff >= step >> 2:
            code |= 1
        delta = step >> 3
        if code & 1:
            delta += step >> 2
        if code & 2:
            delta += step >> 1
        if code & 4:
            delta += step
        if code & 8:
            pred -= delta
        else:
            pred += delta
        pred = max(-2048, min(2047, pred))
        idx = max(0, min(48, idx + INDEX_ADJ[code & 7]))
        nibbles.append(code)
    if len(nibbles) & 1:
        nibbles.append(0)
    return bytes(nibbles[i] | (nibbles[i + 1] << 4) for i in range(0, len(nibbles), 2))


def emit_c(path_c, path_h, names_data):
    with open(path_h, "w") as f:
        f.write("/* Generated - do not edit */\n#ifndef ADPCM_SMP_H\n"
                "#define ADPCM_SMP_H\n#include <stdint.h>\n")
        for name, data in names_data:
            f.write(f"extern const uint8_t {name}[];\n")
            f.write(f"#define {name.upper()}_LEN {len(data)}\n")
        f.write("#endif\n")
    with open(path_c, "w") as f:
        f.write('/* Generated - do not edit */\n#include "adpcm_smp.h"\n\n')
        for name, data in names_data:
            f.write(f"const uint8_t {name}[] = {{\n")
            for i in range(0, len(data), 12):
                f.write("    " + ",".join(f"0x{b:02X}" for b in data[i:i+12]) + ",\n")
            f.write("};\n\n")
