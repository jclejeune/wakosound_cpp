# WakoSound — Roadmap

## v1.0 ✅ (terminé)

- Sampler MPC 9 pads, browser sidebar, drag → pad
- Séquenceur 9×32 steps, polymétrie par track
- Effets par channel : Sat (Tube/Transistor/Fuzz) → EQ5 → Reverb → Delay
- Console mixage + VU-mètres 30fps
- Export WAV (enregistrement direct sortie PortAudio)
- Raccourcis AZERTY + Space/L + J/K
- Reset complet (pattern + mixer + effets)
- Stack 100% MIT/LGPL (commercialisable)

---

## Pistes futures

### Distribution
- AppImage Linux (linuxdeploy + plugin-qt)
- Paquet .deb Ubuntu
- Flatpak / Flathub

### Séquenceur
- Patterns multiples (A/B/C…) + arrangement linéaire
- Humanisation (swing, vélocité aléatoire, micro-timing)
- Steps ternaires (base /3 en plus de /4)
- MIDI out (envoyer les steps vers des synths externes)

### Audio
- Resampling de qualité (actuellement interpolation linéaire dans VoicePool)
- Compresseur/limiteur sur la chain master
- Sidechain kick → compressor

### UI
- Zoom sur la StepGrid (afficher moins de tracks, plus grand)
- Thèmes (clair / personnalisé)
- Undo/Redo (historique des modifications pattern)

### Kits
- Import kit depuis dossier (auto-assign pads par ordre alphabétique)
- Prévisualisation waveform dans le browser