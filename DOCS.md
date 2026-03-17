# WakoSound — Documentation technique

> C++20 · Qt6 · PortAudio · libsndfile · RubberBand

---

## Architecture

```
wako::audio      — moteur audio (cache, voix, pitch shifting)
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

#### `size_t size() const`
Nombre de fichiers en cache.

---

### `PitchCache`

Singleton. Pitch shifting **offline** via RubberBand. Cache par `(filePath, semitones)`.

#### `static PitchCache& instance()`
Retourne l'instance unique.

#### `const AudioBuffer* get(const string& filePath, int semitones)`
- `semitones == 0` → délègue directement à `AudioCache` (zéro overhead)
- `semitones != 0` → traitement RubberBand + mise en cache
- Thread-safe avec double-check lock

#### `void clear()`
Vide le cache de buffers pitch-shiftés.

#### `AudioBuffer process(const AudioBuffer& input, int semitones)` *(private)*
Applique RubberBand en mode offline avec durée préservée (`setTimeRatio(1.0)`).
Désentrelace L/R avant traitement, réentrelace après.
`pitchScale = 2^(semitones/12)`

---

### `VoicePool`

Pool de 16 voix audio fixes. Zéro allocation dans le callback PortAudio.

#### `int play(const AudioBuffer* buffer, float volume, bool gate)`
Démarre une voix. Retourne son `id` (pour `stop(id)` en mode gate).
Voice stealing si toutes les voix sont occupées.

#### `void stop(int voiceId)`
Demande un fade-out rapide (16 frames) sur la voix identifiée.

#### `void stopAll()`
Fade-out sur toutes les voix actives.

#### `void mix(float* out, unsigned long frames)`
**Callback RT** — mélange toutes les voix actives dans le buffer de sortie.
Aucune allocation. Gère le fade-out gate.

---

### `Player`

Singleton. Wraps PortAudio + `VoicePool`.

#### `static Player& instance()`
Retourne l'instance unique.

#### `bool init(int sampleRate = 44100, int framesPerBuffer = 256)`
Initialise PortAudio et ouvre le stream stéréo float32.
Retourne `false` si échec (device manquant, etc.).

#### `void shutdown()`
Arrête et ferme le stream PortAudio.

#### `int play(const string& filePath, float volume, int pitch, bool gate)`
Joue un fichier audio.
- `pitch == 0` → `AudioCache` direct
- `pitch != 0` → `PitchCache` (RubberBand)
- `gate == true` → la voix sera coupée au prochain tick gate

Retourne le `voiceId` (utile pour `stop()` en mode gate).

#### `void stop(int voiceId)`
Coupe une voix (fade-out 16 frames).

#### `void stopAll()`
Coupe toutes les voix.

#### `bool isRunning() const`
Vrai si le stream PortAudio est ouvert.

---

## wako::model

### `Pad` *(struct)*

| Champ | Type | Description |
|-------|------|-------------|
| `name` | `string` | Nom affiché |
| `filePath` | `string` | Chemin absolu du fichier audio |
| `volume` | `float` | Volume 0.0–1.0 |
| `enabled` | `bool` | Pad actif |
| `description` | `string` | Optionnel |
| `color` | `string` | Couleur CSS optionnelle |

#### `bool valid() const`
Vérifie que le fichier existe et a une extension audio valide (wav, mp3, aiff, flac).

#### `string displayName() const`
Retourne `name` ou `[name]` si désactivé.

---

### `Kit` *(struct)*

Conteneur de 9 pads maximum (grille 3×3 MPC).

#### `const Pad* pad(int i) const`
Retourne le pad à l'index `i`, ou `nullptr` si hors limites.

#### `int padCount() const` / `bool empty() const` / `bool full() const`
Accesseurs d'état.

---

### `KitManager`

Charge et gère les kits depuis `kits.json`.

#### `bool loadFromFile(const string& jsonPath)`
Parse le JSON. Résout les chemins de fichiers relativement au dossier du JSON.
Retourne `false` si le fichier est manquant ou invalide.

#### `const Kit* currentKit() const`
Retourne le kit courant, ou `nullptr`.

#### `bool switchTo(int index)`
Change le kit courant par index.

#### `bool switchByName(const string& name)`
Change le kit courant par nom.

#### `vector<string> currentKitFilePaths() const`
Retourne les chemins de tous les fichiers du kit courant (pour `AudioCache::preload`).

---

## wako::seq

### `StepData` *(struct)*

Paramètres d'un step individuel dans la grille.

| Champ | Type | Défaut | Description |
|-------|------|--------|-------------|
| `active` | `bool` | `false` | Step activé |
| `volume` | `float` | `1.0` | Volume 0.0–1.0 |
| `pitch` | `int` | `0` | Pitch en semitones (-12 à +12) |
| `gate` | `bool` | `false` | Override gate local (tronque le son) |

#### `bool hasCustomParams() const`
Vrai si `volume != 1.0`, `pitch != 0` ou `gate == true`.
Utilisé pour choisir le format compact ou complet en JSON.

---

### `Pattern` *(struct)*

Grille 2D `[MAX_PADS][MAX_STEPS]` de `StepData`. Contient aussi les états par track.

**Constantes :** `MAX_PADS = 9`, `MAX_STEPS = 32`

**Données par track :**
- `trackLengths[9]` — longueur indépendante par track (1–32)
- `trackSteps[9]` — position courante de lecture par track
- `muted[9]` — track mutée
- `soloed[9]` — track en solo
- `trackGate[9]` — mode gate activé pour toute la track

---

#### Accès

##### `bool get(int pad, int step) const`
Retourne `grid[pad][step].active`. Retourne `false` si hors limites.

##### `const StepData& getStepData(int pad, int step) const`
Accès direct au `StepData` (lecture). Non borné — à utiliser avec des indices valides.

##### `StepData& getStepData(int pad, int step)`
Accès direct en écriture.

##### `bool anySolo() const`
Vrai si au moins une track est en solo.

##### `bool shouldPlay(int pad) const`
Décide si un pad doit jouer selon les règles mute/solo :
- `anySolo() == true` → joue seulement si `soloed[pad]`
- `anySolo() == false` → joue sauf si `muted[pad]`

##### `vector<int> activeNow() const`
Retourne les indices des pads actifs sur leur step courant.

---

#### Mutations (retournent `*this` pour chaînage)

##### `Pattern& set(int pad, int step, bool v)`
Définit l'état actif d'un step.

##### `Pattern& toggle(int pad, int step)`
Inverse l'état actif d'un step.

##### `Pattern& toggleGate(int pad, int step)`
Inverse le gate local d'un step.

##### `Pattern& setStepVolume(int pad, int step, float v)`
Définit le volume d'un step (clampé 0.0–1.0).

##### `Pattern& setStepPitch(int pad, int step, int p)`
Définit le pitch d'un step (clampé -12–+12).

##### `Pattern& toggleTrackGate(int pad)`
Active/désactive le mode gate sur toute la track.

##### `Pattern& toggleMute(int pad)` / `Pattern& toggleSolo(int pad)`
Active/désactive mute ou solo sur une track.

##### `Pattern& clearAll()`
Remet à zéro : toutes les notes, gate, mute, solo, steps courants.

##### `Pattern& clearPad(int pad)`
Efface les notes d'un seul pad et remet son step à 0.

##### `Pattern& setBpm(int b)`
Définit le BPM (clampé 1–9999).

##### `Pattern& setLength(int l)`
Définit la longueur globale et l'applique à toutes les tracks.

##### `Pattern& setTrackLength(int pad, int l)`
Définit la longueur d'une track individuelle (polymétrie).

---

#### Timing / avance

##### `vector<int> advance()`
Avance chaque track d'un step sur sa propre longueur.
Retourne les pads actifs **avant** l'avance (ceux qui jouent sur ce tick).

##### `static int stepIntervalMs(int bpm)`
Calcule la durée d'un step en ms pour un BPM donné.
`(60 * 1000) / (bpm * 4)` — base : double-croches.
Résultat minimum : 1 ms.

---

#### Sérialisation

##### `bool saveToFile(const string& path) const`
Sauvegarde en JSON. Format compact (`true`/`false`) si pas de paramètres custom,
format complet (`{"a":true,"v":0.8,"p":3,"g":false}`) sinon.
Inclut `trackLengths`, `muted`, `soloed`, `trackGate`.

##### `static optional<Pattern> loadFromFile(const string& path)`
Charge depuis JSON. Backward compatible : accepte l'ancien format `bool` et le nouveau `{a,v,p,g}`.
Retourne `nullopt` si le fichier est invalide.

---

### `Engine`

Boucle de séquençage sur un thread dédié. Horloge absolue anti-drift.

#### `void start(shared_ptr<Pattern> pattern, shared_ptr<KitManager> kit, StepCallback onStep)`
Démarre la boucle de séquençage.
- `onStep` : callback `void(const TrackSteps&)` appelé à chaque tick depuis le thread RT
- Arrête la boucle précédente si active

#### `void stop()`
Arrête la boucle. Bloque jusqu'à ce que le thread se termine.
Coupe toutes les voix audio actives.

#### `bool isRunning() const`
Vrai si la boucle est active.

---

#### `loop(...)` *(private)*

Boucle principale. Horloge absolue :
```
target = startTime + tickCount * interval
sleep  = target - now
```
Rebase automatique si le BPM change : `startTime = now - tickCount * newInterval`

Utilise `sleepInterruptible` pour rester réactif au `stop()`.

#### `sleepInterruptible(milliseconds, atomic<bool>&)` *(static, private)*

Deux régimes :
- `sleepFor < 5ms` (≈ > 3000 BPM) → `sleep_for` direct — mode générateur de fréquence
- `sleepFor >= 5ms` → chunks de 50ms vérifiés — `stop()` répond en max 50ms

#### `playPads(activePads, currentSteps, kit, pat)` *(private)*

Pour chaque pad actif :
1. Vérifie `pat.shouldPlay(padIdx)` (mute/solo)
2. Lit `StepData` pour le volume et pitch du step
3. Gate = `pat.trackGate[padIdx] || sd.gate`
4. Appelle `Player::play(filePath, volume, pitch, gate)`
5. Stoppe les voix gate des tracks en mode gate avant de jouer

---

## wako::ui

### `PadGrid`

Grille 3×3 de pads MPC. Carrés, centrés, adaptatifs.

#### `PadGrid(QWidget* parent)`
Crée 9 `QPushButton` en disposition MPC (7 8 9 / 4 5 6 / 1 2 3).

#### `void refresh(const Kit* kit)`
Met à jour les labels et états des boutons depuis le kit.

#### `void flashPad(int idx)`
Flash orange 150ms sur le pad. Appelé au clic et par `onSequencerStep`.

#### `void resizeEvent(QResizeEvent*)` *(override)*
Recalcule `padSize = min(availableW/3, availableH/3)`.
Centre la grille via `setContentsMargins` dynamiques.

**Signal :** `padTriggered(int padIdx)`

---

### `StepGrid`

Grille séquenceur 9×32. Dessin custom via `QPainter`.

#### `StepGrid(shared_ptr<Pattern> pattern, QWidget* parent)`
Initialise avec un pattern partagé avec l'Engine.

#### `void setCurrentSteps(const TrackSteps& steps)`
Met à jour le highlight (step rouge) pour chaque track indépendamment. Appelle `update()`.

#### `void setCurrentStep(int step)`
Version globale — applique le même step à toutes les tracks. Utilisé au stop (-1).

#### `void updatePattern(const Pattern* pat)`
Copie le pattern et rafraîchit l'affichage.

#### `void setKit(const Kit* kit)`
Met à jour les noms de pads affichés.

---

#### Helpers de position *(private)*

| Méthode | Retourne |
|---------|----------|
| `stepX0(s)` | Bord gauche du step `s` en pixels |
| `stepX1(s)` | Bord droit du step `s` en pixels |
| `padY0(p)` | Bord haut du pad `p` en pixels |
| `padY1(p)` | Bord bas du pad `p` en pixels |
| `padAtY(y)` | Index du pad sous la coordonnée Y, ou -1 |
| `stepAtX(x)` | Index du step (≥0), -2=G, -3=M, -4=S, -1=ailleurs |

Positions calculées par division entière depuis la taille totale — pas d'accumulation float.

---

#### Gestures souris

| Geste | Action |
|-------|--------|
| Clic gauche | Toggle step actif/inactif |
| Drag gauche vertical | Volume du step (haut = plus fort) |
| Drag gauche horizontal | Pitch du step (-12 à +12 semitones) |
| Clic droit pur | Toggle gate du step (triangle blanc) |
| Drag droit horizontal | Longueur de la track (glisse jusqu'au step cible) |
| Double-clic gauche sur label | Reset longueur à `patternLength` global |
| Clic sur `G` | Toggle gate de toute la track |
| Clic sur `M` | Toggle mute de la track |
| Clic sur `S` | Toggle solo de la track |

---

#### Visuel

- **Step actif** : couleur selon pitch (palette 25 couleurs) × luminosité selon volume
- **Step courant** : rouge `(220, 40, 40)`
- **Hors longueur** : gris très sombre `(40, 40, 40)`
- **Track mutée** : couleurs divisées par 2, label atténué
- **Gate step** : triangle blanc en coin bas-droit de la cellule
- **Marqueur longueur** : ligne bleue verticale après le dernier step actif
- **Bouton G actif** : bleu `(50, 80, 160)`
- **Bouton M actif** : rouge `(180, 50, 50)`
- **Bouton S actif** : jaune `(190, 150, 0)`
- **Texte volume/pitch** : affiché pendant le drag, disparaît au relâchement

**Palette pitch** (index 0 = -12, index 12 = 0, index 24 = +12) :
marron sombre → rouge → gris neutre → or → orange feu → blanc-jaune

---

#### Signaux

| Signal | Déclenchement |
|--------|---------------|
| `stepToggled(pad, step)` | Clic pur gauche |
| `stepGateToggled(pad, step)` | Clic droit pur |
| `stepDataChanged(pad, step, data)` | Fin de drag gauche |
| `trackLengthChanged(pad, length)` | Fin de drag droit |
| `trackGateToggled(pad)` | Clic bouton G |
| `trackMuteToggled(pad)` | Clic bouton M |
| `trackSoloToggled(pad)` | Clic bouton S |

---

### `TransportBar`

Barre de transport toujours visible. Responsive : labels cachés < 550px.

#### `TransportBar(QWidget* parent)`
Construit la barre avec : Save | Open | Kit ▼ | Play | Clear | Step | BPM | Steps

#### `void setPlaying(bool playing)`
Change l'icône et le label du bouton Play/Stop.

#### `void setStep(int step)`
Met à jour le LCD avec `step + 1` (affichage 1-based).

#### `void setKits(const QStringList& names, int currentIndex)`
Remplit le combobox des kits sans déclencher `kitChanged`.

#### `void resizeEvent(QResizeEvent*)` *(override)*
Cache les sections Step/BPM/Steps si largeur < 550px.

**Signaux :** `playStopClicked`, `clearClicked`, `bpmChanged(int)`, `lengthChanged(int)`,
`saveClicked`, `loadClicked`, `kitChanged(int)`

**Debounce BPM :** le signal `bpmChanged` n'est émis que 400ms après la dernière frappe.

---

### `MainWindow`

Fenêtre principale. Orchestre tous les composants.

#### `MainWindow(QWidget* parent)`
Construit l'UI :
1. Transport bar (toujours visible)
2. Tabs : Sampler | Séquenceur | Bibliothèque
3. Connecte tous les signaux

#### `~MainWindow()`
Arrête l'Engine proprement.

#### `void onPlayStop()` *(slot)*
Lance ou arrête le séquenceur.

#### `void stopSequencer()` *(private)*
Arrête l'Engine, remet les steps à 0, remet le LCD à "1".

#### `void onClear()` *(slot)*
- Arrête le séquenceur si en cours
- `pattern_->clearAll()` + `setLength(16)`
- Reset affichage

#### `void onBpmChanged(int bpm)` *(slot)*
Applique le BPM au pattern (rebase Engine automatique au prochain tick).

#### `void onLengthChanged(int len)` *(slot)*
Applique la longueur globale à toutes les tracks.

#### `void onSequencerStep(const TrackSteps& steps)` *(private)*
Callback RT → EDT via `QMetaObject::invokeMethod` + `Qt::QueuedConnection`.
- Met à jour `StepGrid`
- Met à jour le LCD
- Flash les pads actifs sur ce tick (respecte mute/solo)

#### `void keyPressEvent(QKeyEvent*)` *(override)*
Mapping numpad → pads :
```
7 8 9
4 5 6
1 2 3
```

---

## wako::ui::icons

Toutes les fonctions sont `inline` dans `SvgIcons.h`.

#### `QPixmap render(const char* svg, int size, const QString& color)`
Rend un SVG avec remplacement de `%%COLOR%%` en `QPixmap`.

#### `QIcon icon(const char* svg, int size, const QString& color)`
Wrapper `render` → `QIcon`.

#### `QFont lcdFont(int size)`
Charge `lcd.ttf` depuis les ressources Qt (`:/resources/fonts/lcd.ttf`).
Lazy-init avec `fontId` statique. Fallback Monospace Bold si la font est absente.

**Icônes disponibles :** `PLAY`, `STOP`, `PAUSE`, `CLEAR`, `SAVE`, `FOLDER_OPEN`,
`MUSIC_NOTE`, `GRID`, `LIBRARY`

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

Backward compatible : les cellules `true`/`false` (ancien format sans paramètres custom)
sont chargées avec `volume=1.0, pitch=0, gate=false`.

---

## Dépendances

| Lib | Usage |
|-----|-------|
| Qt6::Widgets | UI |
| Qt6::Svg | Rendu icônes SVG inline |
| PortAudio | Stream audio temps réel |
| libsndfile | Lecture WAV/FLAC/AIFF |
| RubberBand | Pitch shifting durée préservée |
| nlohmann/json | Sérialisation patterns et kits |