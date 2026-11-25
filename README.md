# Gold Box Vita

Standalone PS Vita builds of SSI's Gold Box games. Each game gets its own
LiveArea bubble and boots straight into the game: no DOS prompt, no
RetroArch, no launcher. Built on DOSBox, with touch buttons in the side
strips beside the 4:3 picture.

**Playable now:** Gateway to the Savage Frontier (DOS, GOG version).
**Planned:** Treasures of the Savage Frontier, then the Pool of Radiance and
Krynn series.

**No game files are included.** You need your own copy of the game (GOG sells
it). This project is not affiliated with SSI, TSR or Wizards of the Coast.

## Install (Gateway)

You need a Vita running HENkaku/Ensō with VitaShell.

1. Download `gateway.vpk` from the
   [Releases](../../releases) page and install it with VitaShell.
2. Copy the contents of your GOG Gateway folder (the one with `GAME.EXE`
   and the `.DAX` files) to `ux0:data/goldbox/gateway/` on the Vita.
3. Launch the bubble.

Saves go in `ux0:data/goldbox/gateway/save/`.

### Optional: the Adventurer's Journal on screen

The game tells you to read numbered entries in the printed Journal. With the
page images in place, the Vita opens the entry for you when the game cites
it, and Select shows every entry recorded so far, the intro story and the map.

The images are cut from your own copy of `Journal.pdf`, which GOG ships in
the game folder next to `GAME.EXE`. They can't be included in the download
because the Journal is SSI's text. Making them takes a few minutes on a
Windows PC or a Mac:

1. **Get the tool.** On this page, click **Code → Download ZIP** and unzip it.
2. **Install Python** (free) from
   [python.org](https://www.python.org/downloads/). On Windows, tick
   **Add python.exe to PATH** in the installer.
3. **Find Journal.pdf.**
   - GOG Galaxy: open the game in your Library, click the settings icon next
     to Play, then **Manage installation → Show folder**.
   - GOG offline installer on Windows: by default
     `C:\GOG Games\Gateway to the Savage Frontier\`, or wherever you chose.
4. **Run the tool.** Open a terminal in the unzipped folder (Windows: open
   the folder, type `cmd` in the address bar and press Enter; Mac: Terminal,
   then `cd` into the folder) and run:

   Windows:
   ```
   py -m pip install pymupdf pillow
   py tools\journal\make_journal.py gateway "C:\path\to\Journal.pdf"
   ```
   Mac:
   ```
   python3 -m pip install pymupdf pillow
   python3 tools/journal/make_journal.py gateway /path/to/Journal.pdf
   ```
   Tip: you can drag `Journal.pdf` onto the terminal window to paste its path.
5. **Copy to the Vita.** The images land in `games/gateway/journal/` inside
   the unzipped folder. Copy that `journal` folder to
   `ux0:data/goldbox/gateway/journal/` (VitaShell's FTP or USB mode).

## Controls

| Vita | Game |
|---|---|
| D-pad | walk forward/back, turn left/right |
| Left stick | 8-way movement in combat (keypad 1-9) |
| Cross, L1 | Enter |
| Triangle, R1 | Escape (exit/back) |
| Square | move the menu highlight left |
| Circle | Space (stop Quick combat) |
| Start | Vita keyboard; what you type goes to the game |
| Select | Adventurer's Journal |

Touch buttons in the side strips send the game's most-used letter commands
(Q, V, B, C, F, U on the left; A, I, T, E, S, M on the right). They are set
per game in `configs/<game>/overlay.json`, and the buttons above in
`pad.json`.

## Build

Needs [VitaSDK](https://vitasdk.org) with `$VITASDK` set, and CMake.

```
tools/build.sh gateway        # -> build/gateway/gateway.vpk
VITA_IP=<vita ip> tools/deploy.sh gateway   # FTP upload to ux0:/ (VitaShell FTP on)
```

Layout: `dosbox/` is the vendored DOSBox Vita source, `src/overlay/` the
touch buttons, pad mapping, keyboard and Journal, `configs/<game>/` the
per-game DOSBox config, button layout and LiveArea art.

### Debug log

Create an empty file `ux0:data/goldbox/gateway/debug-on` and the next launch
writes `dosbox_debug.txt` beside it (input, speed and timing every 5 s).
Delete the file to switch logging off again.

## Credits

- [DOSBox](https://www.dosbox.com), and rsn8887's DOSBox Vita port
- Northfear's [SDL-1.2-vita](https://github.com/Northfear/SDL-1.2-vita)
- [stb_image](https://github.com/nothings/stb) by Sean Barrett
- talonius's
  [Gateway source reconstruction](https://github.com/talonius/gateway-to-the-savage-frontier),
  used to understand how the game reads keys and prints Journal references

## Licence

GPL-2.0, the same as DOSBox. See `LICENSE`.
