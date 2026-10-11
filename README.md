# VVVVVV-MUSIC-TO-PSP

Converts VVVVVV's `vvvvvvmusic.vvv` into individual `.at3` tracks
for the [PSP port](https://github.com/dntrnk/VVVVVV-PSP).

## How to use

1. Download `vvvvvv-music-to-psp.exe` from the [Releases](https://github.com/dntrnk/VVVVVV-MUSIC-TO-PSP/releases) page.
2. Put the following three files in the same folder:
    - `vvvvvv-music-to-psp.exe`
    - `vvvvvvmusic.vvv`
    - `at3conv.exe`
3. Run `vvvvvv-music-to-psp.exe`.
4. Wait for it to finish. The resulting `.at3` files will be in `music/`.
5. Copy the `music/` folder next to the original `vvvvvvmusic.vvv` in the PSP port. You can delete `vvvvvvmusic.vvv` from there.

The `temp/` folder is created during conversion and can be deleted afterwards.

## About `at3conv.exe`

`at3conv.exe` is a copy of Sony's AT3 converter. **I cannot distribute it for legal reasons.**

You have to obtain it yourself. Once you have it, rename it to
`at3conv.exe` and put it next to `vvvvvv-music-to-psp.exe`.

## How to Build

Executable available in the [Releases](https://github.com/dntrnk/VVVVVV-MUSIC-TO-PSP/releases) page was built
with MinGW-w64 by following command:
```
x86_64-w64-mingw32-gcc -O2 -o vvvvvv-music-to-psp.exe main.c
```

## Third-party libraries used in this project

- [stb_vorbis](https://github.com/nothings/stb)
- [dr_wav](https://github.com/mackron/dr_libs)
