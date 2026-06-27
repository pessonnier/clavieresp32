# ESP32-S3 clavier + souris USB HID

Ce dépôt contient un environnement local PlatformIO pour développer un périphérique USB HID composite avec un **ESP32-S3 WROOM-1** équipé de deux ports USB-C.

La specification fonctionnelle et l'architecture destinees aux agents de developpement sont dans [`docs/agent_development_spec.md`](docs/agent_development_spec.md).

Le firmware démarre le périphérique USB natif de l'ESP32-S3, expose une souris et un clavier HID. La démonstration optionnelle de démarrage exécute une seule fois :

1. déplacement approximatif du pointeur au centre de l'écran ;
2. clic gauche ;
3. saisie du texte `salut`.

Cette démonstration de démarrage est désactivée par défaut. Elle peut être réactivée depuis l'interface Web, et son texte est mémorisé en NVS pour les prochains démarrages.

Deux boutons physiques peuvent ensuite déclencher des actions HID :

- bouton entre le GPIO configurable et `GND` : écrit la chaîne configurable ;
- bouton action configurable, `GPIO4` par défaut : ouvre la boîte Exécuter Windows avec `Win+R`, saisit la commande configurée, puis valide avec `Entrée` ;
- bouton WiFi configurable, `GPIO5` par défaut : active ou coupe le point d'accès Web de configuration, éteint par défaut au démarrage.

La LED WS2812/RGB intégrée s'allume en rouge pendant l'écriture des caractères, en bleu quand le WiFi de configuration est actif, en violet pendant une mise à jour OTA, en vert pour une action réussie et en rouge pour une erreur. Elle clignote en rouge quand `Caps Lock` est actif sur l'ordinateur hôte.

> Branchez le câble USB-C sur le port relié à l'USB natif/OTG de l'ESP32-S3, pas sur le port USB-série/UART, pour que le clavier et la souris HID soient visibles par l'ordinateur hôte.

## Prérequis

- le `.venv` local fourni dans ce dossier, contenant PlatformIO Core ;
- ou, si vous repartez d'une copie sans `.venv`, Python 3 pour le recréer ;
- une carte ESP32-S3 compatible avec `esp32-s3-devkitc-1` ou une carte à sélectionner dans `platformio.ini`.

## Environnement local PlatformIO

Le projet utilise un environnement virtuel Python local dans `.venv/`. Il isole PlatformIO du reste de la machine et permet d'utiliser les mêmes commandes depuis PowerShell, Zed ou un terminal intégré.

Pour activer l'environnement dans PowerShell :

```powershell
.\.venv\Scripts\Activate.ps1
```

Pour vérifier PlatformIO :

```powershell
pio --version
```

Sans activation préalable, vous pouvez toujours appeler PlatformIO directement :

```powershell
.\.venv\Scripts\platformio.exe --version
```

Si `.venv/` n'existe pas encore, créez-le puis installez PlatformIO :

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install platformio
```

## Compilation

Depuis un terminal où le `.venv` est activé :

```powershell
pio run
```

Ou sans activation :

```powershell
.\.venv\Scripts\platformio.exe run
```

## Téléversement

Depuis un terminal où le `.venv` est activé :

```powershell
pio run --target upload
```

Ou sans activation :

```powershell
.\.venv\Scripts\platformio.exe run --target upload
```

Si le téléversement échoue, maintenez le bouton **BOOT** pendant le reset de la carte afin de passer l'ESP32-S3 en mode téléchargement, puis relancez la commande.

## Configuration de la position centrale

Les souris HID classiques envoient des mouvements relatifs. Le firmware force donc d'abord le pointeur vers le coin supérieur gauche, puis avance de la moitié de la résolution configurée.

Modifiez ces constantes dans `include/hid_config.h` pour correspondre à l'écran hôte :

```cpp
constexpr int SCREEN_WIDTH_PX = 1920;
constexpr int SCREEN_HEIGHT_PX = 1080;
```

## Boutons GPIO

Les boutons utilisent les résistances pull-up internes de l'ESP32-S3. Câblez donc chaque bouton entre le GPIO indiqué et `GND` :

```text
Bouton écriture :
  GPIO14 ---- bouton ---- GND

