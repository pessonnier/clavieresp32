#include <Arduino.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include "esp32-hal-rgb-led.h"
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"

#include "hid_config.h"

USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;
Preferences preferences;
WebServer webServer(80);

namespace {
constexpr int HID_MOUSE_MIN_DELTA = -127;
constexpr int HID_MOUSE_MAX_DELTA = 127;
constexpr int MOVE_STEP_DELAY_MS = 5;
constexpr int ACTION_DELAY_MS = 250;
constexpr int KEY_PRESS_DELAY_MS = 35;
constexpr int RUN_DIALOG_DELAY_MS = 500;

// Codes HID bruts correspondant aux touches physiques. Avec une disposition
// Windows AZERTY FR, QWERTY physique produit la chaîne "azerty".
constexpr uint8_t RAW_KEY_A = 0x04;
constexpr uint8_t RAW_KEY_B = 0x05;
constexpr uint8_t RAW_KEY_C = 0x06;
constexpr uint8_t RAW_KEY_D = 0x07;
constexpr uint8_t RAW_KEY_E = 0x08;
constexpr uint8_t RAW_KEY_F = 0x09;
constexpr uint8_t RAW_KEY_G = 0x0A;
constexpr uint8_t RAW_KEY_H = 0x0B;
constexpr uint8_t RAW_KEY_I = 0x0C;
constexpr uint8_t RAW_KEY_J = 0x0D;
constexpr uint8_t RAW_KEY_K = 0x0E;
constexpr uint8_t RAW_KEY_L = 0x0F;
constexpr uint8_t RAW_KEY_M = 0x10;
constexpr uint8_t RAW_KEY_N = 0x11;
constexpr uint8_t RAW_KEY_O = 0x12;
constexpr uint8_t RAW_KEY_P = 0x13;
constexpr uint8_t RAW_KEY_Q = 0x14;
constexpr uint8_t RAW_KEY_R = 0x15;
constexpr uint8_t RAW_KEY_S = 0x16;
constexpr uint8_t RAW_KEY_T = 0x17;
constexpr uint8_t RAW_KEY_U = 0x18;
constexpr uint8_t RAW_KEY_V = 0x19;
constexpr uint8_t RAW_KEY_W = 0x1A;
constexpr uint8_t RAW_KEY_X = 0x1B;
constexpr uint8_t RAW_KEY_Y = 0x1C;
constexpr uint8_t RAW_KEY_Z = 0x1D;
constexpr uint8_t RAW_KEY_1 = 0x1E;
constexpr uint8_t RAW_KEY_2 = 0x1F;
constexpr uint8_t RAW_KEY_3 = 0x20;
constexpr uint8_t RAW_KEY_4 = 0x21;
constexpr uint8_t RAW_KEY_5 = 0x22;
constexpr uint8_t RAW_KEY_6 = 0x23;
constexpr uint8_t RAW_KEY_7 = 0x24;
constexpr uint8_t RAW_KEY_8 = 0x25;
constexpr uint8_t RAW_KEY_9 = 0x26;
constexpr uint8_t RAW_KEY_0 = 0x27;
constexpr uint8_t RAW_KEY_ENTER = 0x28;
constexpr uint8_t RAW_KEY_ESC = 0x29;
constexpr uint8_t RAW_KEY_BACKSPACE = 0x2A;
constexpr uint8_t RAW_KEY_TAB = 0x2B;
constexpr uint8_t RAW_KEY_SPACE = 0x2C;
constexpr uint8_t RAW_KEY_MINUS = 0x2D;
constexpr uint8_t RAW_KEY_EQUAL = 0x2E;
constexpr uint8_t RAW_KEY_LEFT_BRACKET = 0x2F;
constexpr uint8_t RAW_KEY_RIGHT_BRACKET = 0x30;
constexpr uint8_t RAW_KEY_BACKSLASH = 0x31;
constexpr uint8_t RAW_KEY_SEMICOLON = 0x33;
constexpr uint8_t RAW_KEY_APOSTROPHE = 0x34;
constexpr uint8_t RAW_KEY_GRAVE = 0x35;
constexpr uint8_t RAW_KEY_COMMA = 0x36;
constexpr uint8_t RAW_KEY_DOT = 0x37;
constexpr uint8_t RAW_KEY_SLASH = 0x38;
constexpr uint8_t RAW_KEY_NON_US_BACKSLASH = 0x64;

bool demoAlreadyRun = false;
volatile bool capsLockEnabled = false;
bool statusLedForced = false;
bool statusLedBlinkOn = false;
unsigned long lastStatusLedBlinkMs = 0;
bool typeButtonWasPressed = false;
bool terminalButtonWasPressed = false;
bool wifiToggleButtonWasPressed = false;
int typeAzertyButtonPin = TYPE_AZERTY_BUTTON_PIN;
int terminalButtonPin = OPEN_TERMINAL_BUTTON_PIN;
int wifiToggleButtonPin = WIFI_TOGGLE_BUTTON_PIN;
String typeAzertyButtonText = DEFAULT_TYPE_AZERTY_TEXT;
bool startupDemoEnabled = DEFAULT_STARTUP_DEMO_ENABLED;
String startupDemoText = DEFAULT_STARTUP_DEMO_TEXT;
String configApSsid = CONFIG_AP_SSID;
String configApPassword = CONFIG_AP_PASSWORD;
String pairFirstWords[MAX_WORD_PAIRS];
String pairSecondWords[MAX_WORD_PAIRS];
size_t pairCount = 0;
size_t selectedPairIndex = 0;
String uploadedPairsText;
bool firmwareUpdateOk = false;
String firmwareUpdateError;
bool webRoutesConfigured = false;
bool wifiConfigEnabled = false;

struct AzertyKey {
  uint8_t rawKey;
  uint8_t modifier;
  uint8_t followRawKey;
  uint8_t followModifier;
};

void typeAzertyString(const char* text);
AzertyKey rawKeyForAzertyAsciiChar(char character);
String normalizedApSsid(const String& value);
String normalizedApPassword(const String& value);

// Écrit une couleur brute sur la WS2812 intégrée.
void writeStatusLed(uint8_t red, uint8_t green, uint8_t blue) {
  neopixelWrite(STATUS_NEOPIXEL_PIN, red, green, blue);
}

// Force l'indicateur d'activité HID en rouge.
void writeStatusLedRed() {
  writeStatusLed(STATUS_NEOPIXEL_RED_BRIGHTNESS, 0, 0);
}

// Force l'indicateur d'import réussi en vert.
void writeStatusLedGreen() {
  writeStatusLed(0, STATUS_NEOPIXEL_GREEN_BRIGHTNESS, 0);
}

// Éteint la WS2812.
void writeStatusLedOff() {
  writeStatusLed(0, 0, 0);
}

// Affiche un signal LED temporaire sans modifier les paramètres mémorisés.
void signalStatusLed(uint8_t red, uint8_t green, uint8_t blue) {
  statusLedForced = true;
  writeStatusLed(red, green, blue);
  delay(IMPORT_STATUS_SIGNAL_MS);
  statusLedForced = false;
  writeStatusLedOff();
}

// Signale un import ou ajout réussi.
void signalImportSuccess() {
  signalStatusLed(0, STATUS_NEOPIXEL_GREEN_BRIGHTNESS, 0);
}

// Signale que la liste de paires est pleine.
void signalPairListFull() {
  signalStatusLed(STATUS_NEOPIXEL_RED_BRIGHTNESS, 0, 0);
}

// Réserve la WS2812 pour une action de saisie en cours.
void setStatusLedRed() {
  statusLedForced = true;
  writeStatusLedRed();
}

// Libère la WS2812 après une action de saisie.
void setStatusLedOff() {
  statusLedForced = false;
  statusLedBlinkOn = false;
  lastStatusLedBlinkMs = millis();
  writeStatusLedOff();
}

// Fait clignoter la WS2812 en rouge lorsque l'hôte signale Caps Lock actif.
void handleStatusLed() {
  if (statusLedForced) {
    return;
  }

  if (!capsLockEnabled) {
    if (statusLedBlinkOn) {
      statusLedBlinkOn = false;
      writeStatusLedOff();
    }
    return;
  }

  const unsigned long now = millis();
  if (lastStatusLedBlinkMs == 0 || now - lastStatusLedBlinkMs >= CAPS_LOCK_BLINK_INTERVAL_MS) {
    statusLedBlinkOn = !statusLedBlinkOn;
    lastStatusLedBlinkMs = now;
    if (statusLedBlinkOn) {
      writeStatusLedRed();
    } else {
      writeStatusLedOff();
    }
  }
}

// Reçoit les états LED clavier envoyés par l'hôte USB, dont Caps Lock.
void onKeyboardLedEvent(void* arg, esp_event_base_t eventBase, int32_t eventId, void* eventData) {
  (void)arg;
  (void)eventBase;

  if (eventId != ARDUINO_USB_HID_KEYBOARD_LED_EVENT || eventData == nullptr) {
    return;
  }

  const auto* keyboardLeds = static_cast<arduino_usb_hid_keyboard_event_data_t*>(eventData);
  capsLockEnabled = keyboardLeds->capslock;
}

// Détecte un front d'appui sur un bouton câblé entre GPIO et GND.
bool readButtonPress(int pin, bool& wasPressed) {
  bool isPressed = digitalRead(pin) == LOW;
  if (isPressed && !wasPressed) {
    delay(BUTTON_DEBOUNCE_DELAY_MS);
    isPressed = digitalRead(pin) == LOW;
    wasPressed = isPressed;
    return isPressed;
  }

  wasPressed = isPressed;
  return false;
}

// Échappe les caractères spéciaux avant insertion dans l'attribut HTML value.
String htmlEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length());
  for (size_t i = 0; i < value.length(); ++i) {
    switch (value[i]) {
      case '&':
        escaped += F("&amp;");
        break;
      case '<':
        escaped += F("&lt;");
        break;
      case '>':
        escaped += F("&gt;");
        break;
      case '"':
        escaped += F("&quot;");
        break;
      case '\'':
        escaped += F("&#39;");
        break;
      default:
        escaped += value[i];
        break;
    }
  }
  return escaped;
}

