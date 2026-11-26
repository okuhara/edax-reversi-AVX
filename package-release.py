"""Create the Edax runtime ZIP with the upstream 4.5.5 directory layout.

Usage: python package-release.py path/to/edax-4.5.5-nikque.2.zip [--without-macos] [--artifacts DIR] [--pgo DIR]
--artifacts DIR: directory containing the unmerged release-* artifacts from one workflow run for this build
(wEdax-arm64.exe, Linux, macOS, Android). The default is release-artifacts/ next to this script.
--pgo DIR: directory containing the Windows x64 and x86 programs built with PGO (the vc-pgo-* targets of
src/NMakefile: 5 executables and 3 DLLs) and their pgo-build.txt. The default is bin/ next to this script.
Each program must have its line in pgo-build.txt (same SHA-256, a clean source of HEAD, the eval.dat of the
ZIP for the profile) and be linked with /LTCG:PGO (the POGO entry of the program says PGU).
--without-macos makes a ZIP to check before the macOS files (built by the
release-binaries workflow) are there: it leaves them out.
The 32-bit macOS executable is intentionally omitted: current Xcode SDKs
cannot link it, and shipping the upstream executable would misrepresent it
as containing this fork's fixes.
"""

from hashlib import sha256
from os import replace
from pathlib import Path
import re
from struct import error as struct_error, unpack_from
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

