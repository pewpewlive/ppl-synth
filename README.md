# ppl-synth

The sound synthesizer used in PewPew Live.

* `src/` contains the code for the library.

## Example

`example/` contains an example usage of the library.

You build with CMake. To build from the command line:
```
mkdir out
cd out
cmake ..
make
```

The binary outputs a raw pcm file that can be played with ffplay:
```
ffplay -f s16le -ar 22k -ac 1 out_16bit.pcm
```