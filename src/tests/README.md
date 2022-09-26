# tests

automated unit testing folder

commented out tests are tests that currently fail. for this reason, this is more of a regression test.

i do plan to convert the output of ctest to this readme in a nice easy to read table.

the .png in roms_and_output are the output from this emulator.

---

## running a new test

- `argv[1]` = test rom path

- `argv[2]` = number of frames to run

- `argv[3]` = 0=load output.png and compare, 1=write new output.png

- `argv[4]` = (optional) button;frame pair that looks like this: `"UP;30;START;60;B;240"`, this will press the up button after 30 frames, start after 60 frames, b after 240 frames. NOTE: each button is pressed for a single frame and then released!

---

## sources

- https://github.com/destoer/armwrestler-gba-fixed/

- https://gbdev.gg8.se/files/roms/blargg-gb-tests/

- https://github.com/jsmolka/gba-tests

- https://github.com/nba-emu/hw-test

- https://gekkio.fi/files/mooneye-test-suite/

- https://github.com/mgba-emu/suite
