GP-200 Sound Clone import prototype

Replace/add these files over source_latest7.

UI:
- New "Sound Clone" button on the utility row.
- Opens a window with AMP Slot 1-5 and DIST Slot 1-5.
- "Import CLO" selects a .clo file.

Validation:
- Requires at least 8192 bytes.
- Requires Valeton/GP-200 VTSI header.
- Hotone HTSI CLO files are rejected as incompatible.

Protocol used (confirmed from captures):
- 28-byte GP-200 wrapper + first 8192 CLO bytes.
- 45 chunks, marker 0x40, nibble encoding, 183 decoded bytes per full chunk.
- Global slot mapping AMP 1-5 => 0-4; DIST 1-5 => 5-9.
- Prepare category byte AMP=0x03, DIST=0x02.
- Commit and checksum match captured transfers.

This prototype was not compiled in this environment. Build and test first with a Valeton-generated VTSI namClo.clo.
