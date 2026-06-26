# Specification fonctionnelle et architecture

Ce document decrit l'application pour un agent de developpement. Il sert de carte du projet avant modification du firmware.

## Objectif

Le projet transforme un ESP32-S3 avec USB natif en peripherique HID composite clavier/souris. Il expose aussi un point d'acces WiFi local avec une interface Web de configuration.

L'application doit rester utilisable sans outil externe apres flash :

- les boutons physiques declenchent des actions HID ;
- les textes, GPIO et parametres WiFi sont modifiables depuis l'interface Web ;
- les parametres sont persistants via NVS ;
- un fichier de parametrage permet d'importer/exporter les parametres et les paires de texte ;
- le firmware peut etre mis a jour par OTA depuis l'interface Web.

## Fonctionnalites

### USB HID

- Clavier HID USB natif via `USBHIDKeyboard`.
- Souris HID USB native via `USBHIDMouse`.
- Demonstration de demarrage optionnelle :
  - deplacement relatif approximatif vers le centre ecran ;
  - clic gauche ;
  - saisie du texte configure.
- La demonstration est desactivee par defaut.
- Toute sequence de saisie doit finir par `Keyboard.releaseAll()` pour eviter les touches ou modificateurs bloques cote hote.

### Saisie clavier AZERTY

- Le firmware cible un hote Windows configure en AZERTY francais.
- La table `rawKeyForAzertyAsciiChar()` convertit les caracteres imprimables ASCII en codes HID physiques.
- Les sequences echappees sont gerees par `typeEscapedSequence()` :
  - `\t` : tabulation ;
  - `\r` et `\n` : Entree ;
  - `\b` : retour arriere ;
  - `\e` : Echap ;
  - `\\` : antislash ;
  - `\^` : caret ;
  - `\cA` a `\cZ` : Ctrl-A a Ctrl-Z.

### Boutons physiques

Les boutons sont cables entre GPIO et GND. Les entrees utilisent `INPUT_PULLUP`.

- Bouton ecriture, par defaut GPIO14 :
  - ecrit le second champ de la paire selectionnee ;
  - s'il n'y a aucune paire chargee, ecrit `typeAzertyButtonText`.
- Bouton action Win+R, par defaut GPIO4 :
  - ouvre la boite Executer Windows avec `Win+R` ;
  - saisit `actionButtonText` ;
  - valide avec Entree.
- Bouton WiFi, par defaut GPIO5 :
  - active ou coupe le point d'acces de configuration.

### Interface Web

Le point d'acces WiFi par defaut est :

- SSID : `ESP32-HID-Config` ;
- mot de passe : `esp32hid` ;
- URL : `http://192.168.4.1/`.

L'interface Web permet de modifier :

- le SSID du point d'acces ;
- le mot de passe du point d'acces ;
- le GPIO du bouton ecriture ;
- le GPIO du bouton action Win+R ;
- la commande du bouton action ;
- le GPIO du bouton WiFi ;
- l'activation et le texte de la demo de demarrage ;
- le texte manuel d'ecriture ;
- la paire selectionnee ;
- la liste de paires via ajout manuel ou import de fichier ;
- le firmware via upload OTA.

Elle affiche aussi :

- l'etat courant des GPIO de boutons (`repos` ou `appuye`) ;
- le dernier rapport d'import ;
- un journal des evenements recents.

### Fichier de parametrage

Le fichier de parametrage contient deux types de lignes :

- cles reservees `app.*` pour les parametres applicatifs ;
- autres lignes pour les paires HID `nom=texte`.

Exemple :

```text
app.apSsid=ESP32-HID-Config
app.apPass=esp32hid
app.typePin=14
app.actionPin=4
app.wifiPin=5
app.typeText=abc
app.actionText=wt
app.demoOn=0
app.demoText=salut
app.selectedPair=0

nom=texte-a-ecrire
```

Cles supportees :

- `app.apSsid` : SSID du point d'acces ;
- `app.apPass` : mot de passe du point d'acces ;
- `app.typePin` : GPIO du bouton ecriture ;
- `app.actionPin` : GPIO du bouton action Win+R ;
- `app.terminalPin` : ancien nom accepte pour compatibilite avec `app.actionPin` ;
- `app.wifiPin` : GPIO du bouton WiFi ;
- `app.typeText` : texte manuel d'ecriture ;
- `app.actionText` : commande saisie apres `Win+R` ;
- `app.demoOn` : activation de la demo de demarrage (`1`, `0`, `true`, `false`, `oui`, `non`) ;
- `app.demoText` : texte de demo ;
- `app.selectedPair` : index de paire selectionnee.

Contraintes :