// Échappe un champ avant export dans le fichier de paramétrage.
String configFileEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length());
  for (size_t i = 0; i < value.length(); ++i) {
    switch (value[i]) {
      case '\\':
        escaped += F("\\\\");
        break;
      case '\t':
        escaped += F("\\t");
        break;
      case '\r':
        escaped += F("\\r");
        break;
      case '\n':
        escaped += F("\\n");
        break;
      case '=':
      case ';':
      case ',':
        escaped += '\\';
        escaped += value[i];
        break;
      default:
        escaped += value[i];
        break;
    }
  }
  return escaped;
}

// Décode uniquement les séparateurs échappés du format de paires.
String unescapePairFieldSyntax(const String& value) {
  String unescaped;
  unescaped.reserve(value.length());
  for (size_t i = 0; i < value.length(); ++i) {
    if (value[i] == '\\' && i + 1 < value.length()) {
      const char next = value[i + 1];
      if (next == '=' || next == ';' || next == ',') {
        unescaped += next;
        ++i;
        continue;
      }
    }
    unescaped += value[i];
  }
  return unescaped;
}

// Filtre les GPIO utilisables pour le bouton configurable.
bool isValidButtonPin(int pin) {
  return pin >= 0 && pin <= 48 && pin != STATUS_NEOPIXEL_PIN;
}

// Applique le GPIO de bouton courant et initialise sa résistance pull-up interne.
void configureTypeAzertyButtonPin(int pin) {
  typeAzertyButtonPin = pin;
  pinMode(typeAzertyButtonPin, INPUT_PULLUP);
  typeButtonWasPressed = digitalRead(typeAzertyButtonPin) == LOW;
}

// Applique le GPIO du bouton terminal et initialise sa résistance pull-up interne.
void configureTerminalButtonPin(int pin) {
  terminalButtonPin = pin;
  pinMode(terminalButtonPin, INPUT_PULLUP);
  terminalButtonWasPressed = digitalRead(terminalButtonPin) == LOW;
}

// Applique le GPIO du bouton WiFi et initialise sa résistance pull-up interne.
void configureWifiToggleButtonPin(int pin) {
  wifiToggleButtonPin = pin;
  pinMode(wifiToggleButtonPin, INPUT_PULLUP);
  wifiToggleButtonWasPressed = digitalRead(wifiToggleButtonPin) == LOW;
}

// Limite un champ de paire pour éviter de saturer la RAM et la NVS.
String trimPairField(String value) {
  value.trim();
  if (value.length() > MAX_PAIR_FIELD_LENGTH) {
    value = value.substring(0, MAX_PAIR_FIELD_LENGTH);
  }
  return value;
}

// Remplace la liste de paires par une liste vide.
void clearWordPairs() {
  for (size_t i = 0; i < MAX_WORD_PAIRS; ++i) {
    pairFirstWords[i] = "";
    pairSecondWords[i] = "";
  }
  pairCount = 0;
  selectedPairIndex = 0;
}

// Ajoute une paire en mémoire si elle est non vide et dans la limite configurée.
bool addWordPair(const String& first, const String& second) {
  const String normalizedFirst = trimPairField(first);
  const String normalizedSecond = trimPairField(second);
  if (normalizedFirst.isEmpty() || normalizedSecond.isEmpty()) {
    return false;
  }

  if (pairCount >= MAX_WORD_PAIRS) {
    return false;
  }

  pairFirstWords[pairCount] = normalizedFirst;
  pairSecondWords[pairCount] = normalizedSecond;
  ++pairCount;
  return true;
}

// Fusionne une paire : met à jour la valeur si la clé existe, sinon ajoute.
bool mergeWordPair(const String& first, const String& second, bool& fullReached) {
  const String normalizedFirst = trimPairField(first);
  const String normalizedSecond = trimPairField(second);
  if (normalizedFirst.isEmpty() || normalizedSecond.isEmpty()) {
    return false;
  }

  for (size_t i = 0; i < pairCount; ++i) {
    if (pairFirstWords[i] == normalizedFirst) {
      pairSecondWords[i] = normalizedSecond;
      return true;
    }
  }

  if (pairCount >= MAX_WORD_PAIRS) {
    fullReached = true;
    return false;
  }

  pairFirstWords[pairCount] = normalizedFirst;
  pairSecondWords[pairCount] = normalizedSecond;
  ++pairCount;
  return true;
}

// Convertit les valeurs texte courantes d'un fichier de paramétrage en booléen.
bool configBoolValue(const String& value, bool fallback) {
  String normalized = value;
  normalized.trim();
  normalized.toLowerCase();

  if (normalized == "1" || normalized == "true" || normalized == "on" || normalized == "yes" || normalized == "oui") {
    return true;
  }
  if (normalized == "0" || normalized == "false" || normalized == "off" || normalized == "no" || normalized == "non") {
    return false;
  }
  return fallback;
}

