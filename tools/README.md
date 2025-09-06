# Tools

## gen_spleen.py

Generates the Spleen 8x16 VGA font table used by the kernel (`g_Spleen8x16`).

Primary input is the BDF file: `external/spleen/spleen-8x16.bdf` (Unicode capable).
For codepoints 0..255 we extract the first 256 glyphs by encoding value. If a glyph
is missing in the BDF for a codepoint < 256, the script optionally falls back to
the DOS ASM file `external/spleen/dos/spleen.asm` (which contains exactly 256 glyphs).

Invocation is automatic via the Makefile rule for `kern/font_spleen.c` when Python 3
is available in PATH:

```
make all
```

Manual usage:

```
python3 tools/gen_spleen.py external/spleen/spleen-8x16.bdf kern/font_spleen.c external/spleen/dos/spleen.asm
```

Output: `kern/font_spleen.c` (regenerated whenever inputs change).

License: The resulting table remains under the font's BSD 2-Clause license (see upstream files).
