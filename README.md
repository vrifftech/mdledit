# README #

### What is this repository for? ###

This repository contains the source code for mdledit.

Current version: v1.0.5b BETA.

### How do I get set up? ###

C++14 is used. Build from an **MSYS2 UCRT64** shell.

For a standalone Windows executable that does not require the MSYS2 GCC runtime
DLLs beside it, use:

```sh
make portable CXX=g++ WINDRES=windres
```

The output is:

```text
build/portable/mdledit.exe
```

The `portable` target performs a serial release build with full static runtime
linking and then inspects the PE imports. It fails if the result still imports a
non-system `lib*.dll` runtime.

For development inside MSYS2, the dynamic-runtime build remains available:

```sh
make safe CXX=g++ WINDRES=windres
```

That output is `build/release/mdledit.exe`. When copied outside the MSYS2 UCRT64
environment, it normally also needs the matching compiler runtime DLLs. To make
a distributable directory instead of a single static executable, use:

```sh
make bundle CXX=g++ WINDRES=windres
```

This creates `dist/mdledit/` and copies the executable plus any imported DLLs
that exist beside the active compiler. Keep the entire directory together.
Never obtain replacement runtime DLLs from unrelated download sites; use the
DLLs from the exact toolchain that built the program.

To inspect imports for an existing selected build:

```sh
make CONFIG=portable STATIC=1 runtime-deps
```

A faster dynamic build can be made with:

```sh
make clean
make fast CXX=g++ WINDRES=windres -j$(nproc)
```

Override the toolchain as needed, for example:

```sh
make portable CXX=i686-w64-mingw32-g++ WINDRES=i686-w64-mingw32-windres
```

The Windows system libraries linked by the Makefile are: gdi32, user32,
kernel32, comctl32, comdlg32, and shlwapi.


### Embedded stock supermodel metadata ###

ASCII compilation no longer requires neighboring binary files for the stock
K1/K2 humanoid supermodels (`S_Male01`, `S_Male02`, `S_Female01`,
`S_Female02`, and `S_Female03`). A valid binary supermodel beside the ASCII
model still takes precedence, preserving support for custom supermodels.
Otherwise MDLedit uses the destination game's embedded hierarchy/supernode
metadata.

See `EMBEDDED_SUPERMODELS.md` and the reusable CSV/JSON files in `metadata/`.

### Who do I talk to? ###

PM me on deadlystream.com (Vriff).
