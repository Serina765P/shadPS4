<!--
SPDX-FileCopyrightText: 2026 Serina765P
SPDX-License-Identifier: GPL-2.0-or-later
-->

# Idolmaster release compatibility

This branch starts from Nyces/shadPS4 `release` at
`a22e1eac86f085cd1f6ba060da2836ac69d2be2f`. It retains the fork's existing
GS copy shader, AJM batch lifecycle, audio pacing, per-port locking and PM4
compatibility changes for Platinum Stars (CUSA03628/CUSA05706) and Stella
Stage (CUSA07753/CUSA09153).

## Upstream backports

| Upstream PR | Change | Adaptation for this release |
| --- | --- | --- |
| [#4839](https://github.com/shadps4-emu/shadPS4/pull/4839) | Widen Precise readback fault windows | Keep the release buffer-cache interface and page tracking. |
| [#5110](https://github.com/shadps4-emu/shadPS4/pull/5110) | Respect multisample storage-image support and compatible image view types | Apply the original image validation fixes. |
| [#5258](https://github.com/shadps4-emu/shadPS4/pull/5258) | Release completed nested indirect-buffer coroutine frames | Add destruction at the CE/GFX/ASC completion sites without replacing fork PM4 handling. |
| [#5092](https://github.com/shadps4-emu/shadPS4/pull/5092) | Invalidate aliased texture images after raw buffer writes | Keep the fork's buffer barriers; broaden the invalidation condition. |
| [#5317](https://github.com/shadps4-emu/shadPS4/pull/5317) | Restore Windows page protections after splitting mappings | Preserve the release mapping implementation and restore protections on surviving regions. |
| [#4859](https://github.com/shadps4-emu/shadPS4/pull/4859) | Wake audio waiters when their port closes | Keep independent per-port locking, SDL pacing and worker priorities. |
| [#5001](https://github.com/shadps4-emu/shadPS4/pull/5001) + [#5196](https://github.com/shadps4-emu/shadPS4/pull/5196) | Correct mip layout, small-mip detiling, thick array padding and tile-mode mapping | Carry per-mip micro-tiling flags through the release uniform buffer and adapt the GLSL preprocessor path. |
| [#5062](https://github.com/shadps4-emu/shadPS4/pull/5062), cache-key portion | Separate tiling pipelines by sample count and exact bit depth | Retain runtime GLSL compilation and use the upstream TilingKey; avoid 64/96-bit collisions. |
| [#5231](https://github.com/shadps4-emu/shadPS4/pull/5231) | Cover every submit wait semaphore and presentation texture reads | Keep the release submit mutex and use the corrected wait-stage array and fragment-read barrier. |

The sparse-arena/batched-upload renderer refactors are deliberately outside
this release backport. Their interfaces and subsequent regression fixes must
be migrated together if a future branch adopts that architecture.
The image write-barrier checks restored by upstream #5193 already exist in
this release, so they are not duplicated here.

## Cloud builds

The **Idolmaster Release Compatibility** GitHub Actions workflow builds the
Windows x64 and Linux x64 emulator on pushes to the compatibility branch,
pull requests targeting `release`, and manual runs. The original all-platform
workflow remains available for `main`.

Windows artifacts contain the emulator executable, license, this document and
the exact commit ID. Linux artifacts contain a tarball that preserves the
executable bit; it is built on Ubuntu 24.04 and is not a universal AppImage.
These are emulator-core builds; use a compatible QtLauncher if a launcher is
needed. Obtain firmware modules and game data separately as described in the
project README.

Linux CI also compiles representative tiling/detiling GLSL variants and runs
the existing settings, NGS2 and HTTP CPU tests. GPU-dependent GCN tests are
excluded from hosted-runner execution. Shader compilation checks syntax and
SPIR-V generation; it does not verify rendered pixels or hardware timing.

## Game validation

The core automatically acknowledges error dialog `0x80550006`
(`ORBIS_NP_ERROR_SIGNED_OUT`) when both network connectivity and ShadNet are
disabled. It finishes the dialog as clicking OK would, while logging the code
at Info level. NP authentication results remain unchanged. Other error codes,
including saved-data errors, and sign-out dialogs with networking or ShadNet
enabled continue to require confirmation.

CI exercises the real error-dialog APIs with a headless layer registry. It
first reproduces the waiting dialog using the preceding release implementation,
then verifies automatic completion, repeated notifications, preserved online
dialogs, unrelated errors and parameter validation with the updated code.

Keep readback mode at **Precise**, matching the original fork recommendation.
Actual compatibility remains unverified until both games are played with
these artifacts. Compare against the original release using the same game
version, settings, GPU driver and save file:

- Story scenes and live-performance startup, including character faces.
- Multiple consecutive songs and character/costume changes.
- Audio continuity, dance timing and recovery after shader compilation.
- At least one hour of play, observing memory growth and texture corruption.
- Closing/relaunching the game and audio-port shutdown.

Record the artifact commit, title ID, CPU/GPU, driver and reproduction steps
with results. A passing cloud build does not establish that the two games'
remaining rendering defects have been fixed.
