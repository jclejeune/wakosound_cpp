# WakoSound

Sampler MPC + séquenceur pas-à-pas. C++20, Qt6, PortAudio, libsndfile.

> Projet personnel — première app C++, assisté par IA.

---

## Fonctionnalités

- **Sampler** — grille 3×3 MPC, déclenchement clavier AZERTY
- **Browser** — navigateur de samples (wav, mp3, flac, aiff), drag → pad
- **Séquenceur** — 9 tracks × 32 steps, polymétrie, pitch/volume/gate par step
- **Mixage** — volume par track, master, VU-mètres animés 30 fps
- **Effets** — par channel : Saturation (Tube / Transistor / Fuzz) → EQ 5 bandes → Reverb → Delay
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

Aucun submodule — toutes les dépendances sont dans les dépôts système.

---

## Build

```bash
git clone https://github.com/jclejeune/wakosound_cpp.git
cd wakosound

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Exécutable : `build/WakoSound`
Sons : `build/sounds/`
Kits : `build/kits.json`

---

## Raccourcis clavier

| Touche            | Action                            |
|-------------------|-----------------------------------|
| `A` `Z` `E`       | Pads 1 2 3 (ligne haute)          |
| `Q` `S` `D`       | Pads 4 5 6 (ligne milieu)         |
| `W` `X` `C`       | Pads 7 8 9 (ligne basse)          |
| `Space` / `L`     | Play / Stop                       |
| `K`               | Step suivant (séquenceur arrêté)  |
| `J`               | Step précédent (séquenceur arrêté)|

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

> Le pitch est un **variateur de vitesse de lecture** (style MPC/sampler).
> Pitch up = lecture plus rapide, pitch down = plus lente. Zéro latence, zéro dépendance externe.

---

## Licences

| Composant     | Licence   |
|---------------|-----------|
| Qt6           | LGPL v3   |
| PortAudio     | MIT       |
| libsndfile    | LGPL v2.1 |
| nlohmann/json | MIT       |

Stack 100% MIT/LGPL — projet commercialisable sans restriction.