// Trouve le séparateur d'une ligne de fichier de paires.
int findPairSeparator(const String& line) {
  const char separators[] = {'\t', ';', ',', '='};
  bool escaped = false;
  for (size_t i = 0; i < line.length(); ++i) {
    if (escaped) {
      escaped = false;
      continue;
    }

    if (line[i] == '\\') {
      escaped = true;
      continue;
    }

    for (char separator : separators) {
      if (line[i] == separator && i > 0) {
        return i;
      }
    }
  }
  return -1;
}

// Applique une entrée app.* du fichier de paramétrage aux variables runtime.
bool applyAppConfigEntry(const String& key, const String& value) {
  if (key == "app.typePin") {
    const int requestedPin = value.toInt();
    if (isValidButtonPin(requestedPin)) {
      configureTypeAzertyButtonPin(requestedPin);
      return true;
    }
    return false;
  }

  if (key == "app.terminalPin") {
    const int requestedPin = value.toInt();
    if (isValidButtonPin(requestedPin)) {
      configureTerminalButtonPin(requestedPin);
      return true;
    }
    return false;
  }

  if (key == "app.wifiPin") {
    const int requestedPin = value.toInt();
    if (isValidButtonPin(requestedPin)) {
      configureWifiToggleButtonPin(requestedPin);
      return true;
    }
    return false;
  }

  if (key == "app.typeText") {
    typeAzertyButtonText = value;
    return true;
  }

  if (key == "app.demoOn") {
    startupDemoEnabled = configBoolValue(value, startupDemoEnabled);
    return true;
  }

  if (key == "app.demoText") {
    startupDemoText = value;
    return true;
  }

  if (key == "app.apSsid") {
    configApSsid = normalizedApSsid(value);
    return true;
  }

  if (key == "app.apPass") {
    configApPassword = normalizedApPassword(value);
    return true;
  }

  if (key == "app.selectedPair") {
    const int requestedPair = value.toInt();
    if (requestedPair >= 0) {
      selectedPairIndex = requestedPair;
      return true;
    }
    return false;
  }

  return false;
}

// Parse un fichier texte. Les clés app.* configurent le logiciel ; les autres lignes restent des paires HID.
size_t parseWordPairs(const String& content, bool mergeMode, bool& fullReached) {
  if (!mergeMode) {
    clearWordPairs();
  }

  size_t importedPairs = 0;
  size_t importedSettings = 0;

  size_t start = 0;
  while (start < content.length()) {
    int end = content.indexOf('\n', start);
    if (end < 0) {
      end = content.length();
    }

    String line = content.substring(start, end);
    line.replace("\r", "");
    line.trim();

    if (!line.isEmpty() && !line.startsWith("#")) {
      const int separator = findPairSeparator(line);
      if (separator > 0) {
        String key = unescapePairFieldSyntax(line.substring(0, separator));
        String value = unescapePairFieldSyntax(line.substring(separator + 1));
        key.trim();
        if (key.startsWith("app.")) {
          value.trim();
          if (applyAppConfigEntry(key, value)) {
            ++importedSettings;
          }
        } else if (mergeWordPair(key, value, fullReached)) {
          ++importedPairs;
        }
      }
    }

    start = end + 1;
  }

  if (selectedPairIndex >= pairCount) {
    selectedPairIndex = 0;
  }

  Serial.print("Parametres app importes: ");
  Serial.println(importedSettings);
  return importedPairs;
}

// Renvoie le texte HID actif : second mot de la paire sélectionnée ou texte manuel historique.
String selectedTypeText() {
  if (pairCount > 0 && selectedPairIndex < pairCount) {
    return pairSecondWords[selectedPairIndex];
  }
  return typeAzertyButtonText;
}

// Valide le SSID du point d'accès : non vide et limité à 32 octets.
String normalizedApSsid(const String& value) {
  String normalized = value;
  normalized.trim();
  if (normalized.length() == 0) {
    return CONFIG_AP_SSID;
  }
  if (normalized.length() > 32) {
    normalized = normalized.substring(0, 32);
  }
  return normalized;
}

// Valide le mot de passe WPA2 du point d'accès : 8 à 63 caractères.
String normalizedApPassword(const String& value) {
  String normalized = value;
  normalized.trim();
  if (normalized.length() < 8) {
    return CONFIG_AP_PASSWORD;
  }
  if (normalized.length() > 63) {
    normalized = normalized.substring(0, 63);
  }
  return normalized;
}

// Charge les paramètres persistants depuis la NVS.
void loadSettings() {
  preferences.begin("hidcfg", false);
  const int storedPin = preferences.getInt("typePin", TYPE_AZERTY_BUTTON_PIN);
  configureTypeAzertyButtonPin(isValidButtonPin(storedPin) ? storedPin : TYPE_AZERTY_BUTTON_PIN);
  const int storedTerminalPin = preferences.getInt("termPin", OPEN_TERMINAL_BUTTON_PIN);
  configureTerminalButtonPin(isValidButtonPin(storedTerminalPin) ? storedTerminalPin : OPEN_TERMINAL_BUTTON_PIN);
  const int storedWifiPin = preferences.getInt("wifiPin", WIFI_TOGGLE_BUTTON_PIN);
  configureWifiToggleButtonPin(isValidButtonPin(storedWifiPin) ? storedWifiPin : WIFI_TOGGLE_BUTTON_PIN);
  typeAzertyButtonText = preferences.getString("typeText", DEFAULT_TYPE_AZERTY_TEXT);
  startupDemoEnabled = preferences.getBool("demoOn", DEFAULT_STARTUP_DEMO_ENABLED);
  startupDemoText = preferences.getString("demoText", DEFAULT_STARTUP_DEMO_TEXT);
  configApSsid = normalizedApSsid(preferences.getString("apSsid", CONFIG_AP_SSID));
  configApPassword = normalizedApPassword(preferences.getString("apPass", CONFIG_AP_PASSWORD));
  clearWordPairs();

  pairCount = preferences.getUInt("pairCount", 0);
  if (pairCount > MAX_WORD_PAIRS) {
    pairCount = MAX_WORD_PAIRS;
  }

  for (size_t i = 0; i < pairCount; ++i) {
    pairFirstWords[i] = preferences.getString(("p" + String(i) + "a").c_str(), "");
    pairSecondWords[i] = preferences.getString(("p" + String(i) + "b").c_str(), "");
  }

  selectedPairIndex = preferences.getUInt("selected", 0);
  if (selectedPairIndex >= pairCount) {
    selectedPairIndex = 0;
  }
}

// Sauvegarde le GPIO et le texte en NVS pour le prochain démarrage.
void saveSettings() {
  preferences.putInt("typePin", typeAzertyButtonPin);
  preferences.putInt("termPin", terminalButtonPin);
  preferences.putInt("wifiPin", wifiToggleButtonPin);
  preferences.putString("typeText", typeAzertyButtonText);
  preferences.putBool("demoOn", startupDemoEnabled);
  preferences.putString("demoText", startupDemoText);
  preferences.putString("apSsid", configApSsid);
  preferences.putString("apPass", configApPassword);
  preferences.putUInt("pairCount", pairCount);
  preferences.putUInt("selected", selectedPairIndex);
  for (size_t i = 0; i < MAX_WORD_PAIRS; ++i) {
    const String firstKey = "p" + String(i) + "a";
    const String secondKey = "p" + String(i) + "b";
    if (i < pairCount) {
      preferences.putString(firstKey.c_str(), pairFirstWords[i]);
      preferences.putString(secondKey.c_str(), pairSecondWords[i]);
    } else {
      preferences.remove(firstKey.c_str());
      preferences.remove(secondKey.c_str());
    }
  }
}

