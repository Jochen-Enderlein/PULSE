# Spotify Integration Guide

Diese Datei beschreibt, wie die Spotify-Integration später hinzugefügt werden kann.

## 🎵 Architektur-Überblick

Das System ist bereits vorbereitet für Spotify-Integration. Die notwendigen Strukturen und Platzhalter sind vorhanden.

### Vorbereitete Elemente

1. **Sequenz-Struktur**
   ```cpp
   struct Sequence {
       String spotifyUri;        // ✅ Bereits vorhanden
       bool syncWithSpotify;     // ✅ Bereits vorhanden
   }
   ```

2. **Playback-State**
   ```cpp
   struct PlaybackState {
       bool spotifyConnected;    // ✅ Bereits vorhanden
   }
   ```

3. **Platzhalter-Methoden**
   ```cpp
   bool connectSpotify(const String& clientId, const String& clientSecret);
   bool playSpotifyTrack(const String& uri);
   unsigned long getSpotifyPosition();
   ```

## 📋 Implementierungs-Schritte

### Schritt 1: Spotify Developer Account

1. Gehe zu: https://developer.spotify.com/dashboard
2. Erstelle eine neue App
3. Notiere:
   - Client ID
   - Client Secret
   - Redirect URI (z.B. `http://192.168.1.100/callback`)

### Schritt 2: OAuth2 Flow implementieren

```cpp
bool LightCommander::connectSpotify(const String& clientId, const String& clientSecret) {
    // 1. Authorization Code Flow starten
    String authUrl = "https://accounts.spotify.com/authorize?";
    authUrl += "client_id=" + clientId;
    authUrl += "&response_type=code";
    authUrl += "&redirect_uri=http://" + WiFi.localIP().toString() + "/callback";
    authUrl += "&scope=user-modify-playback-state user-read-playback-state";
    
    Serial.println("Visit this URL to authorize:");
    Serial.println(authUrl);
    
    // 2. Warte auf Callback mit Authorization Code
    // (Implementierung eines temporären Webservers für Callback)
    
    // 3. Exchange Code für Access Token
    HTTPClient http;
    http.begin("https://accounts.spotify.com/api/token");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    String auth = clientId + ":" + clientSecret;
    String authBase64 = base64::encode(auth);
    http.addHeader("Authorization", "Basic " + authBase64);
    
    String body = "grant_type=authorization_code";
    body += "&code=" + authorizationCode;
    body += "&redirect_uri=http://" + WiFi.localIP().toString() + "/callback";
    
    int httpCode = http.POST(body);
    
    if (httpCode == 200) {
        String response = http.getString();
        
        // Parse JSON Response
        StaticJsonDocument<1024> doc;
        deserializeJson(doc, response);
        
        spotifyAccessToken = doc["access_token"].as<String>();
        String refreshToken = doc["refresh_token"].as<String>();
        int expiresIn = doc["expires_in"];
        
        spotifyConnected = true;
        playback.spotifyConnected = true;
        
        Serial.println("✓ Spotify connected!");
        return true;
    }
    
    http.end();
    return false;
}
```

### Schritt 3: Track-Wiedergabe

```cpp
bool LightCommander::playSpotifyTrack(const String& uri) {
    if (!spotifyConnected || spotifyAccessToken.isEmpty()) {
        Serial.println("Not connected to Spotify");
        return false;
    }
    
    HTTPClient http;
    http.begin("https://api.spotify.com/v1/me/player/play");
    http.addHeader("Authorization", "Bearer " + spotifyAccessToken);
    http.addHeader("Content-Type", "application/json");
    
    String body = "{\"uris\":[\"" + uri + "\"]}";
    
    int httpCode = http.PUT(body);
    bool success = (httpCode == 204 || httpCode == 202);
    
    if (success) {
        Serial.println("✓ Spotify playback started");
    } else {
        Serial.printf("✗ Spotify error: %d\n", httpCode);
    }
    
    http.end();
    return success;
}
```

### Schritt 4: Position auslesen

```cpp
unsigned long LightCommander::getSpotifyPosition() {
    if (!spotifyConnected) return 0;
    
    HTTPClient http;
    http.begin("https://api.spotify.com/v1/me/player");
    http.addHeader("Authorization", "Bearer " + spotifyAccessToken);
    
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        String response = http.getString();
        
        StaticJsonDocument<2048> doc;
        deserializeJson(doc, response);
        
        unsigned long position = doc["progress_ms"];
        bool isPlaying = doc["is_playing"];
        
        http.end();
        return position;
    }
    
    http.end();
    return 0;
}
```

### Schritt 5: Synchronisation in playSequence()

```cpp
bool LightCommander::playSequence(const String& sequenceId) {
    currentSequence = getSequence(sequenceId);
    if (!currentSequence) return false;
    
    playback.active = true;
    playback.currentSequence = sequenceId;
    playback.startTime = millis();
    playback.position = 0;
    playback.paused = false;
    currentEventIndex = 0;
    
    // ⭐ SPOTIFY INTEGRATION
    if (currentSequence->syncWithSpotify && !currentSequence->spotifyUri.isEmpty()) {
        if (spotifyConnected) {
            // Spotify Track starten
            if (playSpotifyTrack(currentSequence->spotifyUri)) {
                Serial.println("✓ Sequence synced with Spotify");
                
                // Optional: Kleine Verzögerung für Spotify-Start
                delay(100);
            } else {
                Serial.println("⚠ Failed to start Spotify, playing without sync");
            }
        } else {
            Serial.println("⚠ Spotify not connected, playing without sync");
        }
    }
    
    return true;
}
```

