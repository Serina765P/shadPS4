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
| [NpUtility at 443f022](https://github.com/shadps4-emu/shadPS4/blob/443f022450f6857256e780daa3d5a14029a748f7/src/core/libraries/np/np_utility/np_utility.cpp), WordFilter subset | Return valid context/request handles instead of the generic zero stub | Adapt the NpHandler include path; retain this release's user identities and authentication state. |

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
disabled. Opening still enters RUNNING. After a status query has returned
RUNNING to the guest, the UI frame finishes through the same path as the OK
button, without drawing the offline notification. This preserves the observable
dialog lifecycle, including when rendering precedes the first status query.
Immediate completion inside the open call was removed after a reported game
stall. The code and acknowledgement stages are retained in Info-level logs.
NP authentication results remain unchanged. Other error codes,
including saved-data errors, and sign-out dialogs with networking or ShadNet
enabled continue to require confirmation.

For play without trophy data, `0x80551604` (trophy INVALID_ARGUMENT) is also
acknowledged when trophy registration has confirmed that every registered
context for the dialog's user lacks its trophy definition XML. Unregistered,
destroyed, other-user and mixed available/unavailable contexts retain normal
dialogs. An unspecified-user dialog requires all registered contexts to lack
definitions. Existing definition files, even if subsequently invalid, prevent
this fallback from hiding parameter errors. This only acknowledges the UI
notification; trophy API results, unlock flags and records are unchanged. Adding
valid trophy data and restarting restores normal error-dialog behavior.

CI exercises the real error-dialog APIs with a headless layer registry. It
first reproduces the skipped RUNNING state using the preceding immediate-completion
implementation, then verifies the status-query/UI-frame handoff, repeated
notifications, close/termination before confirmation, preserved online dialogs,
unrelated errors and parameter validation with the updated code. These tests
check the real API and UI acknowledgement path; actual game progression still
requires gameplay validation.

The WordFilter subset of NpUtility is also registered. Stella Stage startup
called the missing `sceNpWordFilterCreateTitleCtxA` NID and displayed error
`0x0` after the generic fallback returned zero. Context creation now returns a
valid positive handle, with the upstream request, completion, cancellation and
deletion lifecycle. CI replays the actual generic fallback to reproduce zero,
then verifies the real APIs and startup NID registration. Zero-code dialogs are
not dismissed by the offline policy. As in the upstream implementation, comment
processing currently passes text through; an online censorship service is not
implemented by this backport.

The trophy policy has its own baseline regression: the previous dialog-open
function leaves the missing-data notification waiting for OK, while the updated
code completes after RUNNING and a UI frame. CPU checks also render normal
dialogs in an ImGui context to verify that preserved errors remain RUNNING.

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
