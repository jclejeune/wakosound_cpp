# WakoSound

Sampler MPC + séquenceur pas-à-pas. C++20, Qt6, PortAudio, libsndfile, signalsmith-stretch.

> Projet personnel — première app C++, assisté par IA.

---

## Fonctionnalités

- **Sampler** — grille 3×3 MPC, déclenchement clavier numpad
- **Browser** — navigateur de samples (wav, mp3, flac, aiff), drag → pad
- **Séquenceur** — 9 tracks × 32 steps, polymétrie, pitch/volume/gate par step
- **Mixage** — volume par track, master, VU-mètres animés 30 fps
- **Effets** — par channel : Saturation (Tube / 303-808 / Fuzz) → EQ 5 bandes → Reverb → Delay
- **Render** — export WAV par enregistrement direct de la sortie audio

---

## Dépendances

```bash
# Ubuntu / Debian
sudo apt install cmake ninja-build qt6-base-dev qt6-svg-dev \
  libportaudio2 portaudio19-dev libsndfile1-dev \
  nlohmann-json3-dev

# Arch
sudo pacman -S cmake qt6-base qt6-svg portaudio libsndfile nlohmann-json

# macOS
brew install cmake qt6 portaudio libsndfile nlohmann-json
```

> signalsmith-stretch est inclus via git submodule — pas besoin de l'installer.

---

## Build

```bash
# Cloner avec les dépendances
git clone --recurse-submodules https://github.com/toi/wakosound.git
cd wakosound

# Ou si déjà cloné sans --recurse-submodules
git submodule update --init --recursive

# Compiler
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Exécutable : `build/WakoSound`
Sons : `build/sounds/`
Kits : `build/kits.json`

---

## Raccourcis

| Touche     | Action       |
|------------|--------------|
| Numpad 7–9 | Pads ligne 1 |
| Numpad 4–6 | Pads ligne 2 |
| Numpad 1–3 | Pads ligne 3 |

---

## Gestes séquenceur

| Geste                  | Action                        |
|------------------------|-------------------------------|
| Clic gauche            | Toggle step                   |
| Drag gauche vertical   | Volume du step                |
| Drag gauche horizontal | Pitch du step (±12 semitones) |
| Clic droit pur         | Toggle gate du step           |
| Drag droit horizontal  | Longueur de la track          |
| Double-clic sur label  | Reset longueur                |
| G / M / S              | Gate / Mute / Solo par track  |

---

## Licences

| Composant | Licence |
|-----------|---------|
| Qt6 | LGPL v3 |
| PortAudio | MIT |
| libsndfile | LGPL v2.1 |
| signalsmith-stretch | MIT |
| nlohmann/json | MIT |