#pragma once

// Résolution cible utilisée pour atteindre le centre avec une souris HID relative.
// Ajustez ces valeurs à l'écran de la machine hôte si nécessaire.
constexpr int SCREEN_WIDTH_PX = 1920;
constexpr int SCREEN_HEIGHT_PX = 1080;

// Délai après l'apparition USB afin de laisser le système hôte énumérer le périphérique.
constexpr unsigned long USB_ENUMERATION_DELAY_MS = 3000;
