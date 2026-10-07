# Building SAC on Windows

The Windows build is driven by CMake from the top of the source tree; the
files it needs are described below.

SAC is a command line program — the windows it opens are only for plots. The
executable is therefore a **console** application and prints its `SAC>` prompt
in the terminal it was started from. The plotting uses a native Windows
backend (`src/win/`, GDI+); no X server is involved, unlike the X11 back end
used on Linux and macOS.

## Prerequisites

* **MSYS2** (https://www.msys2.org/). In the `CLANGARM64` environment for an
  ARM64 build, or `UCRT64` for x86_64:

  | ARM64 (`CLANGARM64`) | x86_64 (`UCRT64`) |
  | --- | --- |
  | `mingw-w64-clang-aarch64-clang` | `mingw-w64-ucrt-x86_64-gcc` |
  | `mingw-w64-clang-aarch64-cmake` | `mingw-w64-ucrt-x86_64-cmake` |
  | `mingw-w64-clang-aarch64-ninja` | `mingw-w64-ucrt-x86_64-ninja` |
  | `mingw-w64-clang-aarch64-libxml2` | `mingw-w64-ucrt-x86_64-libxml2` |
  | `mingw-w64-clang-aarch64-curl` | `mingw-w64-ucrt-x86_64-curl` |
  | `mingw-w64-clang-aarch64-zlib` | `mingw-w64-ucrt-x86_64-zlib` |

  For example:

  ```sh
  pacman -S --needed mingw-w64-clang-aarch64-clang \
                      mingw-w64-clang-aarch64-cmake \
                      mingw-w64-clang-aarch64-ninja \
                      mingw-w64-clang-aarch64-libxml2 \
                      mingw-w64-clang-aarch64-curl \
                      mingw-w64-clang-aarch64-zlib
  ```

* **Git**, used by the build to apply the patches in `patches/`. It does not
  have to be on the MSYS2 `PATH`; CMake locates it itself.

## Building

From the top of the source tree, in a `CLANGARM64` (or `UCRT64`) shell:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This produces `build/sac.exe` and the eight `utils/` programs (`sgftops`,
`sgfswap`, `saclst`, `sacswap`, `sacset`, `sacdiff`, `bbfswap` and `unvis`),
with `sacaux/` staged next to `sac.exe`.

## Running

`build/sac.exe` links `libc++` from the MSYS2 `bin` directory, so start it from
an MSYS2 shell (or put that directory on `PATH`):

```sh
cd build
./sac.exe
```

```
 SEISMIC ANALYSIS CODE Community Edition [2026-10-05 (Version 103.0)]
 Copyright 2025 EarthScope Consortium www.earthscope.org

SAC>
```

The `dist/` tree described below is self-contained and can be run from
anywhere.

`sacaux/` must sit beside `sac.exe` (or `SACAUX` be set): the compiled-in
default is `${CMAKE_INSTALL_PREFIX}/sacaux`, and SAC falls back to the
directory of the executable.

## Packaging

```sh
cmake --build build --target dist
```

writes `sac-<version>-windows-<arch>.zip`, containing:

```
sac.exe        statically linked against libxml2, zlib and curl
*.exe          the eight utils/ programs (sgftops, sgfswap, saclst, sacswap,
               sacset, sacdiff, bbfswap, unvis)
sacaux/        the auxiliary data (colour tables, help, ...)
libc++.dll     the only non-system runtime dependency
libunwind.dll
License
README.md
```

Because the dependencies are linked statically the archive needs no MSYS2
installation. Only `libc++` is shipped alongside, since MSYS2 builds it as a
DLL and the compiler's implicit `-lc++` cannot be redirected to the static
archive.

## Build options

| Option | Default | Meaning |
| --- | --- | --- |
| `SAC_STATIC_DEPS` | `ON` | Link `libxml2`, `zlib` and `curl` statically so `sac.exe` needs no MSYS2. `OFF` uses the shared MSYS2 libraries, which requires them on `PATH`. |
| `SAC_GUI_SUBSYSTEM` | `OFF` | Link as a Windows GUI subsystem application. `win.cpp` then uses its `WinMain()` entry point, which calls `AllocConsole()` and detaches SAC from the terminal that started it — the behaviour of the old Visual Studio build. |
| `SAC_SACAUX` | `<prefix>/sacaux` | Directory compiled in as the default `SACAUX`. |
| `SAC_STAGE_AUX` | `ON` | Copy `sacaux/` next to the executable after building. |
| `SAC_BUILD_UTILS` | `ON` | Also build the `utils/` programs (see below). |

## What is in this directory

| Path | Purpose |
| --- | --- |
| `config.h.in` | Template for the generated `config.h`. One header now serves every Windows toolchain, replacing both `inc/config_win.h` (MSVC) and the autoconf-generated header used by MinGW/Cygwin. |
| `compat.h` | Force-included into every translation unit; supplies the POSIX pieces Windows lacks so that vendored code stays untouched. |
| `compat/` | The implementations behind `compat.h`: `strsep`, `glob`, `gmtime_r`/`localtime_r`, `TIOCGWINSZ` and `<sys/select.h>`, and no-op replacements for the bison trace helpers referenced by the committed parser. |
| `patches/` | Patches applied to the vendored libraries at build time (below). |
| `gen-sources.ps1` | Generates `sources.cmake` from `src/Makefile.am`. |
| `sources.cmake` | Generated list of the sources that make up the SAC program and its libraries. Do not edit by hand. |

## Utility programs

The autotools build builds the programs in `utils/` (they are in its
`SUBDIRS`), and so does this one:

| Program | Purpose |
| --- | --- |
| `sgftops` | Convert a SAC Graphics Format (`.sgf`) file to PostScript. |
| `sgfswap` | Byte-swap an `.sgf` file. |
| `saclst` | List SAC header values. |
| `sacset` | Change SAC header values. |
| `sacdiff` | Compare two SAC files. |
| `sacswap` | Byte-swap a SAC file. |
| `bbfswap` | Byte-swap a blackboard variable file. |
| `unvis` | Decode a `VIS`-encoded text stream. |

`sgftops` matters at run time: SAC's `print`/PostScript path runs
`sgftops <in.sgf> <out.ps>` as an external command (`src/gam/xprint.c`), so
`sgftops.exe` has to be on `PATH` (or in the working directory) for that to
work.

`saclst`, `sacset`, `sacdiff`, `sacswap` and `bbfswap` link against sacio, as
their autotools counterparts link against `src/libsacio.a`. That library is
`sacio` plus `src/ucf/distaz.c` (`sacio.c`'s `update_distaz()` calls it),
`src/co/math.c` and `src/vars/bbf.c`; the `sacio_sac` target reproduces it.

## Source lists

`sources.cmake` is generated from `src/Makefile.am`, which is the authoritative
list of the sources for the Windows (non-X11, non-macOS) configuration. Re-run
the generator after adding or removing files there:

```sh
pwsh -File win/gen-sources.ps1
```

`fern` is small enough that its sources are listed directly in `CMakeLists.txt`,
from `libfern_a_SOURCES` and `libpile_a_SOURCES` in `fern/Makefile.am`. Note
that `fern.c`, the standalone program, must never be linked into the library: it
defines `main()` and its own `error()`, which collides with SAC's.

## Patched vendored libraries

`fern`, `libmseed`, `evalresp` (including its bundled `mxml`) and `sacio` are
third party code and are kept byte for byte identical to upstream.  Where
Windows needs a change the component is copied into the build tree and the
matching patch in `patches/` is applied to the copy. If a vendored library is
updated and the patched lines move, the build stops and names the patch that
needs refreshing, rather than silently dropping the fix.

| Patch | Reason |
| --- | --- |
| `libmseed-stat.patch` | Drops `#define stat _stat` / `#define fstat _fstat`, which rewrite `struct stat` inside MinGW's `<sys/stat.h>` into `_stat64i32` and collide with the declarations above. |
| `fern-arg-double.patch` | Renames the `DOUBLE` enumerator: Windows has `typedef double DOUBLE` in `wtypesbase.h`. |
| `mxml-ssize_t.patch` | Only typedefs `ssize_t` where the CRT really lacks it; MinGW already defines it. |
| `time64-prid64.patch` | Adds spaces around `PRId64`. Clang lexes the macro body before expansion in C++11 and rejects a literal followed directly by an identifier. |
| `sacio-binary-mode.patch` | Opens the binary SAC files written by `sacio.c` in binary mode (`"wb"`/`"r+b"`). Upstream's `"w"`/`"r+"` mean the same thing on POSIX, but on Windows they enable `\n` -> `\r\n` translation, which corrupted every file written by `write` (an extra byte per `0x0a` in the data) so that SAC could not read its own output back. |

`sacio` is compiled from the patched copy, not from the source tree, because
this patch changes one of its `.c` files and not just a header.

Should a patch be rejected, refresh it with:

```sh
# edit the file, then
git diff -- <file> > win/patches/<name>.patch
git checkout -- <file>
```

Note `.gitattributes` marks `win/patches/*.patch` as `-text`, so
`core.autocrlf` cannot convert them to CRLF and break `git apply`.

## Testing

The autotools test suite in `t/` is **not wired into the CMake build yet**, so
nothing currently verifies SAC's numerical behaviour.

For a quick check that the build is sane:

```sh
cd build && timeout 5 ./sac.exe     # banner and SAC> prompt
```

A macro or a single command can be given on the command line and is run at
start-up.  This is the way to script SAC on Windows:

```sh
./sac.exe commands.m
```

Put one command per line in the macro and finish with `quit`, so that SAC exits
with status 0; a non-zero status means a command failed.

Interactive commands that read the cursor, such as `ppk`, work too.  When one
starts, the plot window is brought to the front and takes the keyboard; move the
mouse to the pick position on the trace and type the single-character response
in the **plot window**, not the console.  The pick location is the pointer
position at the moment the key is pressed.  When the command finishes, click the
console again to type further SAC commands.  Because `ppk` waits for keystrokes,
it cannot be run from a macro unattended.

Closing a plot window does **not** end the SAC session.  The window is hidden and
the next plotting command re-opens it; closing it while a picking command is
waiting ends that command and returns to the prompt.

Plot windows open at three quarters of the usable screen, centred, rather than
at a fixed small size.

Two caveats:

* Piping commands in does not work — SAC reads its input through the Windows
  console API, so a redirected stdin is ignored and `./sac.exe < commands.m`
  prints only the banner.  Use the macro argument above instead, or type at the
  prompt in a real terminal.
* Redirecting output to a file and killing the process loses the banner: stdout
  is block-buffered, so nothing is flushed. Let it exit cleanly, or use a
  console.