# the programs taken from the artifacts of the release-binaries workflow
ARTIFACT_FOR = {
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

# The Windows x64 and x86 programs are the local PGO builds of src/NMakefile (vc-pgo-x64-v4, vc-pgo-x64-v3,
# vc-pgo-x64, vc-pgo-x86-sse, vc-pgo-x86 and vc-pgo-lib), never the ones of the workflow: they are taken from
# --pgo DIR, each with its line in DIR/pgo-build.txt (written by src/pgo-record.cmd after its /LTCG:PGO link).
PGO_FILES = {
    "bin/wEdax-x86-64-v4.exe": "x64",
    "bin/wEdax-x86-64-v3.exe": "x64",
    "bin/wEdax-x86-64.exe": "x64",
    "bin/wEdax-x86-sse.exe": "x86",
    "bin/wEdax-x86.exe": "x86",
    "bin/libedax-x64.dll": "x64",
    "bin/libedax-x64-v3.dll": "x64",
    "bin/libedax-x64-v4.dll": "x64",
}
PE_MACHINES = {0x8664: "x64", 0x14C: "x86", 0xAA64: "arm64"}
# the signature of the POGO entry of the debug directory: "PGU" after /LTCG:PGO, "PGI" for /LTCG:PGI, "LTCG" without PGO
POGO_SIGNATURES = {0x50475500: "PGU", 0x50474900: "PGI", 0x4C544347: "LTCG"}
# the path of a source file on the build machine, as tests/check-build-paths.py finds it
SOURCE_PATH = re.compile(rb'(?:[A-Za-z]:\\|/home/|/Users/|/mnt/|/tmp/|/root/)[\x20-\x7e]{0,400}?\.(?:c|h|cc|cpp|hpp)(?=\x00)')


def pe_build(data: bytes, name: str):
    """The machine ("x64", "x86", "arm64") of a Windows program and the signatures of its POGO entries."""
    try:
        if data[:2] != b"MZ":
            raise ValueError
        pe, = unpack_from("<I", data, 0x3C)
        if data[pe:pe + 4] != b"PE\0\0":
            raise ValueError
        machine, n_sections = unpack_from("<HH", data, pe + 4)
        optional_size, = unpack_from("<H", data, pe + 20)
        optional = pe + 24
        magic, = unpack_from("<H", data, optional)
        directories = optional + (96 if magic == 0x10B else 112)
        debug_rva, debug_size = unpack_from("<II", data, directories + 6 * 8)
        sections = [unpack_from("<IIII", data, optional + optional_size + 40 * i + 8) for i in range(n_sections)]
        if debug_rva == 0 or debug_size == 0:
            return PE_MACHINES.get(machine, hex(machine)), []
        debug = next(raw + debug_rva - va for size, va, raw_size, raw in sections if va <= debug_rva < va + max(size, raw_size))
        signatures = []
        for i in range(debug_size // 28):
            kind, = unpack_from("<I", data, debug + 28 * i + 12)
            pointer, = unpack_from("<I", data, debug + 28 * i + 24)
            if kind == 13:  # IMAGE_DEBUG_TYPE_POGO
                value, = unpack_from("<I", data, pointer)
                signatures.append(POGO_SIGNATURES.get(value, hex(value)))
    except (ValueError, StopIteration, struct_error):
        raise SystemExit(f"{name} は読めない Windows のプログラムです")
    return PE_MACHINES.get(machine, hex(machine)), signatures


def check_windows_programs(sources, names):
    """Stops when a Windows x64 or x86 program of the ZIP is not a PGO build."""
    for name in names:
        if not name.endswith((".exe", ".dll")):
            continue
        machine, signatures = pe_build(sources[name].read_bytes(), str(sources[name]))
        if machine in ("x64", "x86"):
            if name not in PGO_FILES:
                raise SystemExit(f"{name} は PGO の対象でない Windows の {machine} のプログラムです")
            if signatures != ["PGU"]:
                raise SystemExit(f"{sources[name]} は PGO（/LTCG:PGO）でビルドしたものではありません（POGO: {signatures}）")


def check_pgo_build(pgo, names, eval_sha256, slots=None):
    """Checks the PGO programs of names in pgo against pgo/pgo-build.txt; returns their commit (or None)."""
    wanted = [name for name in names if name in PGO_FILES]
    if not wanted:
        return None
    record = pgo / "pgo-build.txt"
    if not record.is_file():
        raise SystemExit(f"PGO ビルドの記録がありません: {record}")
    lines = {}
    for line in record.read_text(encoding="ascii", errors="replace").splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[0] == "/LTCG:PGO":
            lines[fields[1]] = dict(field.split("=", 1) for field in fields[2:] if "=" in field)
    commits = set()
    for name in wanted:
        path = pgo / Path(name).name
        data = path.read_bytes()
        line = lines.get(path.name)
        if line is None:
            raise SystemExit(f"{record} に {path.name} の行がありません")
        if line.get("sha256", "").lower() != sha256(data).hexdigest():
            raise SystemExit(f"{path} は {record} の最後の {path.name} の行（/LTCG:PGO の後の SHA-256）と違います")
        commit = line.get("commit", "")
        if len(commit) != 40 or any(ch not in "0123456789abcdef" for ch in commit):
            raise SystemExit(f"{record} の {path.name} の行のコミットを読めません")
        if line.get("source") != "clean":
            raise SystemExit(f"{path.name} は、コミットと違うソース（src・include・tests）でビルドしたものです")
        if line.get("eval", "").lower() != eval_sha256:
            raise SystemExit(f"{path.name} のプロファイルは、ZIP に入れる eval.dat と違う eval.dat で取ったものです")
        if slots is not None and line.get("slots") != slots:
            raise SystemExit(f"{path.name} は {line.get('slots')} 枠のビルドで、--slots {slots} と違います")
        machine, signatures = pe_build(data, str(path))
        if machine != PGO_FILES[name]:
            raise SystemExit(f"{path} は {PGO_FILES[name]} ではなく {machine} のプログラムです")
        if signatures != ["PGU"]:
            raise SystemExit(f"{path} は PGO（/LTCG:PGO）でビルドしたものではありません（POGO: {signatures}）")
        found = SOURCE_PATH.search(data)
        if found:
            raise SystemExit(f"{path} にビルド先のソースのパスがあります: {found.group().decode('latin1')}")
        commits.add(commit)
    if len(commits) != 1:
        raise SystemExit(f"{record} の PGO のプログラムのコミットがそろっていません: {', '.join(sorted(commits))}")
    return commits.pop()


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


def check_build_source(artifacts, names, pgo_commit=None):
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
    if origin is not None and pgo_commit is not None and origin[1] != pgo_commit:
        raise SystemExit("ワークフローの成果物と PGO のプログラムのコミットが違います")
    if (ROOT / ".git").exists():
        head = check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
        if any(commit != head for commit in (origin[1] if origin else None, pgo_commit) if commit is not None):
            raise SystemExit("成果物のコミットが梱包元のHEADと一致しません")
    return origin


def main() -> None:
    if len(argv) < 2 or argv[1].startswith("--"):
        raise SystemExit(__doc__)
    output = Path(argv[1]).resolve()
    artifacts = ROOT / "release-artifacts"
    pgo = ROOT / "bin"
    without_macos = False
    rest = argv[2:]
    while rest:
        option = rest.pop(0)
        if option == "--without-macos":
            without_macos = True
        elif option == "--artifacts" and rest:
            artifacts = Path(rest.pop(0)).resolve()
        elif option == "--pgo" and rest:
            pgo = Path(rest.pop(0)).resolve()
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
    sources = {name: artifacts / ARTIFACT_FOR[name] / Path(name).name if name in ARTIFACT_FOR else
               pgo / Path(name).name if name in PGO_FILES else ROOT / name for name in names}
    missing = [str(sources[name]) for name in names if not sources[name].is_file()]
    if missing:
        raise SystemExit("missing files: " + ", ".join(missing))

    pgo_commit = check_pgo_build(pgo, names, EVAL_SHA256)
    check_windows_programs(sources, names)
    check_build_source(artifacts, names, pgo_commit)
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
