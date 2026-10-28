# Hardware validation status

No Xbox 360 console has been available in the development environment.
All real-device behavior is **unverified** for MK360 0.1.

## Completed validation

- Built GNU binutils 2.47 and GCC 15.3.0 with the pinned xsdk ABI patches.
- Compiled C17 code with the Xbox 360 calling convention, big-endian PowerPC
  instructions, 64-bit preserved GPRs, and no XDK or C runtime dependency.
- Linked ELF with no unresolved symbols and packed a real XEX2 DLL.
- Native AddressSanitizer/UndefinedBehaviorSanitizer mapping/configuration
  tests, including malformed reports, rollover, timer wrap, sensitivity,
  disconnect release and deterministic fuzz inputs.
- Independent PE/XEX metadata, compression, import and page digest checks;
  deliberate corruption and truncation are rejected.

## Still requires a console

1. Loader acceptance of the xsdk-derived packer's **DLL adaptation**. The
   upstream packer was designed/tested for titles; it does not claim DLL support.
2. DashLaunch lifetime across title transitions, and system-thread behavior.
3. Kernel/XAM entry point fingerprints, page protections and instruction cache
   visibility across all CPU cores. Only USB add/remove have fixed fingerprint
   checks; other export first instructions are checked for relocatability.
4. Boot-protocol USB transfers, actual byte counts, callbacks and error paths.
5. Registration as one XAM player, and re-registration when opening a game.
6. COD timing, aiming, deadzones and tuning for an identified game/title update.
7. Full endpoint cancellation/close/remove sequencing. The current backend
   deliberately quarantines static transfer memory until reboot. It does not
   claim resource-complete hotplug support; eight lifetime sessions are allowed.
8. Removing the xbdm prerequisite only after the cause of the upstream
   standalone USB freeze has been isolated on hardware.
9. Relocatable system images, more mouse formats/NKRO and a configuration UI.

No console NAND or flash is written. Kernel entry points are patched in RAM.
There is no game-memory camera patch, automation of firing, game-specific
address table, account access or network service in this build.

## Architecture

`DllMain` starts an `EX_CREATE_FLAG_SYSTEM` worker and returns. After checking
the kernel, recovery bypass and required module, it prepares five atomic
single-instruction relative detours and installs USB add last. Transfer records,
buffers and trampolines live in the system DLL; no title heap is used. A 4ms
worker drains relative movement once and publishes a controller snapshot, so
repeated game input polls do not consume the same mouse delta twice. USB
completion callbacks perform no filesystem operations or XAM player binding.
The worker handles configuration, player registration and diagnostics.

Failed USB transfers release held input. On removal, sessions are retired,
not recycled; complete cancellation still needs hardware work. The upstream
USB reset/bugcheck patches and notification patches were deliberately omitted.

## Build

On Ubuntu 24.04:

```sh
sudo apt-get update
sudo apt-get install -y build-essential libgmp-dev libmpfr-dev libmpc-dev flex bison texinfo
bash tools/toolchain/build.sh
export PATH="$HOME/.local/mk360-xsdk/bin:$PATH"
make package
```

GitHub Actions runs this build and uploads the ZIP, XEX, checksums and build
metadata. The source archives are SHA-256 checked, upstream ABI patches are
vendored at a recorded commit and actions are pinned to commits. No secrets
or self-hosted Windows/XDK runner are required. GCC is C-only and freestanding;
no newlib, libgcc or proprietary libraries are linked into MK360.

The ELF is fixed at 0x81F00000 and does not yet contain base relocations. The
loader may refuse it when another system DLL occupies that address. Compiling
and validating this file is not a claim that a real Xbox loader accepts it.

LeakSanitizer is disabled for the native test because this execution container
does not allow its `/proc` task inspection; ASan and UBSan remain enabled.
The mapping/configuration core allocates no dynamic memory.
