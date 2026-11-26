"""Create the Edax runtime ZIP with the upstream 4.5.5 directory layout.

Usage: python package-release.py path/to/edax-4.5.5-nikque.2.zip [--without-macos] [--artifacts DIR]
--artifacts DIR: directory containing the unmerged release-* artifacts from one workflow run for this build.
The default is release-artifacts/ next to this script.
--without-macos makes a ZIP to check before the macOS files (built by the
release-binaries workflow) are there: it leaves them out.
The 32-bit macOS executable is intentionally omitted: current Xcode SDKs
cannot link it, and shipping the upstream executable would misrepresent it
as containing this fork's fixes.
"""

from hashlib import sha256
from os import replace
from pathlib import Path
from subprocess import check_output
from sys import argv
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


ROOT = Path(__file__).resolve().parent
EVAL_SHA256 = "f8b2299612d9fa4414157e70e932636e33111c2602d0c2fc382a7d90ef21b792"

BINARIES = (
    "bin/aEdax-arm64-v8a",
    "bin/aEdax-armeabi-v7a",
    "bin/lEdax-x86",
    "bin/lEdax-x86-64",
    "bin/lEdax-x86-64-v3",
    "bin/lEdax-x86-64-v4",
    "bin/mEdax-arm64",
    "bin/mEdax-x64-modern",
    "bin/wEdax-arm64.exe",
    "bin/wEdax-x86.exe",
    "bin/wEdax-x86-sse.exe",
    "bin/wEdax-x86-64.exe",
    "bin/wEdax-x86-64-v3.exe",
    "bin/wEdax-x86-64-v4.exe",
)

# libedax (Edax as a library)
LIBRARIES = (
    "bin/libedax.universal.dylib",
    "bin/libedax-arm64-v8a.so",
    "bin/libedax-armeabi-v7a.so",
    "bin/libedax-x86-64.so",
    "bin/libedax-x86-64-v3.so",
    "bin/libedax-x86-64-v4.so",
    "bin/libedax-x64.dll",
    "bin/libedax-x64-v3.dll",
    "bin/libedax-x64-v4.dll",
)

ARTIFACT_FOR = {
    "bin/wEdax-x86-64.exe": "release-windows-x64",
    "bin/wEdax-x86-64-v3.exe": "release-windows-x64",
    "bin/wEdax-x86-64-v4.exe": "release-windows-x64",
    "bin/libedax-x64.dll": "release-windows-x64",
    "bin/libedax-x64-v3.dll": "release-windows-x64",
    "bin/libedax-x64-v4.dll": "release-windows-x64",
    "bin/wEdax-x86.exe": "release-windows-x64_x86",
    "bin/wEdax-x86-sse.exe": "release-windows-x64_x86",
    "bin/wEdax-arm64.exe": "release-windows-x64_arm64",
    "bin/lEdax-x86": "release-linux",
    "bin/lEdax-x86-64": "release-linux",
    "bin/lEdax-x86-64-v3": "release-linux",
    "bin/lEdax-x86-64-v4": "release-linux",
    "bin/libedax-x86-64.so": "release-linux",
    "bin/libedax-x86-64-v3.so": "release-linux",
    "bin/libedax-x86-64-v4.so": "release-linux",
    "bin/mEdax-x64-modern": "release-macos-x64",
    "bin/mEdax-arm64": "release-macos-arm64",
    "bin/libedax.universal.dylib": "release-macos-arm64",
    "bin/aEdax-arm64-v8a": "release-android",
    "bin/aEdax-armeabi-v7a": "release-android",
    "bin/libedax-arm64-v8a.so": "release-android",
    "bin/libedax-armeabi-v7a.so": "release-android",
}

MACOS_FILES = ("bin/mEdax-arm64", "bin/mEdax-x64-modern", "bin/libedax.universal.dylib")

OTHER_FILES = (
    "LICENSE",
    "README.md",
    "RELEASE-NOTES.md",
    "RELEASE-NOTES.ja.md",
    "bin/README.MS-Windows.txt",
    "bin/config.ini",
    "bin/data/book.dat",
    "bin/data/eval.dat",
    "problem/README.md",
    "problem/fforum-1-19.obf",
    "problem/fforum-20-39.obf",
    "problem/fforum-40-59.obf",
    "problem/fforum-60-79.obf",
)


