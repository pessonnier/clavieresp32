# ESP32-S3 clavier + souris USB HID

Ce dépôt contient un environnement local PlatformIO pour développer un périphérique USB HID composite avec un **ESP32-S3 WROOM-1** équipé de deux ports USB-C.

Le firmware démarre le périphérique USB natif de l'ESP32-S3, expose une souris et un clavier HID, puis exécute une démonstration une seule fois :

1. déplacement approximatif du pointeur au centre de l'écran ;
2. clic gauche ;
3. saisie du texte `abcdef`.

> Branchez le câble USB-C sur le port relié à l'USB natif/OTG de l'ESP32-S3, pas sur le port USB-série/UART, pour que le clavier et la souris HID soient visibles par l'ordinateur hôte.

## Prérequis

- Python 3 ;
- [PlatformIO Core](https://platformio.org/install/cli) ou l'extension PlatformIO pour VS Code ;
- une carte ESP32-S3 compatible avec `esp32-s3-devkitc-1` ou une carte à sélectionner dans `platformio.ini`.

## Compilation

```bash
pio run
```

## Téléversement

```bash
pio run --target upload
```

Si le téléversement échoue, maintenez le bouton **BOOT** pendant le reset de la carte afin de passer l'ESP32-S3 en mode téléchargement, puis relancez la commande.

## Configuration de la position centrale

Les souris HID classiques envoient des mouvements relatifs. Le firmware force donc d'abord le pointeur vers le coin supérieur gauche, puis avance de la moitié de la résolution configurée.

Modifiez ces constantes dans `include/hid_config.h` pour correspondre à l'écran hôte :

```cpp
constexpr int SCREEN_WIDTH_PX = 1920;
constexpr int SCREEN_HEIGHT_PX = 1080;
```

## Sécurité d'utilisation

Le firmware clique et tape automatiquement après l'énumération USB. Branchez-le uniquement sur une machine de test ou dans un contexte où cette automatisation est attendue.
