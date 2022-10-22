# ppl-synth

The sound synthesizer used in PewPew Live.

* `src/` contains the code for the library.
* `example/` contains an example usage of the library. It outputs a raw pcm file that can be played with ffplay:
```
ffplay -f s16le -ar 22k -ac 1 out_16bit.pcm
```