# the user manual (since v4.5.5-nikque.13): every page of manual/, manual/ja and manual/en
MANUAL_PAGES = (
    "01-introduction.md",
    "02-quick-start.md",
    "03-playing.md",
    "04-settings.md",
    "05-search.md",
    "06-book-basics.md",
    "07-book-learning.md",
    "08-book-maintenance.md",
    "09-large-books.md",
    "10-files.md",
    "11-solve-bench.md",
    "12-integration.md",
    "13-troubleshooting.md",
    "14-reference.md",
    "15-glossary.md",
    "README.md",
)
MANUAL_FILES = ("manual/README.md",) + tuple(
    f"manual/{language}/{page}" for language in ("en", "ja") for page in MANUAL_PAGES
)


def check_build_source(artifacts, names):
    folders = sorted({ARTIFACT_FOR[name] for name in names if name in ARTIFACT_FOR})
    origin = None
    for folder in folders:
        marker = artifacts / folder / "build-id.txt"
        if not marker.is_file():
            raise SystemExit(f"組み立て元の記録がありません: {marker}")
        fields = marker.read_text(encoding="utf-8").split()
        if (len(fields) != 3 or len(fields[1]) != 40 or
                any(ch not in "0123456789abcdef" for ch in fields[1]) or not fields[2].isdigit()):
            raise SystemExit(f"組み立て元の記録を読めません: {marker}")
        if origin is not None and fields != origin:
            raise SystemExit(f"異なるコミットまたは実行の成果物が混在しています: {marker}")
        origin = fields
    if (ROOT / ".git").exists():
        head = check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
        if head != origin[1]:
            raise SystemExit("成果物のコミットが梱包元のHEADと一致しません")
    return origin


def main() -> None:
    if len(argv) < 2 or argv[1].startswith("--"):
        raise SystemExit(__doc__)
    output = Path(argv[1]).resolve()
    artifacts = ROOT / "release-artifacts"
    without_macos = False
    rest = argv[2:]
    while rest:
        option = rest.pop(0)
        if option == "--without-macos":
            without_macos = True
        elif option == "--artifacts" and rest:
            artifacts = Path(rest.pop(0)).resolve()
        else:
            raise SystemExit(__doc__)
    eval_data = (ROOT / "bin/data/eval.dat").read_bytes()
    if sha256(eval_data).hexdigest() != EVAL_SHA256:
        raise SystemExit("bin/data/eval.dat differs from the upstream v4.5.5 asset")

    names = [name for name in (*OTHER_FILES, *MANUAL_FILES, *BINARIES, *LIBRARIES)
             if not (without_macos and name in MACOS_FILES)]
    for name in (*OTHER_FILES, *MANUAL_FILES, *BINARIES, *LIBRARIES):
        if name not in names:
            print(f"left out: {name}")
    # every file is checked before the ZIP is written, and the ZIP is written to another file first: a missing
    # file or a failure leaves no partial ZIP, nor replaces an existing one (up to v4.5.5-nikque.15 it did)
    sources = {name: artifacts / ARTIFACT_FOR[name] / Path(name).name
               if name in ARTIFACT_FOR else ROOT / name for name in names}
    missing = [str(sources[name]) for name in names if not sources[name].is_file()]
    if missing:
        raise SystemExit("missing files: " + ", ".join(missing))

    check_build_source(artifacts, names)
    output.parent.mkdir(parents=True, exist_ok=True)
    partial = output.with_name(output.name + ".tmp")
    try:
        with ZipFile(partial, "w") as archive:
            for name in names:
                path = sources[name]
                data = path.read_bytes()
                info = ZipInfo(name, date_time=(2026, 9, 27, 0, 0, 0))
                info.create_system = 3
                executable = name in (*BINARIES, *LIBRARIES) and not name.endswith((".exe", ".dll"))
                info.external_attr = ((0o100755 if executable else 0o100644) << 16)
                info.compress_type = ZIP_DEFLATED
                archive.writestr(info, data, compress_type=ZIP_DEFLATED, compresslevel=9)

        with ZipFile(partial) as archive:
            bad = archive.testzip()
            if bad is not None:
                raise SystemExit(f"ZIP verification failed: {bad}")
        replace(partial, output)
    finally:
        if partial.exists():
            partial.unlink()
    print(f"Created {output} ({output.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
