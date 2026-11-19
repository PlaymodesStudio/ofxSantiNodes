#!/usr/bin/env python3
"""Blob Birth Voices: core logic, then the real node classes against compiled OF, with ASan/UBSan.

The node test also runs the original BITXOS_BUBBLES chain (debounce, Change, Event Gate,
Voice Expanding 2) over the same chop sequence and prints both results.

Use --core-only on machines without compiled macOS openFrameworks libraries.
OF_ROOT overrides the usual sibling openFrameworks checkout.
Only node registration is substituted (tests/support/ofxOceanodeNodeModel.h).
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    of = Path(os.environ.get("OF_ROOT", ROOT.parent.parent))
    compiler = os.environ.get("CXX", "clang++")
    flags = [compiler, "-std=c++17", "-O0", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    with tempfile.TemporaryDirectory(prefix="blob-birth-") as temp:
        temp = Path(temp)
        core = temp / "core-tests"
        subprocess.run(flags + ["-Wall", "-Wextra", "-pedantic", str(ROOT / "tests/blobBirthVoicesCoreTests.cpp"), "-o", str(core)], check=True)
        subprocess.run([str(core)], check=True)
        if "--core-only" in sys.argv:
            return
        includes = sorted({p.parent for p in (of / "libs/openFrameworks").rglob("*.h")})
        includes += [p for p in (of / "libs").glob("*/include") if p.is_dir()]
        includes += [of / "libs/freetype/include/freetype", of / "libs/cairo/include/cairo"]
        incflags = ["-I" + str(p) for p in dict.fromkeys(includes)]
        hostflags = ["-DOF_NO_FMOD", "-Wno-deprecated-declarations", "-Wno-c++20-extensions"]
        libs = of / "libs/openFrameworksCompiled/lib/macos"
        command = flags + hostflags + ["-I" + str(ROOT / "tests/support")] + incflags
        command += [str(ROOT / "tests/blobBirthVoicesNodeTests.cpp"),
                    str(of / "libs/openFrameworks/types/ofParameter.cpp"),
                    str(of / "libs/openFrameworks/types/ofParameterGroup.cpp")]
        command += [str(libs / "openFrameworks.a")]
        command += [str(p) for p in sorted(libs.glob("*.a")) if "openframeworks" not in p.name.lower()]
        command += ["-liconv", "-Wl,-dead_strip"]
        for framework in ("Accelerate", "AppKit", "ApplicationServices", "AudioToolbox", "AVFoundation",
                          "Cocoa", "CoreAudio", "CoreFoundation", "CoreMedia", "CoreServices", "CoreVideo",
                          "IOKit", "OpenGL", "QuartzCore", "Security"):
            command += ["-framework", framework]
        executable = temp / "node-tests"
        subprocess.run(command + ["-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True, cwd=temp)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode)