// Applique les valeurs reçues par formulaire Web, puis les sauvegarde.
void applyWebSettings() {
  if (webServer.hasArg("pin")) {
    const int requestedPin = webServer.arg("pin").toInt();
    if (isValidButtonPin(requestedPin)) {
      configureTypeAzertyButtonPin(requestedPin);
    }
  }

  if (webServer.hasArg("terminalPin")) {
    const int requestedPin = webServer.arg("terminalPin").toInt();
    if (isValidButtonPin(requestedPin)) {
      configureTerminalButtonPin(requestedPin);
    }
  }

  if (webServer.hasArg("wifiPin")) {
    const int requestedPin = webServer.arg("wifiPin").toInt();
    if (isValidButtonPin(requestedPin)) {
      configureWifiToggleButtonPin(requestedPin);
    }
  }

  if (webServer.hasArg("text")) {
    typeAzertyButtonText = webServer.arg("text");
  }

  if (webServer.hasArg("apSsid")) {
    configApSsid = normalizedApSsid(webServer.arg("apSsid"));
  }

  if (webServer.hasArg("apPass")) {
    configApPassword = normalizedApPassword(webServer.arg("apPass"));
  }

  startupDemoEnabled = webServer.hasArg("demoOn");
  if (webServer.hasArg("demoText")) {
    startupDemoText = webServer.arg("demoText");
  }

  if (webServer.hasArg("pair")) {
    const int requestedPair = webServer.arg("pair").toInt();
    if (requestedPair >= 0 && static_cast<size_t>(requestedPair) < pairCount) {
      selectedPairIndex = requestedPair;
    }
  }

  saveSettings();
}

// Convertit l'énumération WebServer en texte lisible pour les traces série.
const char* httpMethodName(HTTPMethod method) {
  switch (method) {
    case HTTP_GET:
      return "GET";
    case HTTP_POST:
      return "POST";
    case HTTP_PUT:
      return "PUT";
    case HTTP_DELETE:
      return "DELETE";
    default:
      return "OTHER";
  }
}

// Trace chaque appel HTTP pour vérifier que la route attendue est bien appelée.
void logHttpRequest(const char* handlerName) {
  Serial.print("HTTP ");
  Serial.print(httpMethodName(webServer.method()));
  Serial.print(" ");
  Serial.print(webServer.uri());
  Serial.print(" -> ");
  Serial.print(handlerName);
  Serial.print(" args=");
  Serial.println(webServer.args());
  for (uint8_t i = 0; i < webServer.args(); ++i) {
    Serial.print("  ");
    Serial.print(webServer.argName(i));
    Serial.print("=");
    Serial.println(webServer.arg(i));
  }
}

// Génère la page de configuration sans JavaScript obligatoire.
void sendConfigPage() {
  logHttpRequest("sendConfigPage");
  String page;
  page.reserve(10200);
  page += F("<!doctype html><html lang='fr'><head><meta charset='utf-8'>");
  page += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<title>ESP32 HID</title><style>");
  page += F("body{font-family:system-ui,Segoe UI,sans-serif;margin:24px;background:#f6f7f9;color:#15171a}");
  page += F("main{max-width:680px;margin:auto;background:white;border:1px solid #d9dde3;border-radius:8px;padding:20px}");
  page += F("label{display:block;font-weight:600;margin-top:16px}input{box-sizing:border-box;width:100%;font-size:18px;padding:10px;margin-top:6px;border:1px solid #b8bec8;border-radius:6px}");
  page += F("button{font-size:16px;padding:10px 14px;margin-top:16px;border:0;border-radius:6px;background:#145bd7;color:white}");
  page += F("select{box-sizing:border-box;width:100%;font-size:18px;padding:10px;margin-top:6px;border:1px solid #b8bec8;border-radius:6px}");
  page += F(".drop{height:150px;margin-top:16px;border:2px dashed #8b96a8;border-radius:8px;background:#f9fafb;display:flex;align-items:center;justify-content:center;text-align:center;color:#4b5563;padding:12px}");
  page += F(".drop.active{border-color:#145bd7;background:#eef4ff;color:#145bd7}");
  page += F(".muted{color:#5f6875}.row{display:flex;gap:10px;flex-wrap:wrap}.row button{flex:1}");
  page += F("</style></head><body><main>");
  page += F("<h1>ESP32 HID</h1>");
  page += F("<p class='muted'>Point d'acces WiFi: ");
  page += htmlEscape(configApSsid);
  page += F(" - Adresse: http://192.168.4.1/</p>");
  page += F("<form id='cfg' method='post' action='/type' accept-charset='UTF-8'>");
  page += F("<label for='apSsid'>Nom du point d'acces WiFi</label>");
  page += F("<input id='apSsid' name='apSsid' type='text' maxlength='32' value=\"");
  page += htmlEscape(configApSsid);
  page += F("\">");
  page += F("<label for='apPass'>Mot de passe du point d'acces WiFi</label>");
  page += F("<input id='apPass' name='apPass' type='password' minlength='8' maxlength='63' value=\"");
  page += htmlEscape(configApPassword);
  page += F("\">");
  page += F("<p class='muted'>Le nouveau nom et le nouveau mot de passe sont memorises et seront utilises au prochain demarrage du WiFi. Mot de passe: 8 a 63 caracteres.</p>");
  page += F("<label for='pin'>TYPE_AZERTY_BUTTON_PIN</label>");
  page += F("<input id='pin' name='pin' type='number' min='0' max='48' value='");
  page += String(typeAzertyButtonPin);
  page += F("'>");
  page += F("<label for='terminalPin'>GPIO bouton terminal wt</label>");
  page += F("<input id='terminalPin' name='terminalPin' type='number' min='0' max='48' value='");
  page += String(terminalButtonPin);
  page += F("'>");
  page += F("<label for='wifiPin'>GPIO bouton WiFi on/off</label>");
  page += F("<input id='wifiPin' name='wifiPin' type='number' min='0' max='48' value='");
  page += String(wifiToggleButtonPin);
  page += F("'>");
  page += F("<label style='font-weight:400'><input id='demoOn' name='demoOn' type='checkbox' value='1' style='width:auto;margin-right:8px'");
  if (startupDemoEnabled) {
    page += F(" checked");
  }
  page += F(">Activer la demo de demarrage</label>");
  page += F("<label for='demoText'>Texte demo de demarrage</label>");
  page += F("<input id='demoText' name='demoText' type='text' value=\"");
  page += htmlEscape(startupDemoText);
  page += F("\">");
  page += F("<label for='text'>Texte associe a ce GPIO</label>");
  page += F("<input id='text' name='text' type='text' value=\"");
  page += htmlEscape(typeAzertyButtonText);
  page += F("\">");
  page += F("<label for='pair'>Paire selectionnee</label>");
  page += F("<select id='pair' name='pair'>");
  if (pairCount == 0) {
    page += F("<option value='0'>Aucune paire chargee</option>");
  } else {
    for (size_t i = 0; i < pairCount; ++i) {
      page += F("<option value='");
      page += String(i);
      page += F("'");
      if (i == selectedPairIndex) {
        page += F(" selected");
      }
      page += F(">");
      page += htmlEscape(pairFirstWords[i]);
      page += F(" -> ");
      page += htmlEscape(pairSecondWords[i]);
      page += F("</option>");
    }
  }
  page += F("</select>");
  page += F("<div class='row'><button type='submit' formaction='/save'>Sauvegarder</button><button type='submit'>Ecrire maintenant</button></div>");
  page += F("</form>");
  page += F("<p class='muted'>Appuyer sur Entree dans le champ texte sauvegarde les parametres puis ecrit le texte via le clavier HID.</p>");
  page += F("<form method='post' action='/add-pair' accept-charset='UTF-8'>");
  page += F("<label for='newFirst'>Ajouter une paire - nom</label>");
  page += F("<input id='newFirst' name='first' type='text' placeholder='nom-affiche'>");
  page += F("<label for='newSecond'>Ajouter une paire - texte HID</label>");
  page += F("<input id='newSecond' name='second' type='text' placeholder='texte-a-ecrire'>");
  page += F("<div class='row'><button type='submit'>Memoriser cette paire</button><a href='/download' style='box-sizing:border-box;flex:1;text-align:center;text-decoration:none;font-size:16px;padding:10px 14px;margin-top:16px;border-radius:6px;background:#374151;color:white'>Telecharger le parametrage</a></div>");
  page += F("</form>");
  page += F("<form method='post' action='/upload' enctype='multipart/form-data'>");
  page += F("<label for='pairs'>Fichier de parametrage</label>");
  page += F("<div id='drop' class='drop'>Deposez ici un fichier de parametrage .txt, .csv ou .tsv<br>ou choisissez un fichier ci-dessous</div>");
  page += F("<input id='pairs' name='pairs' type='file' accept='.txt,.csv,.tsv'>");
  page += F("<label style='font-weight:400'><input id='mergeUpload' name='merge' type='checkbox' value='1' style='width:auto;margin-right:8px'>Fusionner avec les paires existantes</label>");
  page += F("<button type='submit'>Deposer le fichier</button>");
  page += F("</form>");
  page += F("<p class='muted'>Par defaut l'import remplace la liste de paires. En fusion, une cle de paire existante met a jour sa valeur et une cle nouvelle est ajoutee. Les cles app.* mettent toujours a jour les parametres.</p>");
  page += F("<p class='muted'>Format: cles app.* pour les parametres, puis une paire HID par ligne. Exemple: app.typePin=14 puis nom;mot-a-ecrire</p>");
  page += F("<form method='post' action='/firmware' enctype='multipart/form-data'>");
  page += F("<label for='firmware'>Mise a jour firmware</label>");
  page += F("<div id='firmwareDrop' class='drop'>Deposez ici le fichier firmware.bin<br>ou choisissez un fichier ci-dessous</div>");
  page += F("<input id='firmware' name='firmware' type='file' accept='.bin'>");
  page += F("<button type='submit'>Mettre a jour le binaire</button>");
  page += F("</form>");
  page += F("<p class='muted'>Utilisez le fichier .pio/build/esp32s3-usb-hid/firmware.bin. Ne coupez pas l'alimentation pendant la mise a jour.</p>");
  page += F("<script>");
  page += F("function upload(path,field,file,reload){const fd=new FormData();fd.append(field,file,file.name);if(field==='pairs'&&document.getElementById('mergeUpload').checked)fd.append('merge','1');fetch(path,{method:'POST',body:fd}).then(r=>r.text()).then(t=>{alert(t||'OK');if(reload)location.reload();});}");
  page += F("function bindDrop(id,path,field,reload){const d=document.getElementById(id);['dragenter','dragover'].forEach(e=>d.addEventListener(e,x=>{x.preventDefault();d.classList.add('active');}));['dragleave','drop'].forEach(e=>d.addEventListener(e,x=>{x.preventDefault();d.classList.remove('active');}));d.addEventListener('drop',e=>{const f=e.dataTransfer.files[0];if(f)upload(path,field,f,reload);});}");
  page += F("bindDrop('drop','/upload','pairs',true);");
  page += F("bindDrop('firmwareDrop','/firmware','firmware',false);");
  page += F("</script>");
  page += F("</main></body></html>");
  webServer.send(200, "text/html; charset=utf-8", page);
}

