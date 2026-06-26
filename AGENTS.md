# Consignes pour agents de developpement

Ce depot contient un firmware PlatformIO/Arduino pour ESP32-S3 utilisant l'USB natif comme peripherique HID clavier/souris, avec une interface Web WiFi de configuration.

La specification fonctionnelle et l'architecture detaillees sont dans `docs/agent_development_spec.md`. Lisez ce document avant toute modification non triviale.

## Portee du projet

- Firmware principal : `src/main.cpp`.
- Constantes configurables : `include/hid_config.h`.
- Configuration de build : `platformio.ini`.
- Documentation utilisateur : `README.md`.
- Exemple de parametrage : `examples/parametrage_paires.txt`.

Ne modifiez pas `server/` ou `web/` sauf demande explicite : ces dossiers ne font pas partie du firmware PlatformIO actuel.

## Regles de modification

- Garder le firmware simple et lisible ; eviter les abstractions inutiles.
- Conserver la compatibilite des fichiers de parametrage existants, notamment `app.terminalPin`.
- Mettre a jour ensemble :
  - la logique firmware ;
  - les cles NVS ;
  - l'interface Web ;
  - l'export/import `app.*` ;
  - `README.md` ;
  - `examples/parametrage_paires.txt`.
- Ne pas utiliser le GPIO de la WS2812 comme bouton.
- Les boutons sont cables entre GPIO et GND avec `INPUT_PULLUP`.
- Toute action clavier HID doit terminer par `releaseKeyboardState()` ou `Keyboard.releaseAll()`.
- Les erreurs d'import doivent etre visibles dans le rapport Web du dernier import.
- Les evenements importants doivent etre ajoutes au journal Web avec `addEventLog()`.

## Conventions HID

- Le clavier cible Windows en disposition AZERTY francais.
- `rawKeyForAzertyAsciiChar()` convertit les caracteres affiches en touches physiques.
- `rawKeyForControlLetter()` sert aux raccourcis Ctrl-lettre et utilise les codes physiques A-Z.
- Ne confondez pas ces deux mappings.

## LED WS2812

Conserver la convention actuelle :

- bleu fixe : WiFi de configuration actif ;
- rouge : saisie HID en cours ;
- vert bref : succes ;
- rouge bref : erreur, liste pleine ou arret WiFi ;
- violet : OTA en cours ;
- rouge clignotant : Caps Lock actif.

## Validation

Apres modification du firmware, compiler avec PlatformIO :

```powershell
.\.venv\Scripts\platformio.exe run
```

Si le `.venv` est active dans le terminal, la commande courte est equivalente :

```powershell
pio run
```

Le binaire OTA attendu par l'interface Web est genere dans `.pio\build\esp32s3-usb-hid\firmware.bin` apres compilation.

## Git

- Ne pas forcer le push.
- Ne pas supprimer `LICENSE`.
- Ignorer les fichiers de parametrage telecharges a la racine (`parametrage_*.txt`).
- Verifier `git status --short` avant de conclure.
