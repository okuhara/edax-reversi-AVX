"""Package this release without Android, whose CI build failed.

Usage: python package-release-without-android.py output.zip
Uses the normal package checks, layout and permissions. No old Android files
are substituted for the binaries that could not be rebuilt.
"""
import importlib.util
from pathlib import Path

spec = importlib.util.spec_from_file_location("edax_package", Path(__file__).with_name("package-release.py"))
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)
package.BINARIES = tuple(p for p in package.BINARIES if not p.startswith("bin/aEdax-"))
package.LIBRARIES = tuple(p for p in package.LIBRARIES if not p.startswith(("bin/libedax-arm64-v8a", "bin/libedax-armeabi-v7a")))
if __name__ == "__main__":
    package.main()
