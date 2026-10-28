# Third-party notices

- **xenosworks/xsdk**, commit `8c58c7a3fb37bb261c5a11a8a1f4fa8a8bfd0d28`:
  `tools/elf2xex.py`, `tools/gen_imports.py`, and the binutils/GCC ABI patches.
  Copyright (c) 2026 Mena, MIT; see `third_party/licenses/xsdk-MIT.txt`.
  The generated compiler and GNU sources retain their GNU licenses.
- **EinTim23/hiddriver360**, commit `6159866ff0049d379238751d50a19409abf08e9d`:
  reverse-engineered USB structures, kernel 17559 entry points and virtual
  controller registration form the basis of the MK360 hardware backend.
  GPL-3.0, https://github.com/EinTim23/hiddriver360.
- **HB-Agadir/x360remap**, commit `fc250bb5c33eeee4dfe8d44f7a168cb6f30047b3`:
  consulted for boot HID handling and known USB lifecycle limitations.
  GPL-3.0, https://github.com/HB-Agadir/x360remap.

MK360's source and hardware backend are distributed under GPL-3.0-or-later.
No Microsoft headers, import libraries, XDK binaries or xbdm binaries are included.