// Route POST /save : sauvegarde les paramètres sans écrire de texte HID.
void handleSaveSettings() {
  logHttpRequest("handleSaveSettings");
  applyWebSettings();
  webServer.sendHeader("Location", "/");
  webServer.send(303);
}

// Route POST /type : sauvegarde les paramètres puis écrit le texte HID configuré.
void handleSaveAndType() {
  logHttpRequest("handleSaveAndType");
  applyWebSettings();
  const String textToType = selectedTypeText();
  Serial.print("HTTP /type ecriture HID: ");
  Serial.println(textToType);
  setStatusLedRed();
  typeAzertyString(textToType.c_str());
  setStatusLedOff();
  webServer.sendHeader("Location", "/");
  webServer.send(303);
}

// Route POST /add-pair : ajoute une paire saisie dans la page et la rend active.
void handleAddPair() {
  logHttpRequest("handleAddPair");

  if (!webServer.hasArg("first") || !webServer.hasArg("second")) {
    webServer.send(400, "text/plain; charset=utf-8", "Champs first/second manquants");
    return;
  }

  if (!addWordPair(webServer.arg("first"), webServer.arg("second"))) {
    if (pairCount >= MAX_WORD_PAIRS) {
      signalPairListFull();
    }
    webServer.send(400, "text/plain; charset=utf-8", "Paire vide ou limite atteinte");
    return;
  }

  selectedPairIndex = pairCount - 1;
  saveSettings();
  Serial.print("Paire ajoutee index=");
  Serial.println(selectedPairIndex);
  signalImportSuccess();
  webServer.sendHeader("Location", "/");
  webServer.send(303);
}

// Route GET /download : exporte les paramètres et toutes les paires en fichier texte réimportable.
void handleDownloadPairs() {
  logHttpRequest("handleDownloadPairs");

  String output;
  output.reserve(700 + pairCount * 48);
  output += F("# Parametres de l'application\n");
  output += F("app.apSsid=");
  output += configFileEscape(configApSsid);
  output += '\n';
  output += F("app.apPass=");
  output += configFileEscape(configApPassword);
  output += '\n';
  output += F("app.typePin=");
  output += String(typeAzertyButtonPin);
  output += '\n';
  output += F("app.terminalPin=");
  output += String(terminalButtonPin);
  output += '\n';
  output += F("app.wifiPin=");
  output += String(wifiToggleButtonPin);
  output += '\n';
  output += F("app.typeText=");
  output += configFileEscape(typeAzertyButtonText);
  output += '\n';
  output += F("app.demoOn=");
  output += startupDemoEnabled ? F("1") : F("0");
  output += '\n';
  output += F("app.demoText=");
  output += configFileEscape(startupDemoText);
  output += '\n';
  output += F("app.selectedPair=");
  output += String(selectedPairIndex);
  output += F("\n\n# Paires HID: nom=texte_a_ecrire\n");
  for (size_t i = 0; i < pairCount; ++i) {
    output += configFileEscape(pairFirstWords[i]);
    output += '=';
    output += configFileEscape(pairSecondWords[i]);
    output += '\n';
  }

  webServer.sendHeader("Content-Disposition", "attachment; filename=\"parametrage_application.txt\"");
  webServer.send(200, "text/plain; charset=utf-8", output);
}

// Route POST /upload : finalise l'import de fichier et sauvegarde la liste de paires.
void handleUploadDone() {
  logHttpRequest("handleUploadDone");
  const bool mergeMode = webServer.hasArg("merge") && webServer.arg("merge") == "1";
  bool fullReached = false;
  const size_t loadedPairs = parseWordPairs(uploadedPairsText, mergeMode, fullReached);
  selectedPairIndex = 0;
  saveSettings();
  Serial.print(mergeMode ? "Parametrage importe, paires fusionnees/importees: " : "Parametrage importe, paires remplacees/importees: ");
  Serial.println(loadedPairs);

  if (fullReached) {
    Serial.println("Import limite: liste de paires pleine.");
    signalPairListFull();
  } else {
    signalImportSuccess();
  }

  webServer.sendHeader("Location", "/");
  webServer.send(303);
}

