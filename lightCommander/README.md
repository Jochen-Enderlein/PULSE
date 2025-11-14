# Light Commander

Zentrale Steuereinheit für ein LED-Scheinwerfer-System mit WLED-basierten Scheinwerfern.

## 🎯 Übersicht

Der Light Commander ist ein ESP32-basiertes Steuergerät, das mehrere LED-Scheinwerfer orchestriert. Jeder Scheinwerfer besteht aus:
- **Innerem Ring**: 8 LEDs hinter Bi-Konvex-Linse (fokussierter Spotlight)
- **Äußerem Ring**: 26 LEDs (Umgebungsbeleuchtung)

## ✨ Features

- ✅ **Multi-Device Control**: Steuere beliebig viele Scheinwerfer
- ✅ **REST API**: Vollständige Kontrolle über HTTP
- ✅ **Sequenz-Player**: Choreographierte Light-Shows
- ✅ **Rotation-Effekte**: Verschiedene Rotationsmuster (Single, Trail, Opposite, Wave)
- ✅ **Effekte**: Static, Fade, Strobe, Pulse, Rainbow, Chase
- ✅ **Individuelle Ring-Steuerung**: Inner/Outer/Both
- 🔜 **Spotify-Integration**: Vorbereitet für Musik-Synchronisation

## 📋 Hardware-Anforderungen

- ESP32 DevKit oder ähnlich
- WiFi-Verbindung
- WLED-Controller an jedem Scheinwerfer

## 🚀 Installation

### 1. Arduino IDE Setup

1. Installiere die Arduino IDE (1.8.19 oder höher)
2. Füge ESP32 Board Support hinzu:
   - Datei → Voreinstellungen → Zusätzliche Boardverwalter-URLs:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Installiere ESP32 Boards über den Boardverwalter

### 2. Bibliotheken installieren

Installiere folgende Bibliotheken über den Bibliotheksverwalter:

- `ArduinoJson` (Version 6.x)
- `ESPAsyncWebServer` (optional, für verbesserte Performance)

### 3. Projekt hochladen

1. Öffne `light-commander.ino`
2. Passe WiFi-Einstellungen an:
   ```cpp
   const char* WIFI_SSID = "DeinWiFi";
   const char* WIFI_PASSWORD = "DeinPasswort";
   const bool AP_MODE = false;  // true für Access Point
   ```
3. Wähle Board: "ESP32 Dev Module"
4. Upload

## 📡 API-Dokumentation

### Basis-URL

```
http://<ESP32-IP>/api/
```

### Endpoints

#### 1. Status abrufen

```http
GET /api/status
```

**Response:**
```json
{
  "uptime": 123456,
  "freeHeap": 234567,
  "wifiConnected": true,
  "ip": "192.168.1.100",
  "playback": {
    "active": true,
    "sequence": "pulse-intro",
    "position": 5000,
    "paused": false,
    "spotifyConnected": false
  },
  "devices": [
    {
      "id": "spot-1",
      "name": "Front Left",
      "online": true,
      "ip": "192.168.1.101"
    }
  ]
}
```

#### 2. Scheinwerfer hinzufügen

```http
POST /api/spotlight/add
Content-Type: application/json

{
  "id": "spot-1",
  "name": "Front Left",
  "ip": "192.168.1.101"
}
```

#### 3. Scheinwerfer auflisten

```http
GET /api/spotlight/list
```

#### 4. Effekt senden

```http
POST /api/effect/send
Content-Type: application/json

{
  "targets": ["spot-1", "spot-2"],
  "ring": "inner",
  "effect": "rotation",
  "rotation": {
    "activeColor": [255, 0, 0],
    "inactiveColor": [0, 0, 50],
    "speed": 100,
    "direction": "clockwise",
    "pattern": "trail",
    "trailFade": true,
    "trailLength": 3
  }
}
```

**Effekt-Typen:**
- `static` - Statische Farbe
- `fade` - Überblendung
- `strobe` - Stroboskop
- `pulse` - Pulsieren
- `rotation` - Rotation (siehe unten)
- `rainbow` - Regenbogen
- `chase` - Lauflicht