Bouton action Win+R :
  GPIO4  ---- bouton ---- GND

Bouton WiFi :
  GPIO5  ---- bouton ---- GND
```

Ne branchez pas ces boutons sur `3V3` : au repos l'entrée est maintenue à l'état haut par `INPUT_PULLUP`, et l'appui met simplement le GPIO à `LOW` en le reliant à `GND`.

Diagnostic du bouton `GPIO14` : comme le firmware lit `GPIO14` et `GPIO4` avec la même fonction, le test le plus fiable consiste à déplacer le bouton qui fonctionne sur `GPIO4` vers `GPIO14`, sans changer le fil `GND`. Le moniteur série doit afficher `GPIO14 appuye: ecriture de la chaine.` à l'appui.

Les séquences texte sont envoyées avec une table de conversion pour un hôte Windows configuré en clavier AZERTY français. La commande du bouton action est configurable depuis l'interface Web et par la clé `app.actionText`.

## Interface Web WiFi

Au démarrage, le firmware force le WiFi éteint : aucun point d'accès n'est créé par défaut. Appuyez sur le bouton WiFi configurable (`GPIO5` par défaut) pour créer le point d'accès local :

```text
SSID: ESP32-HID-Config
Mot de passe: esp32hid
Adresse: http://192.168.4.1/
```

La page Web permet de visualiser et modifier :

- le nom du point d'accès WiFi, `ESP32-HID-Config` par défaut ;
- le mot de passe du point d'accès WiFi, `esp32hid` par défaut ;
- `TYPE_AZERTY_BUTTON_PIN`, `GPIO14` par défaut ;
- le GPIO du bouton action `Win+R`, `GPIO4` par défaut ;
- la commande du bouton action, `wt` par défaut ;
- le GPIO du bouton WiFi on/off, `GPIO5` par défaut ;
- l'activation et le texte de la démonstration de démarrage, `salut` par défaut ;
- une liste de paires de chaînes chargée depuis un fichier texte ;
- la paire sélectionnée ;
- l'ajout manuel et la suppression d'une paire ;
- le téléchargement d'un fichier contenant tous les paramètres et toutes les paires mémorisées ;
- le dernier rapport d'import et un journal des événements récents.

Format du fichier de paramétrage :

```text
app.apSsid=ESP32-HID-Config
app.apPass=esp32hid
app.typePin=14
app.actionPin=4
app.wifiPin=5
# Le bouton action lance Win+R, tape cette commande, puis valide avec Entree.
app.actionText=notepad
app.demoOn=0
app.demoText=salut
app.selectedPair=0

