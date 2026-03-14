# WakoSound

Sampler MPC + séquenceur pas-à-pas. C++20, Qt6, PortAudio, libsndfile.

## Dépendances
```bash
# Ubuntu / Debian
sudo apt install cmake ninja-build qt6-base-dev \
  libportaudio2 portaudio19-dev libsndfile1-dev nlohmann-json3-dev

# Arch
sudo pacman -S cmake qt6-base portaudio libsndfile nlohmann-json

# macOS
brew install cmake qt6 portaudio libsndfile nlohmann-json
```

## Build
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Exécutable : `build/WakoSound`
Sons : `build/sounds/`
Kits : `build/kits.json`

## Raccourcis

| Touche      | Action      |
|-------------|-------------|
| Numpad 7–9  | Pads ligne 1 |
| Numpad 4–6  | Pads ligne 2 |
| Numpad 1–3  | Pads ligne 3 |