- les separateurs acceptes sont tabulation, `;`, `,`, `=` ;
- les lignes commencant par `#` sont ignorees ;
- les separateurs dans les champs doivent etre echappes avec `\` ;
- les champs de paires sont bornes par `MAX_PAIR_FIELD_LENGTH` ;
- la liste de paires est bornee par `MAX_WORD_PAIRS` ;
- les erreurs d'import doivent apparaitre dans `lastImportReport`.

### LED WS2812

La LED integree est pilotee par `neopixelWrite()`.

Convention actuelle :

- bleu fixe : WiFi de configuration actif ;
- rouge : saisie HID en cours ;
- vert bref : action reussie ;
- rouge bref : erreur, liste pleine ou arret WiFi ;
- violet : OTA en cours ;
- rouge clignotant : Caps Lock actif cote hote.

## Architecture

### Fichiers

- `platformio.ini` : configuration PlatformIO et flags USB natif Arduino.
- `include/hid_config.h` : constantes modifiables sans toucher la logique.
- `src/main.cpp` : firmware complet.
- `examples/parametrage_paires.txt` : exemple de fichier de parametrage.
- `README.md` : documentation utilisateur.
- `.venv/` : environnement Python local contenant PlatformIO. Il est ignore par Git.

### Organisation de `src/main.cpp`

Le fichier est volontairement monolithique pour rester simple sur microcontroleur.

Zones principales :

- constantes HID et etat global ;
- helpers LED et journal ;
- helpers HTML et format de fichier ;
- configuration des GPIO ;
- gestion des paires ;
- parsing du fichier de parametrage ;
- chargement/sauvegarde NVS ;
- routes HTTP ;
- WiFi AP ;
- souris HID ;
- clavier HID AZERTY ;
- `setup()` ;
- `loop()`.

### Flux de demarrage

1. `setup()` initialise `Serial`.
2. `loadSettings()` restaure les parametres depuis NVS.
3. La LED est eteinte.
4. Le clavier, la souris et l'USB natif sont initialises.
5. Le firmware attend `USB_ENUMERATION_DELAY_MS`.
6. Le point d'acces Web demarre.
7. Les etats initiaux des boutons sont lus.
8. Un evenement de demarrage est ajoute au journal.

### Boucle principale

`loop()` execute :

1. la demo de demarrage si elle est activee et pas encore executee ;
2. la detection du bouton ecriture ;
3. la detection du bouton action ;
4. la detection du bouton WiFi ;
5. le traitement HTTP si le WiFi est actif ;
6. la gestion LED, notamment Caps Lock ;
7. un court `delay(10)`.

### Persistance NVS

Namespace : `hidcfg`.

Cles principales :

- `typePin` ;
- `termPin` ;
- `wifiPin` ;
- `typeText` ;
- `actionText` ;
- `demoOn` ;
- `demoText` ;
- `apSsid` ;
- `apPass` ;
- `pairCount` ;
- `selected` ;
- `p{index}a` et `p{index}b` pour les paires.

Note : la cle NVS `termPin` reste conservee pour compatibilite interne, meme si l'interface parle maintenant de bouton action.

## Points d'extension recommandés

### Ajouter un nouveau parametre applicatif

1. Ajouter une constante par defaut dans `include/hid_config.h` si necessaire.
2. Ajouter une variable globale dans `src/main.cpp`.
3. Charger dans `loadSettings()`.
4. Sauvegarder dans `saveSettings()`.
5. Ajouter le champ dans `sendConfigPage()`.
6. Lire le champ dans `applyWebSettings()`.
7. Ajouter la cle `app.*` dans `applyAppConfigEntry()`.
8. Exporter la cle dans `handleDownloadPairs()`.
9. Documenter dans `README.md` et `examples/parametrage_paires.txt`.

### Ajouter une action HID

1. Preferer une action configurable a une action codee en dur.
2. Encapsuler les appuis dans une fonction dediee.
3. Toujours terminer par `releaseKeyboardState()`.
4. Ajouter un evenement via `addEventLog()`.
5. Donner un retour LED coherent.

### Modifier la table clavier

1. Travailler dans `rawKeyForAzertyAsciiChar()`.
2. Garder la difference entre lettres de controle et lettres affichees :
   - `rawKeyForControlLetter()` utilise les codes HID physiques A-Z pour Ctrl ;
   - `rawKeyForAzertyAsciiChar()` cible le caractere affiche en AZERTY.
3. Tester au minimum minuscules, majuscules, chiffres, ponctuation, `\t`, `\r`, `\cC`, `\cD`.

## Invariants a respecter

- Ne jamais laisser une touche HID enfoncee apres une action : appeler `releaseKeyboardState()`.
- Ne pas utiliser le GPIO de la WS2812 comme bouton.
- Ne pas brancher les boutons sur `3V3`.
- Ne pas bloquer longtemps dans une route HTTP sauf signal LED court ou OTA.
- Garder l'import tolerant, mais toujours reporter les lignes ignorees.
- Conserver la compatibilite de `app.terminalPin` tant que des anciens fichiers de parametrage peuvent exister.
- Ne pas augmenter fortement `MAX_WORD_PAIRS` sans revoir l'usage NVS.

## Verification attendue

Apres modification firmware :

```powershell
.\.venv\Scripts\platformio.exe run
```

Si le terminal a active `.venv`, la commande courte `pio run` est equivalente.

Tests fonctionnels conseilles sur carte :

- connexion au point d'acces WiFi ;
- affichage de la page Web ;
- sauvegarde d'un GPIO et redemarrage ;
- import d'un fichier valide ;
- import d'un fichier avec une cle inconnue ;
- bouton ecriture avec texte finissant par majuscule ;
- bouton action avec commande `wt` ;
- bouton WiFi on/off ;
- OTA avec `firmware.bin`.
