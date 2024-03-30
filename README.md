# ppl-synth

The sound synthesizer used in PewPew Live.

Heavily based on and compatible with [jfxr](https://jfxr.frozenfractal.com/).

## Repo organisation

* `src/` contains the code for the library.
* `example/` contains a minimal example that generates two pcm files.

## Using the library

### Step 1: Define a configuration:
```cpp
SynthesizerConfig config;
config.frequency_ = 200;
config.sustain_ = 0.04;
config.decay_ = 0.14;
config.frequency_jump1_amount_ = 60;
config.wave_generator_type_ = SynthesizerConfig::SAWTOOTH;
```
The values for the configuration can be found by using [jfxr](https://jfxr.frozenfractal.com/).

There complete list of supported fields can be found in the source code for [SynthesizerConfig](https://github.com/pewpewlive/ppl-synth/blob/master/src/synthesizer_config.h).

### Step 2: Synthetize the sound using said configuration:
```cpp
auto pcm_data = Synthesize(config);
```

## Building and running the example

`example/` contains an example usage of the library.

You build the example with CMake.

To build from the command line:
```
mkdir out
cd out
cmake ..
make
```

Executing the binary results in the creation of 2 raw pcm files.
Those raw pcm file can be played with ffplay:
```
ffplay -f s16le -ar 22k -ac 1 out_16bit.pcm
```
