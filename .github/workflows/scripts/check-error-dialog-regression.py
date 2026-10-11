# SPDX-FileCopyrightText: 2026 Serina765P
# SPDX-License-Identifier: GPL-2.0-or-later

import json
import subprocess
from pathlib import Path


baseline = "240400c4069e3ff8dfece6cd6c18cd514e44e7ad"
source = Path("src/core/libraries/ime/error_dialog.cpp")
original = source.read_text()
start_marker = "Error PS4_SYSV_ABI sceErrorDialogOpen("
end_marker = "\nint PS4_SYSV_ABI sceErrorDialogOpenDetail("
build = ["cmake", "--build", "build-tests", "--parallel", "4",
         "--target", "shadps4_error_dialog_test"]
binary = str(Path("build-tests/tests/shadps4_error_dialog_test").resolve())
report = Path("build-tests/error-dialog-baseline.json").resolve()

subprocess.run(["git", "fetch", "--no-tags", "--depth=1", "origin", baseline], check=True)
old_source = subprocess.check_output(["git", "show", f"{baseline}:{source.as_posix()}"], text=True)
old_open = old_source[old_source.index(start_marker):old_source.index(end_marker)]

try:
    start = original.index(start_marker)
    end = original.index(end_marker)
    source.write_text(original[:start] + old_open + original[end:])
    subprocess.run(build, check=True)
    result = subprocess.run([
        binary, "--gtest_filter=ErrorDialogTest.OfflineSignedOutIsAutomaticallyAcknowledged",
        f"--gtest_output=json:{report}",
    ])
    failures = json.loads(report.read_text())["testsuites"][0]["testsuite"][0].get("failures", [])
    messages = "\n".join(failure["failure"] for failure in failures)
    if result.returncode != 1 or "sceErrorDialogGetStatus()" not in messages or "Status::FINISHED" not in messages:
        raise RuntimeError("Baseline did not reproduce the manual-confirmation dialog state.")
    print("Baseline reproduced the offline sign-out dialog waiting for confirmation.", flush=True)
finally:
    source.write_text(original)

subprocess.run(build, check=True)
subprocess.run([binary], check=True)
print("Fixed dialog lifecycle passed all regression cases.", flush=True)
