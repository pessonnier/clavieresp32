#pragma once

// Résolution cible utilisée pour atteindre le centre avec une souris HID relative.
// Ajustez ces valeurs à l'écran de la machine hôte si nécessaire.
constexpr int SCREEN_WIDTH_PX = 1920;
constexpr int SCREEN_HEIGHT_PX = 1080;

// Délai après l'apparition USB afin de laisser le système hôte énumérer le périphérique.
constexpr unsigned long USB_ENUMERATION_DELAY_MS = 3000;

// Démo exécutée une seule fois au démarrage : déplacement souris, clic, puis saisie.
// Elle peut être désactivée et son texte peut être modifié depuis l'interface Web.
constexpr bool DEFAULT_STARTUP_DEMO_ENABLED = true;
constexpr const char* DEFAULT_STARTUP_DEMO_TEXT = "salut";

// Boutons câblés entre le GPIO et GND. Les entrées utilisent la résistance pull-up interne,
// donc aucun fil ne doit aller vers 3V3 pour ces boutons.
//
// Bouton d'écriture :
//   patte 1 du bouton -> GPIO14
//   patte 2 du bouton -> GND
constexpr int TYPE_AZERTY_BUTTON_PIN = 14;
constexpr const char* DEFAULT_TYPE_AZERTY_TEXT = "abc";
constexpr size_t MAX_WORD_PAIRS = 32;
constexpr size_t MAX_PAIR_FIELD_LENGTH = 96;
constexpr size_t MAX_PAIR_UPLOAD_BYTES = 4096;

// Bouton d'ouverture de terminal :
//   patte 1 du bouton -> GPIO4
//   patte 2 du bouton -> GND
constexpr int OPEN_TERMINAL_BUTTON_PIN = 4;

// Bouton d'activation/désactivation du WiFi de configuration :
//   patte 1 du bouton -> GPIO5
//   patte 2 du bouton -> GND
constexpr int WIFI_TOGGLE_BUTTON_PIN = 5;

constexpr unsigned long BUTTON_DEBOUNCE_DELAY_MS = 40;

// LED WS2812/RGB intégrée. Sur les cartes ESP32-S3 AI-S3 violettes, elle est
// généralement raccordée au GPIO48.
constexpr int STATUS_NEOPIXEL_PIN = 48;
constexpr uint8_t STATUS_NEOPIXEL_RED_BRIGHTNESS = 32;
constexpr uint8_t STATUS_NEOPIXEL_GREEN_BRIGHTNESS = 32;
constexpr unsigned long CAPS_LOCK_BLINK_INTERVAL_MS = 400;
constexpr unsigned long IMPORT_STATUS_SIGNAL_MS = 1000;

// Interface Web de configuration. L'ESP32 crée ce point d'accès WiFi au démarrage.
constexpr const char* CONFIG_AP_SSID = "ESP32-HID-Config";
constexpr const char* CONFIG_AP_PASSWORD = "esp32hid";
