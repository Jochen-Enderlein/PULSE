# LED Spotlight - Scheinwerfer-Firmware

ESP32-basierter LED-Scheinwerfer mit 2 WS2812B LED-Ringen.
Empfängt Befehle vom Light Commander und führt sie aus.

## 🔦 Hardware-Aufbau

```
┌─────────────────────────────────┐
│  ESP32 DevKit                   │
│                                 │
│  GPIO16 ─→ Innerer Ring (8 LEDs)│
│  GPIO17 ─→ Äußerer Ring (26 LEDs)│
│  GND    ─→ LED GND              │
│  5V     ─→ LED VCC              │
└─────────────────────────────────┘

Physischer Aufbau:
┌─────────────────────┐
│ Vorne (mit Linse)   │
│   ┌─────────┐       │
│   │ 26 LEDs │ ← Outer
│   └─────────┘       │
│   ┌─────────┐       │
│   │  Linse  │       │
│   └─────────┘       │
│                     │
│ ═══════════════     │ Röhre
│                     │
│   ┌─────────┐       │
│   │ 8 LEDs  │ ← Inner
│   └─────────┘       │
│                     │
│ Hinten              │
└─────────────────────┘
```

## 📋 Hardware-Anforderungen

- **ESP32 DevKit** (oder ähnlich)
- **WS2812B LED-Strips**:
  - Innerer Ring: 8 LEDs
  - Äußerer Ring: 26 LEDs
- **5V Netzteil** (mindestens 2A bei voller Helligkeit)
- **Bi-Konvex Linse** für Spotlight-Effekt

## 🚀 Installation

### 1. Arduino IDE Setup

1. Installiere Arduino IDE
2. Füge ESP32 Board Support hinzu
3. Installiere Bibliotheken:
   - **FastLED** (über Bibliotheksverwalter)
   - **ArduinoJson** (Version 6.x)

### 2. Hardware verkabeln

```
ESP32 Pin 16 → Data-In vom inneren Ring (8 LEDs)
ESP32 Pin 17 → Data-In vom äußeren Ring (26 LEDs)
ESP32 GND    → LED GND (beide Ringe)
ESP32 5V     → LED VCC über Netzteil
```

**Wichtig:** 
- LEDs brauchen separates 5V Netzteil (nicht vom ESP32!)
- GND vom ESP32 und LEDs verbinden
- Data-Leitung mit 330Ω Widerstand schützen (optional)

### 3. Code konfigurieren

Öffne `led-spotlight.ino` und passe an:

```cpp
// WiFi zum Light Commander
const char* WIFI_SSID = "LightCommander";
const char* WIFI_PASSWORD = "lightshow2024";

// Eindeutige ID für diesen Scheinwerfer
const char* SPOTLIGHT_ID = "spot-1";  // 👈 ÄNDERN für jeden Scheinwerfer!
```

**Für mehrere Scheinwerfer:**
- Scheinwerfer 1: `SPOTLIGHT_ID = "spot-1"`
- Scheinwerfer 2: `SPOTLIGHT_ID = "spot-2"`
- Scheinwerfer 3: `SPOTLIGHT_ID = "spot-3"`
- etc.

### 4. Upload

1. ESP32 per USB verbinden
2. Board wählen: "ESP32 Dev Module"
3. Port wählen
4. Upload

### 5. IP-Adresse notieren

Nach dem Start zeigt der Serial Monitor:

```
✓ WiFi connected!
IP: 192.168.4.101
```

Diese IP im Light Commander eintragen!

## 📡 API-Dokumentation

### Endpoints

#### POST /effect
Setzt einen Effekt auf den Ringen.

**Body:**
```json
{
  "ring": "inner",              // "inner" / "outer" / "both"
  "effect": "rotation",
  "brightness": 255,
  "rotation": {
    "activeColor": [255, 0, 0],
    "inactiveColor": [0, 0, 50],
    "speed": 100,
    "direction": "clockwise",
    "pattern": "trail",
    "trailLength": 3
  }
}
```

#### POST /stop
Stoppt Effekte.

**Body (optional):**
```json
{
  "ring": "inner"  // Stoppt nur einen Ring
}
```

Ohne Body: Stoppt beide Ringe.

#### GET /status
Gibt Status zurück.

**Response:**
```json
{
  "id": "spot-1",
  "ip": "192.168.4.101",
  "rssi": -45,
  "uptime": 123456,
  "innerRing": {
    "active": true,
    "effect": "rotation"
  },
  "outerRing": {
    "active": false,
    "effect": "off"
  }
}
```

## 🎨 Unterstützte Effekte

### STATIC - Statische Farbe
```json
{
  "effect": "static",
  "color": [255, 0, 0],
  "brightness": 255
}
```

### FADE - Überblendung
```json
{
  "effect": "fade",
  "color": [255, 0, 0],
  "color2": [0, 0, 255],
  "duration": 3000,
  "brightness": 200
}
```

### STROBE - Stroboskop
```json
{
  "effect": "strobe",
  "color": [255, 255, 255],
  "speed": 15,
  "duration": 2000
}
```

