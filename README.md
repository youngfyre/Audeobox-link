# Aueobox

This is a minimal JUCE plugin shell that opens a window and draws `aueobox` in the center.

## What it builds

- `VST3`
- `Standalone`

## What you need

1. Install [CMake](https://cmake.org/download/).
2. Install Visual Studio with the C++ desktop workload.
3. Download JUCE and place it at one of these locations:
   - `C:\Users\young\OneDrive\Documents\Aueobox\JUCE`
   - anywhere else, then pass `-DJUCE_DIR=...` to CMake

## Generate a Visual Studio project

```powershell
cmake -S . -B build -DJUCE_DIR="C:\path\to\JUCE"
cmake --build build --config Debug
```

If JUCE is inside the project at `Aueobox\JUCE`, you can omit `-DJUCE_DIR`.

## First next steps

- Add knobs or buttons in `Source/PluginEditor.cpp`
- Add DSP in `Source/PluginProcessor.cpp`
- Change plugin metadata in `CMakeLists.txt`
