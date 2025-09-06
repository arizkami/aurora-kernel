#!/usr/bin/env python3
"""Spleen font generator

Generates a C font table (256 x 16 bytes) for the Spleen 8x16 font.

Input can be either:
  - DOS .asm file (original previously used) containing 4096 db rows after the 'spleen:' label.
  - BDF file (spleen-8x16.bdf) containing many Unicode glyphs. We extract codepoints 0..255.

For codepoints missing in the BDF (notably 0..31), we optionally fall back to the DOS ASM
file if it is available; otherwise we emit zero (blank) glyphs for those slots.

License: Resulting table remains under Spleen's BSD 2-Clause license (see upstream headers).
"""
import sys, re, pathlib, io

ASM_LABEL = 'spleen:'
ROW_RE = re.compile(r'^\s*db\s*([01]{8})b')
BDF_STARTCHAR = 'STARTCHAR'
HEADER = """/* Auto-generated Spleen 8x16 font table
 * Source: {source}
 * Generation: tools/gen_spleen.py (BSD 2-Clause upstream font)
 * Do not edit manually. */
"""

def parse_asm(path: pathlib.Path):
    """Parse legacy DOS ASM table (expects 4096 bitmap rows)."""
    if not path.exists():
        return None
    rows = []
    in_table = False
    for line in path.read_text().splitlines():
        if not in_table:
            if line.strip().startswith(ASM_LABEL):
                in_table = True
            continue
        m = ROW_RE.match(line)
        if m:
            rows.append(int(m.group(1), 2))
            if len(rows) == 256 * 16:
                break
    if len(rows) != 256 * 16:
        # Return None instead of aborting so caller can decide.
        return None
    return [rows[i*16:(i+1)*16] for i in range(256)]

def parse_bdf(path: pathlib.Path):
    """Parse BDF file, returning dict codepoint->list[16] (each row byte)."""
    glyphs = {}
    enc = None
    collecting = False
    bitmap_rows = []
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if line.startswith('ENCODING '):
            try:
                enc = int(line.split()[1])
            except Exception:
                enc = None
        elif line == 'BITMAP':
            collecting = True
            bitmap_rows = []
        elif line == 'ENDCHAR':
            if enc is not None and collecting:
                # Use only first 16 rows (font height) or pad if fewer.
                rows = [int(r, 16) & 0xFF for r in bitmap_rows[:16]]
                while len(rows) < 16:
                    rows.append(0)
                glyphs[enc] = rows
            enc = None
            collecting = False
            bitmap_rows = []
        elif collecting:
            if re.fullmatch(r'[0-9A-Fa-f]{2}', line):
                bitmap_rows.append(line)
    return glyphs

def build_glyph_table(src_path: pathlib.Path, maybe_asm: pathlib.Path):
    ext = src_path.suffix.lower()
    if ext == '.bdf':
        bdf_glyphs = parse_bdf(src_path)
        asm_fallback = parse_asm(maybe_asm) if maybe_asm else None
        table = []
        for cp in range(256):
            if cp in bdf_glyphs:
                table.append(bdf_glyphs[cp])
            elif asm_fallback and 0 <= cp < len(asm_fallback):
                table.append(asm_fallback[cp])
            else:
                table.append([0]*16)
        return table, f"{src_path} (BDF)" + (" + ASM fallback" if asm_fallback else "")
    elif ext == '.asm':
        table = parse_asm(src_path)
        if table is None:
            raise SystemExit('Failed to parse ASM font table')
        return table, f"{src_path} (ASM)"
    else:
        raise SystemExit(f"Unsupported source extension: {ext}")

def emit_c(glyphs, out_path: pathlib.Path, source_desc: str):
    with out_path.open('w', newline='\n') as f:
        f.write(HEADER.format(source=source_desc))
        f.write('#include "../aurora.h"\n#include "../include/font.h"\n\n')
        f.write('const unsigned char g_Spleen8x16[AURORA_FONT_GLYPHS][AURORA_FONT_HEIGHT] = {\n')
        for idx, g in enumerate(glyphs):
            f.write(f'    /* 0x{idx:02X} */ {{{', '.join(f'0x{b:02X}' for b in g)}}},\n')
        f.write('};\nBOOL FontInitialize(void){ return TRUE; }\n')

def main():
    if len(sys.argv) < 3:
        print('Usage: gen_spleen.py <spleen.{asm|bdf}> <out.c> [fallback_asm]')
        return 1
    src = pathlib.Path(sys.argv[1])
    out = pathlib.Path(sys.argv[2])
    fallback = pathlib.Path(sys.argv[3]) if len(sys.argv) > 3 else pathlib.Path('external/spleen/dos/spleen.asm')
    glyphs, desc = build_glyph_table(src, fallback)
    emit_c(glyphs, out, desc)
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
