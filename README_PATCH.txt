GP-200 preset slot browser - progress/click/fallback correction

Changes:
- The slot button always keeps its slot text; scan progress is reported in the existing status text.
- Only the dedicated slot-number TextButton starts the browser.
- The next-preset arrow explicitly cancels a scan and only changes preset.
- Fast name read 0x20 is probed once.
- If 0x20 times out, the scanner retries that same slot with the compatible full 0x10 read.
- Full-read trailing chunks are given time to drain before the next request.
- Offset decoding corrected to little-endian 16-bit (high byte << 8).

Replace the files preserving their paths, delete the build directory, reconfigure and rebuild.
