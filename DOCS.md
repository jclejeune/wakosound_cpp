# WakoSound — Documentation technique

> C++20 · Qt6 · PortAudio · libsndfile

---

## Architecture

```
wako::audio      — moteur audio (cache, voix, pitch par vitesse)
wako::model      — modèle de données (kits, pads)
wako::seq        — séquenceur (pattern, moteur temps réel)
wako::ui         — interface Qt (fenêtre, grilles, transport)
wako::ui::icons  — icônes SVG et font LCD
```

---

## wako::audio

### `AudioBuffer` *(struct)*

Buffer audio en mémoire. Données en `float` stereo interleaved (L R L R…).

| Champ | Type | Description |
|-------|------|-------------|
| `samples` | `vector<float>` | Données PCM interleaved |
| `channels` | `int` | Toujours 2 (stereo) |
| `sampleRate` | `int` | Hz (typiquement 44100) |
| `frameCount` | `int64_t` | Nombre de frames |
| `empty()` | `bool` | Vrai si pas de données |

---

### `AudioCache`

Singleton. Charge les fichiers WAV/FLAC **une seule fois** en RAM. Thread-safe.

#### `static AudioCache& instance()`
Retourne l'instance unique.

#### `const AudioBuffer* get(const string& filePath)`
Retourne le buffer depuis le cache. Le charge si absent. Thread-safe double-check.
Retourne `nullptr` si le fichier est invalide.

#### `void preload(const vector<string>& paths)`
Précharge une liste de fichiers. Appelé au démarrage et au changement de kit.

#### `void clear()`
Vide le cache.

---

### `VoicePool`

Pool de 512 voix audio fixes. Zéro allocation dans le callback PortAudio.

#### Pitch par vitesse de lecture
Le pitch shifting est purement mécanique : `pos += pitchFactor` à chaque frame,
avec `pitchFactor = 2^(semitones/12)`. Identique au principe d'un sampler MPC.
- Pitch up → lecture plus rapide (durée raccourcie)
- Pitch down → lecture plus lente (durée allongée)
- Zéro latence, zéro dépendance externe
- Interpolation linéaire entre frames consécutives pour éviter les artefacts

#### `int play(const AudioBuffer* buffer, float volume, bool gate, int padIdx, int pitch)`
Démarre une voix. Voice stealing si toutes les 512 voix sont occupées.

#### `void stop(int voiceId)`
Fade-out 64 frames sur la voix identifiée.

#### `void mix(float* out, unsigned long frames, float masterVolume)`
**Callback RT** — mélange toutes les voix, applique les effect chains par track
puis la chain master. Aucune allocation. Met à jour les peak meters.

---

### `Player`

Singleton. Wraps PortAudio + `VoicePool`.

#### `bool init(int sampleRate = 44100, int framesPerBuffer = 512)`
Initialise PortAudio, ouvre le stream stéréo float32.

#### `int play(const string& filePath, float volume, int pitch, bool gate, int padIdx)`
Charge depuis `AudioCache` (toujours en cache), lance la voix avec pitchFactor.

#### `void startRecording(int totalFrames)` / `bool stopRecording(const string& path)`
Enregistrement direct de la sortie PortAudio dans un buffer pré-alloué.
`stopRecording` écrit le buffer en WAV PCM16 via libsndfile.

#### `EffectChain& trackChain(int pad)` / `EffectChain& masterChain()`
Accès aux chaînes d'effets (tracks 0–8, master = index 9).

---

### `Exporter`

Render offline du pattern vers un fichier WAV. Simule le séquenceur step par step
sans PortAudio. Le pitch utilise la même mécanique que `VoicePool` (vitesse variable),
garantissant un rendu identique au live.

#### `static ExportResult render(...)`
- Simule `loops × patternLength` steps
- Mix par step avec interpolation linéaire + pitchFactor
- Applique les EffectChains (tracks + master) et le masterVolume
- Normalise si clipping, écrit en WAV PCM16 via libsndfile

---

## wako::model

### `Pad` *(struct)*

| Champ | Type | Description |
|-------|------|-------------|
| `name` | `string` | Nom affiché |
| `filePath` | `string` | Chemin absolu du fichier audio |
| `volume` | `float` | Volume 0.0–1.0 |
| `enabled` | `bool` | Pad actif |

---

### `KitManager`

Deux sources : kits factory (`kits.json`, jamais réécrits) + kits utilisateur.

#### `bool loadFactory(const string& jsonPath)`
#### `void loadUser(const string& jsonPath)`
#### `bool setPadFile(int padIdx, const string& filePath, const string& name)`
Retourne `false` si le kit courant est factory.

#### `bool saveUserKits() const`
Écrit uniquement les kits `!isFactory`.

---

## wako::seq

### `Pattern` *(struct)*

Grille `[9][32]` de `StepData`. **Constantes :** `MAX_PADS = 9`, `MAX_STEPS = 32`

**Par track :** `trackLengths[9]` (polymétrie), `trackVolumes[9]`, `muted`, `soloed`, `trackGate`

#### `vector<int> advance()`
Avance chaque track sur sa propre longueur. Retourne les pads actifs avant l'avance.

#### `static int stepIntervalMs(int bpm)`
`(60 × 1000) / (bpm × 4)` — base double-croches. Minimum 1 ms.

---

### `Engine`

Boucle RT sur thread dédié. Horloge absolue anti-drift.
```
target = startTime + tickCount × interval
sleep  = target - now
```
Rebase automatique si BPM change. `sleepInterruptible` en chunks 50ms (réactif à `stop()`).

---

## wako::ui

### `StepGrid`

Grille 9×32, dessin custom `QPainter`.

**Layout colonnes :** `[ LABEL 65px ][ G 18px ][ M 18px ][ S 18px ][ grille... ]`

**`stepAtX` retourne :** `≥0` = step, `-2` = G, `-3` = M, `-4` = S, `-1` = ailleurs

| Geste | Action |
|-------|--------|
| Clic gauche | Toggle step |
| Drag V | Volume (haut = fort) |
| Drag H | Pitch (±12 semitones) |
| Clic droit | Toggle gate step |
| Drag droit | Longueur track |
| Double-clic label | Reset longueur |

---

### `MixerPanel`

Sliders stockés dans `trackSliders_[]` et `masterSlider_` (pas de `findChildren`).
`resetAll()` : détruit les EffectWindows + reset sliders + reset chains.

### `EffectWindow`

Signal `closed()` émis dans `closeEvent()` → synchronise le bouton FX.

### `RenderPanel`

Enregistre la sortie PortAudio live via `Player::startRecording/stopRecording`.

---

## Format JSON pattern

```json
{
  "bpm": 120,
  "length": 16,
  "trackLengths": [16, 12, 8, 16, 16, 16, 16, 16, 16],
  "muted":     [false, false, true, false, ...],
  "soloed":    [false, false, false, false, ...],
  "trackGate": [false, true, false, false, ...],
  "grid": [
    [true, false, {"a":true,"v":0.8,"p":3,"g":true}, false, ...],
    ...
  ]
}
```

---

## Dépendances

| Lib | Licence | Usage |
|-----|---------|-------|
| Qt6::Widgets | LGPL v3 | UI |
| Qt6::Svg | LGPL v3 | Icônes SVG |
| PortAudio | MIT | Stream audio RT |
| libsndfile | LGPL v2.1 | Lecture/écriture WAV/FLAC/AIFF |
| nlohmann/json | MIT | Sérialisation patterns et kits |

Stack 100% MIT/LGPL. Aucun submodule, aucune dépendance propriétaire.