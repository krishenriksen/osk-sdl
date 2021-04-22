Lightweight On-Screen-Keyboard based on SDL2
---

Tested on RG351P, RG351M and RG351V

### Building:

This project uses meson, and can be built with:

```
$ meson --prefix=/roms/ports/AnberPorts/bin/ -Dbuildtype=release build
$ meson compile -j4 -C build
```

### Usage

For text input

`osk-sdl -t "Enter Text"`

For password input

`osk-sdl -p "Enter Password"`

### Controls

d-pad controls which character is selected, and movement will highlight the char.

- A button, will input the selected character.
- B button, will add a space.
- X button, will simulate backspace.
- Y button, will toggle capslock.
- R1, will toggle show password

- SELECT button, will quit without outputting input string.
- START button, will output the current input to current tty

### Support the project

[![Donate](https://github.com/krishenriksen/AnberPorts/raw/master/donate.png)](https://www.paypal.me/krishenriksendk)
[![Patreon](https://github.com/krishenriksen/AnberPorts/raw/master/patreon.png)](https://www.patreon.com/bePatron?u=54003740)
[![Sponsor](https://github.com/krishenriksen/AnberPorts/raw/master/sponsor.png)](https://github.com/sponsors/krishenriksen)
