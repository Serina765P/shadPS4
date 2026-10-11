# SPDX-FileCopyrightText: 2026 Serina765P
# SPDX-License-Identifier: GPL-2.0-or-later

import json
import subprocess
from pathlib import Path


baseline = "233b0747f36379a9717359fa315298267b1370c3"
source = Path("src/core/libraries/np/np_utility/np_utility.cpp")
original = source.read_text()
start_marker = "s32 PS4_SYSV_ABI sceNpWordFilterCreateTitleCtxA("
end_marker = "\ns32 PS4_SYSV_ABI sceNpWordFilterDeleteRequest("
build = ["cmake", "--build", "build-tests", "--parallel", "4",
         "--target", "shadps4_error_dialog_test"]
binary = str(Path("build-tests/tests/shadps4_error_dialog_test").resolve())
report = Path("build-tests/word-filter-baseline.json").resolve()

subprocess.run(["git", "fetch", "--no-tags", "--depth=1", "origin", baseline], check=True)
old_stub = subprocess.check_output(
    ["git", "show", f"{baseline}:src/core/aerolib/stubs.cpp"], text=True)
if old_stub != Path("src/core/aerolib/stubs.cpp").read_text():
    raise RuntimeError("The generic fallback differs from the reported build.")

# The previous release had no NpUtility registration. Replay its actual generic
# fallback for this NID, rather than replacing a positive result with a test mock.
old_create = """s32 PS4_SYSV_ABI sceNpWordFilterCreateTitleCtxA(
    UserService::OrbisUserServiceUserId userId) {
    using CreateTitleCtxA = s32(PS4_SYSV_ABI*)(s32);
    const auto fallback = reinterpret_cast<CreateTitleCtxA>(
        Core::AeroLib::GetStub("6p9jvljuvsw"));
    return fallback(userId);
}
"""

try:
    start = original.index(start_marker)
    end = original.index(end_marker)
    source.write_text('#include "core/aerolib/stubs.h"\n' +
                      original[:start] + old_create + original[end:])
    subprocess.run(build, check=True)
    result = subprocess.run([
        binary, "--gtest_filter=NpWordFilterTest.TitleContextHasAPositiveNonZeroId",
        f"--gtest_output=json:{report}",
    ])
    failures = json.loads(report.read_text())["testsuites"][0]["testsuite"][0].get("failures", [])
    messages = "\n".join(failure["failure"] for failure in failures)
    if result.returncode != 1 or "contextId" not in messages or "0 vs 0" not in messages:
        raise RuntimeError("The generic fallback did not reproduce the zero context ID.")
    print("Baseline reproduced the generic WordFilter fallback returning zero.", flush=True)
finally:
    source.write_text(original)

subprocess.run(build, check=True)
subprocess.run([binary, "--gtest_filter=NpWordFilterTest.*"], check=True)
print("Backported WordFilter API passed all lifecycle regression cases.", flush=True)
