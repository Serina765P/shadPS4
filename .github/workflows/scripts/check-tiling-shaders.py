# SPDX-FileCopyrightText: 2026 Serina765P
# SPDX-License-Identifier: GPL-2.0-or-later

import itertools
import subprocess
import tempfile
from pathlib import Path


shader = Path("src/video_core/host_shaders/tiling.comp")
variants = 0
with tempfile.TemporaryDirectory() as output_dir:
    for array_mode, thickness, micro_mode in (
        (2, 1, 1),
        (3, 4, 4),
        (4, 1, 0),
        (4, 1, 2),
        (7, 4, 4),
        (8, 8, 4),
    ):
        bits = (8, 16, 32, 64, 96, 128) if array_mode in (2, 3) else (8, 16, 32, 64, 128)
        for bpp, samples, is_tiler in itertools.product(bits, (1, 2, 4, 8), (False, True)):
            defines = {
                "BITS_PER_PIXEL": bpp,
                "NUM_SAMPLES": samples,
                "ARRAY_MODE": array_mode,
                "MICRO_TILE_MODE": micro_mode,
                "MICRO_TILE_THICKNESS": thickness,
            }
            if array_mode not in (2, 3):
                defines.update(
                    PIPE_CONFIG=12,
                    BANK_WIDTH=1,
                    BANK_HEIGHT=1,
                    NUM_BANKS=4,
                    NUM_BANK_BITS=2,
                    TILE_SPLIT_BYTES=512,
                    MACRO_TILE_ASPECT=2,
                )
            if is_tiler:
                defines["IS_TILER"] = 1
            command = [
                "glslangValidator", "-V", "--target-env", "vulkan1.3",
                *(f"-D{name}={value}" for name, value in defines.items()),
                "-o", str(Path(output_dir) / "tiling.spv"), str(shader),
            ]
            result = subprocess.run(command, capture_output=True, text=True)
            if result.returncode:
                raise SystemExit(f"Shader variant failed: {defines}\n{result.stdout}\n{result.stderr}")
            variants += 1
print(f"Compiled {variants} tiling/detiling variants for thin, thick, xthick, display and depth modes.")
