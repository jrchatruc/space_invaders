# Space Invaders

A Space Invaders emulator written in C, following [Emulator 101](http://emulator101.com).

## Requirements

- [SDL2](https://www.libsdl.org/)

Only tested on Linux and macOS. On macOS all you need is to to run

```
brew install sdl2
```

On linux, you most likely need to install both `SDL2` and `SDL2-devel`.

## Usage

Place the `invaders.e`, `invaders.f`, `invaders.g` and `invaders.h` files on the `game_files` directory and then run the game with:

```
make run
```

The controls are:

- `C` to insert coins
- `Return` to start the game once coins are inserted
- Arrows to move
- `Space` to shoot

Only supports player one for now.

## Tests

The 8080 processor test structure is essentially copied from [this 8080 emulator](https://github.com/superzazu/8080). To run all tests:

```
make run_tests
```

## TODO

Play the game sound.

## Other References

- [Space Invaders Hardware](http://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html)
- [8080 Programmer's Manual](https://altairclone.com/downloads/manuals/8080%20Programmers%20Manual.pdf)
- [8080 Emulator](https://github.com/superzazu/8080)