nom1;mot-a-ecrire-1
nom2;mot-a-ecrire-2
nom3=mot-a-ecrire-3
nom4,mot-a-ecrire-4
```

Les clés `app.*` modifient les paramètres de l'application. Les autres lignes sont des paires HID. Les séparateurs acceptés sont tabulation, `;`, `,` et `=`. Les lignes commençant par `#` sont ignorées.
Si un champ doit contenir un séparateur, échappez-le avec `\`, par exemple `nom\=special=texte`.

Clés de paramétrage supportées :

- `app.apSsid` : nom du point d'accès WiFi ;
- `app.apPass` : mot de passe du point d'accès WiFi ;
- `app.typePin` : GPIO du bouton d'écriture ;
- `app.actionPin` : GPIO du bouton action `Win+R` ;
- `app.terminalPin` : ancien nom accepté en compatibilité pour le même GPIO ;
- `app.wifiPin` : GPIO du bouton WiFi on/off ;
- `app.typeText` : ancien texte manuel du bouton d'écriture, encore accepté à l'import pour compatibilité ;
- `app.actionText` : commande saisie par le bouton action après `Win+R` ;
- `app.demoOn` : `1`/`0` pour activer ou désactiver la démonstration de démarrage ;
- `app.demoText` : texte de la démonstration de démarrage ;
- `app.selectedPair` : index de la paire sélectionnée.

Lors d'un upload de fichier :

- par défaut, la liste existante est remplacée par le contenu du fichier ;
- si `Fusionner avec les paires existantes` est coché, une paire dont le premier champ existe déjà met à jour son second champ, et une nouvelle paire est ajoutée ;
- si la liste est pleine, les ajouts restants sont ignorés.
- les clés `app.*` sont appliquées dans les deux modes, puis mémorisées en NVS.
- les lignes ignorées, les GPIO invalides, les clés inconnues et les paires trop longues sont affichés dans le rapport du dernier import.

La page permet aussi de supprimer la paire actuellement sélectionnée. Après suppression, la sélection reste sur une paire valide si la liste n'est pas vide.

La WS2812 signale les états principaux :

- bleu fixe : point d'accès WiFi actif ;
- rouge pendant l'écriture HID ;
- vert pendant une seconde : import, ajout ou démarrage WiFi réussi ;
- rouge pendant une seconde : erreur, liste pleine ou arrêt WiFi ;
- violet : mise à jour OTA en cours ;
- rouge clignotant : `Caps Lock` actif.

Le second champ peut contenir des séquences échappées :

```text
tabulation=avant\tapres
retour=ligne1\rligne2
ctrl-c=\cC
ctrl-d=\cD
win-r=\wR
echappement=\e
caret=\^
```

Séquences supportées :

- `\t` : tabulation ;
- `\r` ou `\n` : touche Entrée ;
- `\b` : retour arrière ;
- `\e` : Échap ;
- `\\` : caractère `\` ;
- `\^` : caractère `^` ;
- `\cA` à `\cZ` : raccourcis Ctrl-A à Ctrl-Z, par exemple `\cC` pour Ctrl-C et `\cD` pour Ctrl-D ;
- `\wA` à `\wZ` : raccourcis Win-A à Win-Z, par exemple `\wR` pour Win-R.

Les changements sont mémorisés en flash avec `Preferences` et sont restaurés au prochain démarrage. Le nom du point d'accès WiFi est limité à 32 caractères ; son mot de passe doit contenir 8 à 63 caractères. Ces deux paramètres prennent effet à la prochaine activation du WiFi. Quand une paire est sélectionnée, le bouton physique écrit le second mot de cette paire. S'il n'y a aucune paire chargée, l'ancien texte manuel importé via `app.typeText` reste utilisé par compatibilité. Dans la sélection de paire, appuyer sur `Entrée` sauvegarde les paramètres puis écrit immédiatement le texte via le clavier HID. Le journal Web affiche les derniers événements depuis le démarrage.

La page Web permet aussi de mettre à jour le firmware par OTA en déposant le binaire PlatformIO :

```text
C:\Users\loicp\OneDrive\Documents\clavier_esp2\.pio\build\esp32s3-usb-hid\firmware.bin
```

La carte redémarre automatiquement après une mise à jour valide. Ne coupez pas l'alimentation pendant l'upload du firmware.

## LED WS2812

La WS2812 intégrée est pilotée avec `neopixelWrite()` sur `GPIO48`, broche courante des cartes ESP32-S3 AI-S3 violettes. Si votre carte utilise une autre broche, modifiez cette constante dans `include/hid_config.h` :

```cpp
constexpr int STATUS_NEOPIXEL_PIN = 48;
```

Le clignotement `Caps Lock` utilise l'état LED renvoyé par l'hôte USB HID. Son intervalle est configurable :

```cpp
constexpr unsigned long CAPS_LOCK_BLINK_INTERVAL_MS = 400;
```

## Sécurité d'utilisation

La démonstration automatique de démarrage peut déplacer la souris, cliquer et taper du texte si elle est activée. Branchez l'ESP32 uniquement sur une machine de test ou dans un contexte où cette automatisation est attendue.
