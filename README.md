# fastcut

fastcut lets you cut a fragment of a video with minimal re-encoding. Inspired by [VidCutter](https://github.com/ozmartian/vidcutter) and [avcut](https://github.com/anyc/avcut).

```
Usage: fastcut [options] <input_file> <start_time> <end_time> <output_file>

Options:
  -i <n>       ignore track number n
```

## How to build

### Install dependencies

Ubuntu: `sudo apt install libavcodec-dev libavformat-dev`  
Arch: `sudo pacman -S ffmpeg`

### Build using CMake:

```shell
mkdir build && cd build
cmake ..
cmake --build .
```