// Accumule le contenu du fichier uploadé dans une chaîne bornée.
void handlePairsFileUpload() {
  HTTPUpload& upload = webServer.upload();

  if (upload.status == UPLOAD_FILE_START) {
    uploadedPairsText = "";
    Serial.print("HTTP upload start: ");
    Serial.println(upload.filename);
    return;
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    for (size_t i = 0; i < upload.currentSize && uploadedPairsText.length() < MAX_PAIR_UPLOAD_BYTES; ++i) {
      uploadedPairsText += static_cast<char>(upload.buf[i]);
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_END) {
    Serial.print("HTTP upload end bytes=");
    Serial.println(upload.totalSize);
  }
}

// Route POST /firmware : finalise l'OTA puis redémarre si l'image est valide.
void handleFirmwareUpdateDone() {
  logHttpRequest("handleFirmwareUpdateDone");

  if (!firmwareUpdateOk) {
    const String message = firmwareUpdateError.isEmpty() ? "Echec de mise a jour firmware" : firmwareUpdateError;
    Serial.print("OTA echec: ");
    Serial.println(message);
    webServer.send(500, "text/plain; charset=utf-8", message);
    return;
  }

  webServer.send(200, "text/plain; charset=utf-8", "Firmware recu. Redemarrage...");
  Serial.println("OTA succes: redemarrage.");
  delay(500);
  ESP.restart();
}

// Écrit le binaire uploadé dans la partition OTA inactive.
void handleFirmwareFileUpload() {
  HTTPUpload& upload = webServer.upload();

  if (upload.status == UPLOAD_FILE_START) {
    firmwareUpdateOk = false;
    firmwareUpdateError = "";
    Serial.print("OTA start: ");
    Serial.println(upload.filename);

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      firmwareUpdateError = "Update.begin() a echoue";
      Update.printError(Serial);
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (firmwareUpdateError.isEmpty() && Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      firmwareUpdateError = "Update.write() incomplet";
      Update.printError(Serial);
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_END) {
    if (!firmwareUpdateError.isEmpty()) {
      Update.abort();
      return;
    }

    if (!Update.end(true)) {
      firmwareUpdateError = "Update.end() a echoue";
      Update.printError(Serial);
      return;
    }

    firmwareUpdateOk = !Update.hasError();
    Serial.print("OTA end bytes=");
    Serial.println(upload.totalSize);
  }
}

// Enregistre les routes HTTP une seule fois pour pouvoir arrêter/redémarrer le WiFi.
void configureWebRoutes() {
  if (webRoutesConfigured) {
    return;
  }

  webServer.on("/", HTTP_GET, sendConfigPage);
  webServer.on("/save", HTTP_POST, handleSaveSettings);
  webServer.on("/type", HTTP_POST, handleSaveAndType);
  webServer.on("/add-pair", HTTP_POST, handleAddPair);
  webServer.on("/download", HTTP_GET, handleDownloadPairs);
  webServer.on("/upload", HTTP_POST, handleUploadDone, handlePairsFileUpload);
  webServer.on("/firmware", HTTP_POST, handleFirmwareUpdateDone, handleFirmwareFileUpload);
  webServer.onNotFound([]() {
    logHttpRequest("notFound");
    webServer.send(404, "text/plain; charset=utf-8", "Not found");
  });
  webRoutesConfigured = true;
}

// Démarre le point d'accès WiFi et le serveur HTTP local.
void startConfigWebServer(bool signalLed = false) {
  if (wifiConfigEnabled) {
    return;
  }

  configureWebRoutes();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(configApSsid.c_str(), configApPassword.c_str());
  webServer.begin();
  wifiConfigEnabled = true;

  Serial.print("WiFi AP: ");
  Serial.println(configApSsid);
  Serial.print("Web config: http://");
  Serial.println(WiFi.softAPIP());

  if (signalLed) {
    signalImportSuccess();
  }
}

// Arrête le serveur HTTP et coupe le point d'accès WiFi.
void stopConfigWebServer(bool signalLed = false) {
  if (!wifiConfigEnabled) {
    return;
  }

  webServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiConfigEnabled = false;
  Serial.println("WiFi config arrete.");

  if (signalLed) {
    signalPairListFull();
  }
}

// Bascule le WiFi de configuration depuis le bouton physique dédié.
void toggleConfigWifi() {
  if (wifiConfigEnabled) {
    stopConfigWebServer(true);
  } else {
    startConfigWebServer(true);
  }
}

// Déplace la souris par pas HID relatifs limités à [-127, 127].
void moveRelativePixels(int deltaX, int deltaY) {
  while (deltaX != 0 || deltaY != 0) {
    const int stepX = constrain(deltaX, HID_MOUSE_MIN_DELTA, HID_MOUSE_MAX_DELTA);
    const int stepY = constrain(deltaY, HID_MOUSE_MIN_DELTA, HID_MOUSE_MAX_DELTA);

    Mouse.move(stepX, stepY);
    deltaX -= stepX;
    deltaY -= stepY;
    delay(MOVE_STEP_DELAY_MS);
  }
}

// Approxime le centre écran en partant du coin haut gauche.
void movePointerToApproximateScreenCenter() {
  // Une souris HID classique envoie des déplacements relatifs et ne connaît pas
  // la résolution réelle de l'hôte. On force donc d'abord le pointeur vers le
  // coin supérieur gauche, puis on avance jusqu'au centre configuré.
  moveRelativePixels(-SCREEN_WIDTH_PX * 2, -SCREEN_HEIGHT_PX * 2);
  delay(ACTION_DELAY_MS);
  moveRelativePixels(SCREEN_WIDTH_PX / 2, SCREEN_HEIGHT_PX / 2);
}

// Démo exécutée une seule fois après énumération USB.
void runKeyboardMouseDemo() {
  movePointerToApproximateScreenCenter();
  delay(ACTION_DELAY_MS);

  Mouse.click(MOUSE_LEFT);
  delay(ACTION_DELAY_MS);

  setStatusLedRed();
  typeAzertyString(startupDemoText.c_str());
  setStatusLedOff();
}

// Envoie un appui puis relâchement sur un code HID brut.
void tapRawKey(uint8_t key) {
  Keyboard.pressRaw(key);
  delay(KEY_PRESS_DELAY_MS);
  Keyboard.releaseRaw(key);
  delay(KEY_PRESS_DELAY_MS);
}

// Envoie une touche AZERTY avec son modificateur éventuel, puis une touche de suite si nécessaire.
void tapAzertyKey(AzertyKey key) {
  if (key.modifier != 0) {
    Keyboard.press(key.modifier);
    delay(KEY_PRESS_DELAY_MS);
  }

  tapRawKey(key.rawKey);

  if (key.modifier != 0) {
    Keyboard.release(key.modifier);
    delay(KEY_PRESS_DELAY_MS);
  }

  if (key.followRawKey != 0) {
    if (key.followModifier != 0) {
      Keyboard.press(key.followModifier);
      delay(KEY_PRESS_DELAY_MS);
    }

    tapRawKey(key.followRawKey);

    if (key.followModifier != 0) {
      Keyboard.release(key.followModifier);
      delay(KEY_PRESS_DELAY_MS);
    }
  }
}

// Convertit une lettre A-Z en code HID brut pour les raccourcis Ctrl+lettre.
uint8_t rawKeyForControlLetter(char letter) {
  if (letter >= 'a' && letter <= 'z') {
    letter = static_cast<char>(letter - 'a' + 'A');
  }

  switch (letter) {
    case 'A':
      return RAW_KEY_A;
    case 'B':
      return RAW_KEY_B;
    case 'C':
      return RAW_KEY_C;
    case 'D':
      return RAW_KEY_D;
    case 'E':
      return RAW_KEY_E;
    case 'F':
      return RAW_KEY_F;
    case 'G':
      return RAW_KEY_G;
    case 'H':
      return RAW_KEY_H;
    case 'I':
      return RAW_KEY_I;
    case 'J':
      return RAW_KEY_J;
    case 'K':
      return RAW_KEY_K;
    case 'L':
      return RAW_KEY_L;
    case 'M':
      return RAW_KEY_M;
    case 'N':
      return RAW_KEY_N;
    case 'O':
      return RAW_KEY_O;
    case 'P':
      return RAW_KEY_P;
    case 'Q':
      return RAW_KEY_Q;
    case 'R':
      return RAW_KEY_R;
    case 'S':
      return RAW_KEY_S;
    case 'T':
      return RAW_KEY_T;
    case 'U':
      return RAW_KEY_U;
    case 'V':
      return RAW_KEY_V;
    case 'W':
      return RAW_KEY_W;
    case 'X':
      return RAW_KEY_X;
    case 'Y':
      return RAW_KEY_Y;
    case 'Z':
      return RAW_KEY_Z;
    default:
      return 0;
  }
}

// Envoie un raccourci Ctrl+lettre, par exemple Ctrl-C ou Ctrl-D.
void tapCtrlLetter(char letter) {
  const uint8_t rawKey = rawKeyForControlLetter(letter);
  if (rawKey == 0) {
    return;
  }

  Keyboard.press(KEY_LEFT_CTRL);
  delay(KEY_PRESS_DELAY_MS);
  tapRawKey(rawKey);
  Keyboard.release(KEY_LEFT_CTRL);
  delay(KEY_PRESS_DELAY_MS);
}

// Interprète les séquences échappées du fichier de paramétrage.
bool typeEscapedSequence(const unsigned char*& cursor) {
  if (cursor[0] != '\\' || cursor[1] == '\0') {
    return false;
  }

  switch (cursor[1]) {
    case 't':
      tapRawKey(RAW_KEY_TAB);
      cursor += 1;
      return true;
    case 'r':
    case 'n':
      tapRawKey(RAW_KEY_ENTER);
      cursor += 1;
      return true;
    case 'b':
      tapRawKey(RAW_KEY_BACKSPACE);
      cursor += 1;
      return true;
    case 'e':
      tapRawKey(RAW_KEY_ESC);
      cursor += 1;
      return true;
    case '\\':
    case '^': {
      const AzertyKey key = rawKeyForAzertyAsciiChar(static_cast<char>(cursor[1]));
      if (key.rawKey != 0) {
        tapAzertyKey(key);
      }
      cursor += 1;
      return true;
    }
    case 'c':
    case 'C':
      if (cursor[2] != '\0') {
        tapCtrlLetter(static_cast<char>(cursor[2]));
        cursor += 2;
        return true;
      }
      return false;
    default:
      return false;
  }
}

// Convertit un caractère ASCII imprimable en touche physique pour Windows AZERTY FR.
AzertyKey rawKeyForAzertyAsciiChar(char character) {
  switch (character) {
    case '\b':
      return {RAW_KEY_BACKSPACE, 0};
    case '\t':
      return {RAW_KEY_TAB, 0};
    case '\n':
      return {RAW_KEY_ENTER, 0};
    case 0x1B:
      return {RAW_KEY_ESC, 0};
    case ' ':
      return {RAW_KEY_SPACE, 0};
    case '!':
      return {RAW_KEY_SLASH, 0};
    case '"':
      return {RAW_KEY_3, 0};
    case '#':
      return {RAW_KEY_3, KEY_RIGHT_ALT};
    case '$':
      return {RAW_KEY_RIGHT_BRACKET, 0};
    case '%':
      return {RAW_KEY_APOSTROPHE, KEY_LEFT_SHIFT};
    case '&':
      return {RAW_KEY_1, 0};
    case '\'':
      return {RAW_KEY_4, 0};
    case '(':
      return {RAW_KEY_5, 0};
    case ')':
      return {RAW_KEY_MINUS, 0};
    case '*':
      return {RAW_KEY_BACKSLASH, 0};
    case '+':
      return {RAW_KEY_EQUAL, KEY_LEFT_SHIFT};
    case ',':
      return {RAW_KEY_M, 0};
    case '-':
      return {RAW_KEY_6, 0};
    case '.':
      return {RAW_KEY_COMMA, KEY_LEFT_SHIFT};
    case '/':
      return {RAW_KEY_DOT, KEY_LEFT_SHIFT};
    case '0':
      return {RAW_KEY_0, KEY_LEFT_SHIFT};
    case '1':
      return {RAW_KEY_1, KEY_LEFT_SHIFT};
    case '2':
      return {RAW_KEY_2, KEY_LEFT_SHIFT};
    case '3':
      return {RAW_KEY_3, KEY_LEFT_SHIFT};
    case '4':
      return {RAW_KEY_4, KEY_LEFT_SHIFT};
    case '5':
      return {RAW_KEY_5, KEY_LEFT_SHIFT};
    case '6':
      return {RAW_KEY_6, KEY_LEFT_SHIFT};
    case '7':
      return {RAW_KEY_7, KEY_LEFT_SHIFT};
    case '8':
      return {RAW_KEY_8, KEY_LEFT_SHIFT};
    case '9':
      return {RAW_KEY_9, KEY_LEFT_SHIFT};
    case ':':
      return {RAW_KEY_DOT, 0};
    case ';':
      return {RAW_KEY_COMMA, 0};
    case '<':
      return {RAW_KEY_NON_US_BACKSLASH, 0};
    case '=':
      return {RAW_KEY_EQUAL, 0};
    case '>':
      return {RAW_KEY_NON_US_BACKSLASH, KEY_LEFT_SHIFT};
    case '?':
      return {RAW_KEY_M, KEY_LEFT_SHIFT};
    case '@':
      return {RAW_KEY_0, KEY_RIGHT_ALT};
    case 'a':
      return {RAW_KEY_Q, 0};
    case 'b':
      return {RAW_KEY_B, 0};
    case 'c':
      return {RAW_KEY_C, 0};
    case 'd':
      return {RAW_KEY_D, 0};
    case 'e':
      return {RAW_KEY_E, 0};
    case 'f':
      return {RAW_KEY_F, 0};
    case 'g':
      return {RAW_KEY_G, 0};
    case 'h':
      return {RAW_KEY_H, 0};
    case 'i':
      return {RAW_KEY_I, 0};
    case 'j':
      return {RAW_KEY_J, 0};
    case 'k':
      return {RAW_KEY_K, 0};
    case 'l':
      return {RAW_KEY_L, 0};
    case 'm':
      return {RAW_KEY_SEMICOLON, 0};
    case 'n':
      return {RAW_KEY_N, 0};
    case 'o':
      return {RAW_KEY_O, 0};
    case 'p':
      return {RAW_KEY_P, 0};
    case 'q':
      return {RAW_KEY_A, 0};
    case 'r':
      return {RAW_KEY_R, 0};
    case 's':
      return {RAW_KEY_S, 0};
    case 't':
      return {RAW_KEY_T, 0};
    case 'u':
      return {RAW_KEY_U, 0};
    case 'v':
      return {RAW_KEY_V, 0};
    case 'w':
      return {RAW_KEY_Z, 0};
    case 'x':
      return {RAW_KEY_X, 0};
    case 'y':
      return {RAW_KEY_Y, 0};
    case 'z':
      return {RAW_KEY_W, 0};
    case 'A':
      return {RAW_KEY_Q, KEY_LEFT_SHIFT};
    case 'B':
      return {RAW_KEY_B, KEY_LEFT_SHIFT};
    case 'C':
      return {RAW_KEY_C, KEY_LEFT_SHIFT};
    case 'D':
      return {RAW_KEY_D, KEY_LEFT_SHIFT};
    case 'E':
      return {RAW_KEY_E, KEY_LEFT_SHIFT};
    case 'F':
      return {RAW_KEY_F, KEY_LEFT_SHIFT};
    case 'G':
      return {RAW_KEY_G, KEY_LEFT_SHIFT};
    case 'H':
      return {RAW_KEY_H, KEY_LEFT_SHIFT};
    case 'I':
      return {RAW_KEY_I, KEY_LEFT_SHIFT};
    case 'J':
      return {RAW_KEY_J, KEY_LEFT_SHIFT};
    case 'K':
      return {RAW_KEY_K, KEY_LEFT_SHIFT};
    case 'L':
      return {RAW_KEY_L, KEY_LEFT_SHIFT};
    case 'M':
      return {RAW_KEY_SEMICOLON, KEY_LEFT_SHIFT};
    case 'N':
      return {RAW_KEY_N, KEY_LEFT_SHIFT};
    case 'O':
      return {RAW_KEY_O, KEY_LEFT_SHIFT};
    case 'P':
      return {RAW_KEY_P, KEY_LEFT_SHIFT};
    case 'Q':
      return {RAW_KEY_A, KEY_LEFT_SHIFT};
    case 'R':
      return {RAW_KEY_R, KEY_LEFT_SHIFT};
    case 'S':
      return {RAW_KEY_S, KEY_LEFT_SHIFT};
    case 'T':
      return {RAW_KEY_T, KEY_LEFT_SHIFT};
    case 'U':
      return {RAW_KEY_U, KEY_LEFT_SHIFT};
    case 'V':
      return {RAW_KEY_V, KEY_LEFT_SHIFT};
    case 'W':
      return {RAW_KEY_Z, KEY_LEFT_SHIFT};
    case 'X':
      return {RAW_KEY_X, KEY_LEFT_SHIFT};
    case 'Y':
      return {RAW_KEY_Y, KEY_LEFT_SHIFT};
    case 'Z':
      return {RAW_KEY_W, KEY_LEFT_SHIFT};
    case '[':
      return {RAW_KEY_5, KEY_RIGHT_ALT};
    case '\\':
      return {RAW_KEY_8, KEY_RIGHT_ALT};
    case ']':
      return {RAW_KEY_MINUS, KEY_RIGHT_ALT};
    case '^':
      return {RAW_KEY_LEFT_BRACKET, 0, RAW_KEY_SPACE, 0};
    case '_':
      return {RAW_KEY_8, 0};
    case '`':
      return {RAW_KEY_7, KEY_RIGHT_ALT, RAW_KEY_SPACE, 0};
    case '{':
      return {RAW_KEY_4, KEY_RIGHT_ALT};
    case '|':
      return {RAW_KEY_6, KEY_RIGHT_ALT};
    case '}':
      return {RAW_KEY_EQUAL, KEY_RIGHT_ALT};
    case '~':
      return {RAW_KEY_2, KEY_RIGHT_ALT, RAW_KEY_SPACE, 0};
    default:
      return {0, 0};
  }
}

// Parcourt une chaîne UTF-8 et tape les caractères supportés en AZERTY FR.
void typeAzertyString(const char* text) {
  for (const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text); *cursor != '\0'; ++cursor) {
    if (typeEscapedSequence(cursor)) {
      continue;
    }

    if (cursor[0] == 0xC3 && cursor[1] == 0xA9) {
      tapRawKey(RAW_KEY_2);
      ++cursor;
      continue;
    }

    if (cursor[0] == 0xC3 && cursor[1] == 0xA0) {
      tapRawKey(RAW_KEY_0);
      ++cursor;
      continue;
    }

    if (cursor[0] == 0xC3 && cursor[1] == 0xB4) {
      tapRawKey(RAW_KEY_LEFT_BRACKET);
      tapRawKey(RAW_KEY_O);
      ++cursor;
      continue;
    }

    if (cursor[0] == 0xC3 && cursor[1] == 0xB9) {
      tapRawKey(RAW_KEY_APOSTROPHE);
      ++cursor;
      continue;
    }

    const AzertyKey key = rawKeyForAzertyAsciiChar(static_cast<char>(*cursor));
    if (key.rawKey != 0) {
      tapAzertyKey(key);
    }
  }
}

// Écrit le texte configuré pour le GPIO de saisie.
void typeAzertyText() {
  const String textToType = selectedTypeText();
  Serial.print("Bouton ecriture HID: ");
  Serial.println(textToType);
  setStatusLedRed();
  typeAzertyString(textToType.c_str());
  setStatusLedOff();
}

// Ouvre Windows Terminal via Win+R puis saisie de "wt".
void openWindowsTerminal() {
  Keyboard.press(KEY_LEFT_GUI);
  delay(KEY_PRESS_DELAY_MS);
  Keyboard.pressRaw(RAW_KEY_R);
  delay(KEY_PRESS_DELAY_MS);
  Keyboard.releaseRaw(RAW_KEY_R);
  Keyboard.release(KEY_LEFT_GUI);

  delay(RUN_DIALOG_DELAY_MS);

  // "wt" en disposition AZERTY FR, puis Entrée.
  setStatusLedRed();
  tapRawKey(RAW_KEY_Z);
  tapRawKey(RAW_KEY_T);
  tapRawKey(RAW_KEY_ENTER);
  setStatusLedOff();
}
}  // namespace