**Ring-Optionen:**
- `inner` - Nur innerer Ring (8 LEDs)
- `outer` - Nur äußerer Ring (26 LEDs)
- `both` - Beide Ringe

#### 5. Rotation-Effekt

**Pattern-Typen:**
- `single` - Einzelner rotierender Punkt
- `trail` - Punkt mit ausblendendem Schweif
- `opposite` - Zwei gegenüberliegende Punkte
- `wave` - Mehrere LEDs als Welle
- `rainbow_chase` - Farbwechselnde Verfolgung

**Beispiel: Trail-Rotation**
```json
{
  "targets": ["spot-1"],
  "ring": "inner",
  "effect": "rotation",
  "rotation": {
    "activeColor": [255, 100, 0],
    "inactiveColor": [10, 10, 50],
    "speed": 80,
    "direction": "clockwise",
    "pattern": "trail",
    "trailFade": true,
    "trailLength": 4
  }
}
```

#### 6. Effekt stoppen

```http
POST /api/effect/stop
Content-Type: application/json

{
  "targets": ["spot-1", "spot-2"]
}
```

#### 7. Sequenz laden

```http
POST /api/sequence/load
Content-Type: application/json

{
  "id": "my-show",
  "name": "Meine Show",
  "duration": 60000,
  "loop": false,
  "spotifyUri": "",
  "syncWithSpotify": false,
  "events": [
    {
      "timestamp": 0,
      "targets": ["spot-1"],
      "ring": "inner",
      "effect": "rotation",
      "params": {
        "rotation": {
          "activeColor": [255, 0, 0],
          "inactiveColor": [0, 0, 0],
          "speed": 100,
          "pattern": "single"
        }
      }
    }
  ]
}
```

#### 8. Sequenzen auflisten

```http
GET /api/sequence/list
```

#### 9. Sequenz abspielen

```http
POST /api/sequence/play
Content-Type: application/json

{
  "sequenceId": "my-show"
}
```

#### 10. Sequenz pausieren

```http
POST /api/sequence/pause
```

#### 11. Sequenz stoppen

```http
POST /api/sequence/stop
```

## 🎬 Sequenz-Beispiele

### Simple Rotation

```json
{
  "id": "simple-rotation",
  "name": "Einfache Rotation",
  "duration": 10000,
  "loop": true,
  "events": [
    {
      "timestamp": 0,
      "targets": ["spot-1", "spot-2"],
      "ring": "inner",
      "effect": "rotation",
      "params": {
        "rotation": {
          "activeColor": [255, 255, 255],
          "inactiveColor": [0, 0, 0],
          "speed": 100,
          "pattern": "single"
        }
      }
    }
  ]
}
```

### Wave Effect über 4 Scheinwerfer

```json
{
  "id": "wave-show",
  "name": "Wellen-Effekt",
  "duration": 20000,
  "loop": true,
  "events": [
    {
      "timestamp": 0,
      "targets": ["spot-1"],
      "ring": "inner",
      "effect": "rotation",
      "params": {
        "rotation": {
          "activeColor": [255, 0, 0],
          "inactiveColor": [50, 0, 0],
          "speed": 100,
          "pattern": "trail",
          "trailLength": 3,
          "startOffset": 0
        }
      }
    },
    {
      "timestamp": 0,
      "targets": ["spot-2"],
      "ring": "inner",
      "effect": "rotation",
      "params": {
        "rotation": {
          "activeColor": [255, 0, 0],
          "inactiveColor": [50, 0, 0],
          "speed": 100,
          "pattern": "trail",
          "trailLength": 3,
          "startOffset": 2
        }
      }
    },
    {
      "timestamp": 0,
      "targets": ["spot-3"],
      "ring": "inner",
      "effect": "rotation",
      "params": {
        "rotation": {
          "activeColor": [255, 0, 0],
          "inactiveColor": [50, 0, 0],
          "speed": 100,
          "pattern": "trail",
          "trailLength": 3,
          "startOffset": 4
        }
      }
    },
    {
      "timestamp": 0,
      "targets": ["spot-4"],
      "ring": "inner",
      "effect": "rotation",
      "params": {
        "rotation": {
          "activeColor": [255, 0, 0],
          "inactiveColor": [50, 0, 0],
          "speed": 100,
          "pattern": "trail",
          "trailLength": 3,
          "startOffset": 6
        }
      }
    }
  ]
}
```

