# emscripten-port

the src is very similar to the base sdl2 port.
i am not a fan of having #ifdef everywhere in the code, so i decided to make 2 seperate code paths

## how the audio works

so the idea is simple. if the audio thread doesnt have enough samples, it'll run until it has enough samples.
so both the main thread and audio thread can run the emu core.

it's the same code i used in my totalgb emulator, which produced good results on every browser. downside is if too many samples are generated, then samples are simply dropped, which is not ideal, and will sound awful.