void setup() {
  Serial.begin(115200);

  loadSettings();
  setStatusLedOff();

  Keyboard.onEvent(ARDUINO_USB_HID_KEYBOARD_LED_EVENT, onKeyboardLedEvent);
  Keyboard.begin();
  Mouse.begin();
  USB.begin();

  delay(USB_ENUMERATION_DELAY_MS);

  startConfigWebServer(true);

  typeButtonWasPressed = digitalRead(typeAzertyButtonPin) == LOW;
  terminalButtonWasPressed = digitalRead(terminalButtonPin) == LOW;
  wifiToggleButtonWasPressed = digitalRead(wifiToggleButtonPin) == LOW;

  Serial.print("Bouton ecriture: GPIO");
  Serial.println(typeAzertyButtonPin);
  Serial.print("Bouton terminal: GPIO");
  Serial.println(terminalButtonPin);
  Serial.print("Bouton WiFi: GPIO");
  Serial.println(wifiToggleButtonPin);
  Serial.println("Cablage attendu: chaque bouton relie son GPIO a GND pendant l'appui.");
}

void loop() {
  if (!demoAlreadyRun) {
    demoAlreadyRun = true;
    if (startupDemoEnabled) {
      runKeyboardMouseDemo();
    }
  }

  if (readButtonPress(typeAzertyButtonPin, typeButtonWasPressed)) {
    Serial.print("GPIO");
    Serial.print(typeAzertyButtonPin);
    Serial.println(" appuye: ecriture de la chaine.");
    typeAzertyText();
  }

  if (readButtonPress(terminalButtonPin, terminalButtonWasPressed)) {
    Serial.print("GPIO");
    Serial.print(terminalButtonPin);
    Serial.println(" appuye: ouverture du terminal.");
    openWindowsTerminal();
  }

  if (readButtonPress(wifiToggleButtonPin, wifiToggleButtonWasPressed)) {
    Serial.print("GPIO");
    Serial.print(wifiToggleButtonPin);
    Serial.println(" appuye: bascule WiFi.");
    toggleConfigWifi();
  }

  if (wifiConfigEnabled) {
    webServer.handleClient();
  }
  handleStatusLed();
  delay(10);
}