### PULSE - Pulsieren
```json
{
  "effect": "pulse",
  "color": [0, 100, 200],
  "duration": 2000
}
```

### ROTATION - Rotation (mehrere Pattern!)
```json
{
  "effect": "rotation",
  "rotation": {
    "activeColor": [255, 0, 0],
    "inactiveColor": [0, 0, 50],
    "speed": 100,
    "direction": "clockwise",
    "pattern": "trail",
    "trailLength": 3
  }
}
```

**Pattern-Typen:**
- `single` - Einzelner Punkt
- `trail` - Punkt mit Schweif
- `opposite` - Zwei gegenüberliegende Punkte
- `wave` - Mehrere LEDs als Welle

### RAINBOW - Regenbogen
```json
{
  "effect": "rainbow",
  "brightness": 200
}
```

### CHASE - Lauflicht
```json
{
  "effect": "chase",
  "color": [255, 255, 0],
  "speed": 100
}
```

## 🧪 Testen

### 1. Direkt vom Browser

Gehe zu: `http://<scheinwerfer-ip>/`

Dort siehst du Status und API-Endpunkte.

### 2. Mit curl

```bash
# Rotation starten
curl -X POST http://192.168.4.101/effect \
  -H "Content-Type: application/json" \
  -d '{
    "ring": "inner",
    "effect": "rotation",
    "rotation": {
      "activeColor": [255, 0, 0],
      "inactiveColor": [0, 0, 0],
      "speed": 100,
      "pattern": "single"
    }
  }'

# Status abrufen
curl http://192.168.4.101/status

# Stoppen
curl -X POST http://192.168.4.101/stop
```

## 🔧 Pin-Konfiguration anpassen

Wenn du andere Pins nutzen willst, ändere in `LEDSpotlight.h`:

```cpp
#define PIN_INNER_RING    16    // Dein Pin für inneren Ring
#define PIN_OUTER_RING    17    // Dein Pin für äußeren Ring

#define NUM_LEDS_INNER    8     // Anzahl LEDs innen
#define NUM_LEDS_OUTER    26    // Anzahl LEDs außen
```

## ⚡ Stromversorgung

**Wichtig für stabile LEDs:**

### Berechnung:
```
Pro LED: ~60mA bei voller Helligkeit (Weiß)
Innerer Ring: 8 LEDs × 60mA = 480mA
Äußerer Ring: 26 LEDs × 60mA = 1560mA
Gesamt: ~2A bei voller Helligkeit
```

### Empfehlung:
- **5V 3A Netzteil** für einen Scheinwerfer
- **5V 10A Netzteil** für 4 Scheinwerfer
- Oder separate Netzteile pro Scheinwerfer

**Niemals vom ESP32 5V Pin versorgen!** Maximal 500mA möglich.

## 🔍 Troubleshooting

### LEDs leuchten nicht

1. **Stromversorgung prüfen**
   - Separates 5V Netzteil?
   - GND verbunden?

2. **Pin-Konfiguration prüfen**
   - GPIO16 und GPIO17 richtig verkabelt?
   - In LEDSpotlight.h die richtigen Pins?

3. **LED-Typ prüfen**
   - WS2812B? (Nicht WS2811!)
   - Color-Order GRB? (In LEDSpotlight.cpp)

### WiFi verbindet nicht

1. **SSID/Passwort prüfen**
2. **2.4 GHz verfügbar?**
3. **Signal zu schwach?** → Näher ran

### Effekte ruckeln

1. **Stromversorgung schwach?** → Besseres Netzteil
2. **WiFi instabil?** → Näher an Commander / bessere Antenne

## 📊 Performance

- **Effekt-Update Rate:** 60 FPS
- **HTTP Requests/Sekunde:** ~100
- **RAM Nutzung:** ~50 KB
- **CPU Last:** ~5-10%

## 🎯 Integration mit Light Commander

### Scheinwerfer hinzufügen:

```bash
curl -X POST http://light-commander-ip/api/spotlight/add \
  -H "Content-Type: application/json" \
  -d '{
    "id": "spot-1",
    "name": "Front Left",
    "ip": "192.168.4.101"
  }'
```

### In Sequenz nutzen:

```json
{
  "timestamp": 1000,
  "targets": ["spot-1"],
  "ring": "inner",
  "effect": "rotation",
  "params": {
    "rotation": {
      "activeColor": [255, 0, 0],
      "speed": 100,
      "pattern": "trail"
    }
  }
}
```

## 🔄 Updates

Um Code zu aktualisieren:
1. Änderungen in .cpp/.h Dateien machen
2. Neu kompilieren
3. Upload über USB oder OTA (zukünftig)

## 📝 Lizenz

MIT License - Nutze es wie du willst!

## 🎸 Pink Floyd Style!

Mit diesen Scheinwerfern und dem Light Commander kannst du epische Shows wie die Pulse Tour nachbauen! ✨

---

**Viel Spaß mit deinen LED-Scheinwerfern! 💡🔦**