### Pink Floyd "Pulse" Style

```json
{
  "id": "pulse-style",
  "name": "Pink Floyd Pulse",
  "duration": 30000,
  "loop": true,
  "events": [
    {
      "timestamp": 0,
      "targets": ["spot-1", "spot-2", "spot-3", "spot-4"],
      "ring": "inner",
      "effect": "rotation",
      "params": {
        "rotation": {
          "activeColor": [255, 255, 255],
          "inactiveColor": [0, 0, 0],
          "speed": 50,
          "pattern": "single"
        }
      }
    },
    {
      "timestamp": 5000,
      "targets": ["spot-1", "spot-2", "spot-3", "spot-4"],
      "ring": "outer",
      "effect": "pulse",
      "params": {
        "color": [255, 0, 100],
        "brightness": 200,
        "duration": 3000
      }
    },
    {
      "timestamp": 15000,
      "targets": ["spot-1", "spot-3"],
      "ring": "both",
      "effect": "strobe",
      "params": {
        "color": [255, 255, 255],
        "frequency": 20,
        "duration": 2000
      }
    },
    {
      "timestamp": 20000,
      "targets": ["spot-1", "spot-2", "spot-3", "spot-4"],
      "ring": "outer",
      "effect": "rainbow",
      "params": {
        "brightness": 180,
        "duration": 5000
      }
    }
  ]
}
```

## 🎵 Spotify-Integration (Vorbereitet)

Die Struktur ist bereits vorbereitet für Spotify-Integration:

```cpp
// In Zukunft:
commander.connectSpotify(clientId, clientSecret);
commander.playSpotifyTrack("spotify:track:xxxxx");
```

**Sequenz mit Spotify:**
```json
{
  "id": "comfortably-numb",
  "name": "Comfortably Numb Show",
  "spotifyUri": "spotify:track:4EWCNWgDS8707fNSZ1oaA5",
  "syncWithSpotify": true,
  "events": [...]
}
```

## 🔧 Troubleshooting

### Scheinwerfer werden nicht gefunden

1. Prüfe, ob alle Geräte im selben Netzwerk sind
2. Teste WLED-Scheinwerfer direkt: `http://<scheinwerfer-ip>`
3. Füge Scheinwerfer manuell über API hinzu

### Rotation funktioniert nicht richtig

- WLED-Version prüfen (mindestens 0.13.0)
- Segment-Konfiguration in WLED überprüfen
- Für beste Ergebnisse: Verwende UDP-Modus (zukünftiges Feature)

### WiFi-Verbindung schlägt fehl

- AP-Modus aktivieren: `const bool AP_MODE = true;`
- Passwort muss mindestens 8 Zeichen haben
- ESP32 neustarten nach Änderungen

## 📝 TODO / Zukünftige Features

- [ ] Spotify Web API Integration
- [ ] mDNS Discovery für automatisches Finden von Scheinwerfern
- [ ] UDP-basierte LED-Steuerung für präzisere Rotation
- [ ] Web-Interface für Konfiguration
- [ ] SPIFFS-Persistenz für Sequenzen
- [ ] Audio-reaktive Effekte (Mikrofon-Input)
- [ ] DMX512-Unterstützung

## 📄 Lizenz

MIT License - Nutze es wie du willst!

## 🎸 Inspiration

Inspiriert von Pink Floyds legendärer "Pulse" Tour - eine der spektakulärsten Light-Shows der Rockgeschichte.

---

**Happy Lighting! 💡✨**