### Schritt 6: Sync-Loop für laufende Sequenzen

```cpp
void LightCommander::updateSequencePlayback() {
    if (!currentSequence) return;
    
    unsigned long currentTime;
    
    // ⭐ SPOTIFY SYNC
    if (currentSequence->syncWithSpotify && spotifyConnected) {
        // Verwende Spotify-Position statt lokaler Zeit
        currentTime = getSpotifyPosition();
        
        // Nur alle 2 Sekunden abfragen (Rate Limiting)
        static unsigned long lastSpotifyCheck = 0;
        if (millis() - lastSpotifyCheck > 2000) {
            currentTime = getSpotifyPosition();
            lastSpotifyCheck = millis();
            
            // Position anpassen falls Drift erkannt
            playback.position = currentTime;
            playback.startTime = millis() - currentTime;
        }
    } else {
        // Normale Zeit-basierte Wiedergabe
        currentTime = millis() - playback.startTime;
    }
    
    // Rest der Methode bleibt gleich...
    while (currentEventIndex < currentSequence->events.size()) {
        const SequenceEvent& event = currentSequence->events[currentEventIndex];
        
        if (currentTime >= event.timestamp) {
            processSequenceEvent(event);
            currentEventIndex++;
        } else {
            break;
        }
    }
}
```

## 🔐 Token-Refresh

Spotify Access Tokens laufen nach 1 Stunde ab. Implementiere einen Refresh-Mechanismus:

```cpp
bool LightCommander::refreshSpotifyToken() {
    if (spotifyRefreshToken.isEmpty()) return false;
    
    HTTPClient http;
    http.begin("https://accounts.spotify.com/api/token");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    String auth = spotifyClientId + ":" + spotifyClientSecret;
    String authBase64 = base64::encode(auth);
    http.addHeader("Authorization", "Basic " + authBase64);
    
    String body = "grant_type=refresh_token";
    body += "&refresh_token=" + spotifyRefreshToken;
    
    int httpCode = http.POST(body);
    
    if (httpCode == 200) {
        String response = http.getString();
        StaticJsonDocument<512> doc;
        deserializeJson(doc, response);
        
        spotifyAccessToken = doc["access_token"].as<String>();
        
        Serial.println("✓ Spotify token refreshed");
        return true;
    }
    
    return false;
}
```

Rufe diese Methode automatisch auf bei HTTP 401 Fehlern.

## 📱 App-Integration

Die App muss:

1. **Spotify Authorization Flow starten**
   ```javascript
   // In der App
   fetch('http://commander-ip/api/spotify/authorize', {
       method: 'POST',
       body: JSON.stringify({
           clientId: 'YOUR_CLIENT_ID',
           clientSecret: 'YOUR_CLIENT_SECRET'
       })
   });
   ```

2. **Sequenzen mit Spotify-URI erstellen**
   ```javascript
   const sequence = {
       id: 'comfortably-numb',
       name: 'Comfortably Numb Show',
       spotifyUri: 'spotify:track:4EWCNWgDS8707fNSZ1oaA5',
       syncWithSpotify: true,
       events: [...]
   };
   ```

## 🎛️ API-Erweiterungen

Füge neue Endpoints hinzu:

```cpp
// In setupRoutes()
server.on("/api/spotify/authorize", HTTP_POST, [this]() { 
    handleSpotifyAuthorize(); 
});

server.on("/api/spotify/status", HTTP_GET, [this]() { 
    handleSpotifyStatus(); 
});

server.on("/api/spotify/callback", HTTP_GET, [this]() { 
    handleSpotifyCallback(); 
});
```

## 🔍 Testing

```bash
# 1. Teste Authorization
curl -X POST http://commander-ip/api/spotify/authorize \
  -H "Content-Type: application/json" \
  -d '{"clientId":"xxx","clientSecret":"yyy"}'

# 2. Spiele Track ab
curl -X POST http://commander-ip/api/spotify/play \
  -H "Content-Type: application/json" \
  -d '{"uri":"spotify:track:xxxxx"}'

# 3. Prüfe Status
curl http://commander-ip/api/spotify/status
```

## 📚 Benötigte Libraries

Zusätzlich installieren:

```ini
; In platformio.ini
lib_deps = 
    bblanchon/ArduinoJson@^6.21.3
    ESP Async WebServer@^1.2.3
    rweather/Crypto@^0.2.0        ; Für Base64
```

## ⚡ Performance-Tipps

1. **Rate Limiting**: Spotify API max 2 Anfragen/Sekunde
2. **Caching**: Speichere Track-Infos lokal
3. **Offline-Mode**: Sequenzen sollten auch ohne Spotify funktionieren
4. **Error Handling**: Graceful Fallback bei Verbindungsproblemen

## 🎯 Nächste Schritte

1. ✅ OAuth2 Flow implementieren
2. ✅ Token Management (Refresh)
3. ✅ Playback-Steuerung
4. ✅ Position-Polling
5. ✅ Sync-Algorithmus
6. ⬜ Web-UI für Spotify-Login
7. ⬜ Token-Persistenz in SPIFFS
8. ⬜ Multi-User Support

## 🔗 Weitere Ressourcen

- [Spotify Web API Docs](https://developer.spotify.com/documentation/web-api)
- [OAuth2 Guide](https://developer.spotify.com/documentation/general/guides/authorization-guide/)
- [ESP32 HTTPS Examples](https://github.com/espressif/arduino-esp32/tree/master/libraries/HTTPClient/examples)

---

**Status**: 🟡 Vorbereitet - Bereit für Implementation
