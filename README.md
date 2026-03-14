# WakoSound C++ — Sampler MPC + Séquenceur

Réécriture de la version Clojure en C++20 avec Qt6 + PortAudio + libsndfile.

## Pourquoi C++ ?

La version Clojure saturait la mémoire à haut BPM : chaque déclenchement
créait un `Clip` Java + `ByteArrayOutputStream` que la GC ne collectait pas
assez vite. À 999 BPM avec 9 pads × 16 steps actifs → ~150 objets/seconde.

En C++, le `VoicePool` est un tableau fixe de 16 voix. Zéro allocation
dans le callback PortAudio : voice stealing au lieu de new/delete.

## Dépendances

```bash
# Ubuntu / Debian
sudo apt install \
  cmake ninja-build \
  qt6-base-dev \
  libportaudio2 portaudio19-dev \
  libsndfile1-dev \
  nlohmann-json3-dev

# Arch
sudo pacman -S cmake qt6-base portaudio libsndfile nlohmann-json

# macOS (Homebrew)
brew install cmake qt6 portaudio libsndfile nlohmann-json
```

## Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

L'exécutable est dans `build/WakoSound`.
`kits.json` est copié dans `build/` automatiquement.
Les fichiers audio doivent être dans `build/sounds/`.

## Structure

```
src/
  audio/
    AudioCache.{h,cpp}   — cache WAV en RAM (charge une fois, lit N fois)
    VoicePool.{h,cpp}    — 16 voix fixes, zéro alloc en callback RT
    Player.{h,cpp}       — wrapper PortAudio, API publique
  model/
    KitManager.{h,cpp}   — chargement kits.json, gestion kit courant
  sequencer/
    Pattern.{h,cpp}      — grille 2D [pad][step], données pures
    Engine.{h,cpp}       — boucle RT drift-free sur thread dédié
  ui/
    MainWindow.{h,cpp}   — fenêtre principale
    PadGrid.{h,cpp}      — grille 3×3 MPC
    StepGrid.{h,cpp}     — grille séquenceur peinte (QPainter)
    TransportBar.{h,cpp} — BPM, steps, mode, play/stop
```

## Mémoire

- Chaque sample est chargé **une seule fois** en `float stereo` dans `AudioCache`.
- Le `VoicePool` pointe vers ces buffers (non-owning) : zéro copie à la lecture.
- 16 voix × ~2 MB (sample typique) = ~32 MB max, constant.
- À 999 BPM avec 144 déclenchements/seconde : voice stealing, RAM stable.

## Format kits

`kits.json` dans le répertoire de l'exécutable.
Les chemins `file` sont relatifs au répertoire de `kits.json`.

## Raccourcis

| Touche    | Action          |
|-----------|-----------------|
| Numpad 7–9 | Pads ligne 1   |
| Numpad 4–6 | Pads ligne 2   |
| Numpad 1–3 | Pads ligne 3   |
