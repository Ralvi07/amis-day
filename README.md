# Ami's Day

A native Windows anniversary game. Follow Ami through a day out, choose where she goes, and see her choices at the end.

## Download

Download [AmisDay.exe](https://github.com/Ralvi07/amis-day/releases/latest/download/AmisDay.exe) and run it on 64-bit Windows. The soundtrack and icon are embedded.

## Controls

- Arrow keys to move
- Z to interact or confirm
- X to cancel
- M to toggle music
- F11 to toggle fullscreen

## Build from source

Install MSYS2, open its **UCRT64** shell, and run `pacman -S --needed mingw-w64-ucrt-x86_64-toolchain`. In the folder containing `main.cpp`, `music.mp3`, and `soul.ico`, run these commands in the same shell.

```sh
printf '101 RCDATA "music.mp3"\n102 ICON "soul.ico"\n' > resources.rc
windres resources.rc -O coff -o resources.o
g++ -std=c++17 -O2 -s -mwindows -static -o AmisDay.exe main.cpp resources.o -lgdi32 -luser32 -lwinmm
```

## Credits

Music by Toby Fox.
