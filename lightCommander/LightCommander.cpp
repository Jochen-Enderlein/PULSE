#include "LightCommander.h"

// ============================================================================
// KONSTRUKTOR & INITIALISIERUNG
// ============================================================================

LightCommander::LightCommander() : 
    server(80),
    isAPMode(false),
    currentSequence(nullptr),
    currentEventIndex(0) {
}

void LightCommander::begin(const char* ssid, const char* password, bool apMode) {
    Serial.begin(115200);
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║     LIGHT COMMANDER                     ║");
    Serial.println("║     Master-Controller für Scheinwerfer ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    wifiSSID = String(ssid);
    wifiPassword = String(password);
    isAPMode = apMode;
    
    // WiFi Setup
    if (isAPMode) {
        Serial.println("📡 Starting Access Point Mode...");
        WiFi.softAP(ssid, password);
        Serial.print("AP SSID: ");
        Serial.println(ssid);
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
    } else {
        Serial.println("📡 Connecting to WiFi...");
        Serial.print("SSID: ");
        Serial.println(ssid);
        
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 40) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n✓ WiFi connected!");
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());
            Serial.print("Signal: ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");
        } else {
            Serial.println("\n✗ WiFi connection failed!");
            Serial.println("Starting AP Mode as fallback...");
            WiFi.softAP(ssid, password);
            isAPMode = true;
            Serial.print("AP IP: ");
            Serial.println(WiFi.softAPIP());
        }
    }
    
    // REST API Setup
    setupRoutes();
    server.begin();
    
    Serial.println("\n✓ Light Commander ready!");
    Serial.println("Waiting for spotlight connections...\n");
}

void LightCommander::loop() {
    server.handleClient();
    
    // Sequenz-Playback
    if (playback.active && !playback.paused) {
        updateSequencePlayback();
    }
    
    // Periodischer Health-Check (alle 30 Sekunden)
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 30000) {
        checkSpotlightStatus();
        lastCheck = millis();
    }
}

// ============================================================================
// REST API ROUTES
// ============================================================================

void LightCommander::setupRoutes() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    
    server.on("/api/spotlight/add", HTTP_POST, [this]() { handleAddSpotlight(); });
    server.on("/api/spotlight/list", HTTP_GET, [this]() { handleListSpotlights(); });
    
    server.on("/api/effect/send", HTTP_POST, [this]() { handleSendEffect(); });
    server.on("/api/effect/stop", HTTP_POST, [this]() { handleStopEffect(); });
    
    server.on("/api/sequence/load", HTTP_POST, [this]() { handleLoadSequence(); });
    server.on("/api/sequence/list", HTTP_GET, [this]() { handleListSequences(); });
    server.on("/api/sequence/play", HTTP_POST, [this]() { handlePlaySequence(); });
    server.on("/api/sequence/pause", HTTP_POST, [this]() { handlePauseSequence(); });
    server.on("/api/sequence/resume", HTTP_POST, [this]() { handleResumeSequence(); });
    server.on("/api/sequence/stop", HTTP_POST, [this]() { handleStopSequence(); });
}

void LightCommander::handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>Light Commander</title>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<style>body{font-family:Arial;margin:20px;background:#1a1a1a;color:#fff;}";
    html += "h1{color:#00aaff;}ul{line-height:1.8;}</style></head><body>";
    html += "<h1>🎆 Light Commander</h1>";
    html += "<h2>Connected Spotlights:</h2><ul>";
    
    for (auto& pair : spotlights) {
        html += "<li>" + pair.second.name + " (" + pair.second.id + ") - ";
        html += pair.second.online ? "🟢 Online" : "🔴 Offline";
        html += " @ " + pair.second.ip + "</li>";
    }
    if (spotlights.empty()) {
        html += "<li>No spotlights connected</li>";
    }
    
    html += "</ul><h2>Loaded Sequences:</h2><ul>";
    for (auto& pair : sequences) {
        html += "<li>" + pair.second.name + " (" + pair.second.id + ") - ";
        html += String(pair.second.events.size()) + " events, ";
        html += String(pair.second.duration / 1000) + "s</li>";
    }
    if (sequences.empty()) {
        html += "<li>No sequences loaded</li>";
    }
    
    html += "</ul><h2>Playback Status:</h2><p>";
    if (playback.active) {
        html += "▶️ Playing: " + playback.currentSequence;
        if (playback.paused) html += " (PAUSED)";
    } else {
        html += "⏹️ Stopped";
    }
    
    html += "</p><h2>API Endpoints:</h2><ul>";
    html += "<li>POST /api/spotlight/add - Add spotlight</li>";
    html += "<li>GET /api/spotlight/list - List spotlights</li>";
    html += "<li>POST /api/effect/send - Send effect</li>";
    html += "<li>POST /api/sequence/load - Load sequence</li>";
    html += "<li>POST /api/sequence/play - Play sequence</li>";
    html += "<li>GET /api/status - Get status</li>";
    html += "</ul></body></html>";
    
    server.send(200, "text/html", html);
}

void LightCommander::handleStatus() {
    server.send(200, "application/json", getStatusJson());
}

void LightCommander::handleAddSpotlight() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, server.arg("plain"))) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    String id = doc["id"] | "";
    String name = doc["name"] | "";
    String ip = doc["ip"] | "";
    
    if (addSpotlight(id, name, ip)) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to add spotlight\"}");
    }
}

void LightCommander::handleListSpotlights() {
    StaticJsonDocument<2048> doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (auto& pair : spotlights) {
        JsonObject obj = array.createNestedObject();
        obj["id"] = pair.second.id;
        obj["name"] = pair.second.name;
        obj["ip"] = pair.second.ip;
        obj["online"] = pair.second.online;
    }
    
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void LightCommander::handleSendEffect() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    
    String body = server.arg("plain");
    Serial.println("\n📥 Received effect command:");
    Serial.println(body);
    
    StaticJsonDocument<2048> doc;
    if (deserializeJson(doc, body)) {
        Serial.println("✗ JSON parse error!");
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    // Targets parsen
    std::vector<String> targets;
    JsonArray targetsArray = doc["targets"];
    for (JsonVariant v : targetsArray) {
        targets.push_back(v.as<String>());
    }
    
    // Ring
    String ringStr = doc["ring"] | "both";
    RingType ring = RING_BOTH;
    if (ringStr == "inner") ring = RING_INNER;
    else if (ringStr == "outer") ring = RING_OUTER;
    
    // Effekt-Typ
    String effectStr = doc["effect"] | "static";
    EffectType effect = EFFECT_STATIC;
    if (effectStr == "fade") effect = EFFECT_FADE;
    else if (effectStr == "strobe") effect = EFFECT_STROBE;
    else if (effectStr == "pulse") effect = EFFECT_PULSE;
    else if (effectStr == "rotation") effect = EFFECT_ROTATION;
    else if (effectStr == "rainbow") effect = EFFECT_RAINBOW;
    else if (effectStr == "chase") effect = EFFECT_CHASE;
    
    // Parameter
    EffectParams params;
    if (doc.containsKey("color")) {
        JsonArray c = doc["color"];
        params.color = Color(c[0], c[1], c[2]);
    }
    if (doc.containsKey("color2")) {
        JsonArray c2 = doc["color2"];
        params.color2 = Color(c2[0], c2[1], c2[2]);
    }
    if (doc.containsKey("brightness")) params.brightness = doc["brightness"];
    if (doc.containsKey("speed")) params.speed = doc["speed"];
    if (doc.containsKey("duration")) params.duration = doc["duration"];
    
    // Rotation params
    if (effect == EFFECT_ROTATION && doc.containsKey("rotation")) {
        JsonObject rot = doc["rotation"];
        if (rot.containsKey("activeColor")) {
            JsonArray ac = rot["activeColor"];
            params.rotation.activeColor = Color(ac[0], ac[1], ac[2]);
        }
        if (rot.containsKey("inactiveColor")) {
            JsonArray ic = rot["inactiveColor"];
            params.rotation.inactiveColor = Color(ic[0], ic[1], ic[2]);
        }
        if (rot.containsKey("speed")) params.rotation.speed = rot["speed"];
        if (rot.containsKey("direction")) {
            String dir = rot["direction"] | "clockwise";
            params.rotation.direction = (dir == "counterclockwise") ? 
                DIRECTION_COUNTERCLOCKWISE : DIRECTION_CLOCKWISE;
        }
        if (rot.containsKey("pattern")) {
            String pat = rot["pattern"] | "single";
            if (pat == "trail") params.rotation.pattern = PATTERN_TRAIL;
            else if (pat == "opposite") params.rotation.pattern = PATTERN_OPPOSITE;
            else if (pat == "wave") params.rotation.pattern = PATTERN_WAVE;
            else params.rotation.pattern = PATTERN_SINGLE;
        }
        if (rot.containsKey("trailLength")) {
            params.rotation.trailLength = rot["trailLength"];
        }
    }
    
    // Senden!
    if (sendEffect(targets, ring, effect, params)) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to send effect\"}");
    }
}

void LightCommander::handleStopEffect() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, server.arg("plain"))) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    std::vector<String> targets;
    JsonArray targetsArray = doc["targets"];
    for (JsonVariant v : targetsArray) {
        targets.push_back(v.as<String>());
    }
    
    String ringStr = doc["ring"] | "both";
    RingType ring = RING_BOTH;
    if (ringStr == "inner") ring = RING_INNER;
    else if (ringStr == "outer") ring = RING_OUTER;
    
    if (stopEffect(targets, ring)) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to stop\"}");
    }
}

void LightCommander::handleLoadSequence() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    
    if (loadSequence(server.arg("plain"))) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to load sequence\"}");
    }
}

void LightCommander::handleListSequences() {
    std::vector<String> seqList = listSequences();
    
    StaticJsonDocument<2048> doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (const String& id : seqList) {
        Sequence* seq = getSequence(id);
        if (seq) {
            JsonObject obj = array.createNestedObject();
            obj["id"] = seq->id;
            obj["name"] = seq->name;
            obj["duration"] = seq->duration;
            obj["eventCount"] = seq->events.size();
        }
    }
    
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void LightCommander::handlePlaySequence() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, server.arg("plain"))) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    String seqId = doc["sequenceId"] | "";
    
    if (playSequence(seqId)) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Failed to play sequence\"}");
    }
}

void LightCommander::handlePauseSequence() {
    if (pauseSequence()) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Not playing\"}");
    }
}

void LightCommander::handleResumeSequence() {
    if (resumeSequence()) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Cannot resume\"}");
    }
}

void LightCommander::handleStopSequence() {
    if (stopSequence()) {
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Not playing\"}");
    }
}

// ============================================================================
// SCHEINWERFER-MANAGEMENT
// ============================================================================

bool LightCommander::addSpotlight(const String& id, const String& name, const String& ip) {
    Spotlight spot;
    spot.id = id;
    spot.name = name;
    spot.ip = ip;
    spot.online = false;
    
    spotlights[id] = spot;
    
    Serial.printf("Added spotlight: %s (%s) at %s\n", id.c_str(), name.c_str(), ip.c_str());
    
    // Sofort checken ob online
    checkSpotlightStatus();
    
    return true;
}

bool LightCommander::removeSpotlight(const String& id) {
    return spotlights.erase(id) > 0;
}

Spotlight* LightCommander::getSpotlight(const String& id) {
    auto it = spotlights.find(id);
    if (it != spotlights.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::vector<Spotlight*> LightCommander::getAllSpotlights() {
    std::vector<Spotlight*> result;
    for (auto& pair : spotlights) {
        result.push_back(&pair.second);
    }
    return result;
}

void LightCommander::checkSpotlightStatus() {
    HTTPClient http;
    
    for (auto& pair : spotlights) {
        Spotlight& spot = pair.second;
        
        String url = "http://" + spot.ip + "/status";
        http.begin(url);
        http.setTimeout(3000);
        
        int httpCode = http.GET();
        
        if (httpCode == 200) {
            spot.online = true;
            spot.lastSeen = millis();
        } else {
            spot.online = false;
        }
        
        http.end();
    }
}

// ============================================================================
// EFFEKT-STEUERUNG
// ============================================================================

bool LightCommander::sendEffect(const std::vector<String>& targets, RingType ring,
                                 EffectType effect, const EffectParams& params) {
    bool allSuccess = true;
    
    for (const String& targetId : targets) {
        Spotlight* spot = getSpotlight(targetId);
        if (!spot) {
            Serial.printf("✗ Spotlight '%s' not found\n", targetId.c_str());
            allSuccess = false;
            continue;
        }
        
        Serial.printf("→ Sending to %s (%s)...\n", targetId.c_str(), spot->ip.c_str());
        
        String json = buildEffectJson(ring, effect, params);
        
        if (!sendToSpotlight(spot->ip, json)) {
            Serial.printf("✗ Failed to send to %s\n", targetId.c_str());
            allSuccess = false;
        } else {
            Serial.printf("✓ Sent to %s\n", targetId.c_str());
        }
    }
    
    return allSuccess;
}

bool LightCommander::stopEffect(const std::vector<String>& targets, RingType ring) {
    bool allSuccess = true;
    
    for (const String& targetId : targets) {
        Spotlight* spot = getSpotlight(targetId);
        if (!spot) continue;
        
        HTTPClient http;
        String url = "http://" + spot->ip + "/stop";
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(5000);
        
        // Ring als JSON senden
        StaticJsonDocument<128> doc;
        if (ring == RING_INNER) doc["ring"] = "inner";
        else if (ring == RING_OUTER) doc["ring"] = "outer";
        else doc["ring"] = "both";
        
        String json;
        serializeJson(doc, json);
        
        int httpCode = http.POST(json);
        if (httpCode != 200) {
            allSuccess = false;
        }
        
        http.end();
    }
    
    return allSuccess;
}

bool LightCommander::sendToSpotlight(const String& ip, const String& json) {
    HTTPClient http;
    String url = "http://" + ip + "/effect";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);
    
    int httpCode = http.POST(json);
    bool success = (httpCode == 200);
    
    if (!success) {
        Serial.printf("HTTP Error: %d (%s)\n", httpCode, http.errorToString(httpCode).c_str());
    }
    
    http.end();
    return success;
}

String LightCommander::buildEffectJson(RingType ring, EffectType effect, const EffectParams& params) {
    StaticJsonDocument<2048> doc;
    
    // Ring
    if (ring == RING_INNER) doc["ring"] = "inner";
    else if (ring == RING_OUTER) doc["ring"] = "outer";
    else doc["ring"] = "both";
    
    // Effekt
    if (effect == EFFECT_STATIC) doc["effect"] = "static";
    else if (effect == EFFECT_FADE) doc["effect"] = "fade";
    else if (effect == EFFECT_STROBE) doc["effect"] = "strobe";
    else if (effect == EFFECT_PULSE) doc["effect"] = "pulse";
    else if (effect == EFFECT_ROTATION) doc["effect"] = "rotation";
    else if (effect == EFFECT_RAINBOW) doc["effect"] = "rainbow";
    else if (effect == EFFECT_CHASE) doc["effect"] = "chase";
    
    // Farbe
    JsonArray color = doc.createNestedArray("color");
    color.add(params.color.r);
    color.add(params.color.g);
    color.add(params.color.b);
    
    // Brightness
    doc["brightness"] = params.brightness;
    
    // Speed/Duration
    if (params.speed > 0) doc["speed"] = params.speed;
    if (params.duration > 0) doc["duration"] = params.duration;
    
    // Rotation
    if (effect == EFFECT_ROTATION) {
        JsonObject rot = doc.createNestedObject("rotation");
        
        JsonArray activeColor = rot.createNestedArray("activeColor");
        activeColor.add(params.rotation.activeColor.r);
        activeColor.add(params.rotation.activeColor.g);
        activeColor.add(params.rotation.activeColor.b);
        
        JsonArray inactiveColor = rot.createNestedArray("inactiveColor");
        inactiveColor.add(params.rotation.inactiveColor.r);
        inactiveColor.add(params.rotation.inactiveColor.g);
        inactiveColor.add(params.rotation.inactiveColor.b);
        
        rot["speed"] = params.rotation.speed;
        rot["direction"] = (params.rotation.direction == DIRECTION_CLOCKWISE) ? 
            "clockwise" : "counterclockwise";
        
        if (params.rotation.pattern == PATTERN_SINGLE) rot["pattern"] = "single";
        else if (params.rotation.pattern == PATTERN_TRAIL) {
            rot["pattern"] = "trail";
            rot["trailLength"] = params.rotation.trailLength;
        }
        else if (params.rotation.pattern == PATTERN_OPPOSITE) rot["pattern"] = "opposite";
        else if (params.rotation.pattern == PATTERN_WAVE) rot["pattern"] = "wave";
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

// ============================================================================
// SEQUENZ-MANAGEMENT
// ============================================================================

bool LightCommander::loadSequence(const String& json) {
    DynamicJsonDocument doc(16384);
    
    if (deserializeJson(doc, json)) {
        Serial.println("✗ Failed to parse sequence JSON");
        return false;
    }
    
    Sequence seq;
    seq.id = doc["id"] | "";
    seq.name = doc["name"] | "";
    seq.duration = doc["duration"] | 0;
    seq.loop = doc["loop"] | false;
    seq.spotifyUri = doc["spotifyUri"] | "";
    seq.syncWithSpotify = doc["syncWithSpotify"] | false;
    
    // Events parsen
    JsonArray eventsArray = doc["events"];
    for (JsonObject eventObj : eventsArray) {
        SequenceEvent event;
        event.timestamp = eventObj["timestamp"] | 0;
        
        // Targets
        JsonArray targetsArray = eventObj["targets"];
        for (JsonVariant v : targetsArray) {
            event.targets.push_back(v.as<String>());
        }
        
        // Ring
        String ringStr = eventObj["ring"] | "both";
        if (ringStr == "inner") event.ring = RING_INNER;
        else if (ringStr == "outer") event.ring = RING_OUTER;
        else event.ring = RING_BOTH;
        
        // Effect
        String effectStr = eventObj["effect"] | "static";
        if (effectStr == "fade") event.effect = EFFECT_FADE;
        else if (effectStr == "strobe") event.effect = EFFECT_STROBE;
        else if (effectStr == "pulse") event.effect = EFFECT_PULSE;
        else if (effectStr == "rotation") event.effect = EFFECT_ROTATION;
        else if (effectStr == "rainbow") event.effect = EFFECT_RAINBOW;
        else if (effectStr == "chase") event.effect = EFFECT_CHASE;
        else event.effect = EFFECT_STATIC;
        
        // Params
        JsonObject params = eventObj["params"];
        if (params.containsKey("color")) {
            JsonArray c = params["color"];
            event.params.color = Color(c[0], c[1], c[2]);
        }
        if (params.containsKey("brightness")) {
            event.params.brightness = params["brightness"];
        }
        if (params.containsKey("duration")) {
            event.params.duration = params["duration"];
        }
        if (params.containsKey("speed")) {
            event.params.speed = params["speed"];
        }
        
        // Rotation params
        if (params.containsKey("rotation")) {
            JsonObject rot = params["rotation"];
            if (rot.containsKey("activeColor")) {
                JsonArray ac = rot["activeColor"];
                event.params.rotation.activeColor = Color(ac[0], ac[1], ac[2]);
            }
            if (rot.containsKey("inactiveColor")) {
                JsonArray ic = rot["inactiveColor"];
                event.params.rotation.inactiveColor = Color(ic[0], ic[1], ic[2]);
            }
            if (rot.containsKey("speed")) {
                event.params.rotation.speed = rot["speed"];
            }
            if (rot.containsKey("direction")) {
                String dir = rot["direction"] | "clockwise";
                event.params.rotation.direction = (dir == "counterclockwise") ? 
                    DIRECTION_COUNTERCLOCKWISE : DIRECTION_CLOCKWISE;
            }
            if (rot.containsKey("pattern")) {
                String pat = rot["pattern"] | "single";
                if (pat == "trail") event.params.rotation.pattern = PATTERN_TRAIL;
                else if (pat == "opposite") event.params.rotation.pattern = PATTERN_OPPOSITE;
                else if (pat == "wave") event.params.rotation.pattern = PATTERN_WAVE;
                else event.params.rotation.pattern = PATTERN_SINGLE;
            }
            if (rot.containsKey("trailLength")) {
                event.params.rotation.trailLength = rot["trailLength"];
            }
        }
        
        seq.events.push_back(event);
    }
    
    sequences[seq.id] = seq;
    
    Serial.printf("✓ Loaded sequence '%s' with %d events\n", seq.name.c_str(), seq.events.size());
    return true;
}

Sequence* LightCommander::getSequence(const String& id) {
    auto it = sequences.find(id);
    if (it != sequences.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::vector<String> LightCommander::listSequences() {
    std::vector<String> result;
    for (auto& pair : sequences) {
        result.push_back(pair.first);
    }
    return result;
}

// ============================================================================
// SEQUENZ-PLAYBACK
// ============================================================================

bool LightCommander::playSequence(const String& sequenceId) {
    Sequence* seq = getSequence(sequenceId);
    if (!seq) {
        Serial.printf("✗ Sequence '%s' not found\n", sequenceId.c_str());
        return false;
    }
    
    playback.active = true;
    playback.currentSequence = sequenceId;
    playback.startTime = millis();
    playback.paused = false;
    currentSequence = seq;
    currentEventIndex = 0;
    
    Serial.printf("▶️ Playing sequence '%s'\n", seq->name.c_str());
    return true;
}

bool LightCommander::pauseSequence() {
    if (!playback.active || playback.paused) return false;
    
    playback.paused = true;
    playback.pauseTime = millis();
    
    Serial.println("⏸️ Sequence paused");
    return true;
}

bool LightCommander::resumeSequence() {
    if (!playback.active || !playback.paused) return false;
    
    // Zeit korrigieren
    unsigned long pauseDuration = millis() - playback.pauseTime;
    playback.startTime += pauseDuration;
    playback.paused = false;
    
    Serial.println("▶️ Sequence resumed");
    return true;
}

bool LightCommander::stopSequence() {
    if (!playback.active) return false;
    
    playback.active = false;
    playback.paused = false;
    currentSequence = nullptr;
    currentEventIndex = 0;
    
    Serial.println("⏹️ Sequence stopped");
    return true;
}

void LightCommander::updateSequencePlayback() {
    if (!currentSequence || !playback.active) return;
    
    unsigned long currentTime = millis() - playback.startTime;
    
    // Events abarbeiten
    while (currentEventIndex < currentSequence->events.size()) {
        SequenceEvent& event = currentSequence->events[currentEventIndex];
        
        if (currentTime >= event.timestamp) {
            processSequenceEvent(event);
            currentEventIndex++;
        } else {
            break;
        }
    }
    
    // Ende erreicht?
    if (currentEventIndex >= currentSequence->events.size()) {
        if (currentSequence->loop) {
            // Loop
            currentEventIndex = 0;
            playback.startTime = millis();
            Serial.println("🔄 Sequence looping...");
        } else {
            // Stoppen
            stopSequence();
            Serial.println("✓ Sequence complete");
        }
    }
}

void LightCommander::processSequenceEvent(const SequenceEvent& event) {
    Serial.printf("⚡ Event @ %lums\n", event.timestamp);
    sendEffect(event.targets, event.ring, event.effect, event.params);
}

// ============================================================================
// STATUS
// ============================================================================

String LightCommander::getStatusJson() {
    StaticJsonDocument<2048> doc;
    
    doc["uptime"] = millis();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);
    doc["ip"] = isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    
    // Playback
    JsonObject pb = doc.createNestedObject("playback");
    pb["active"] = playback.active;
    pb["sequence"] = playback.currentSequence;
    pb["paused"] = playback.paused;
    if (playback.active) {
        pb["position"] = millis() - playback.startTime;
    }
    
    // Devices
    JsonArray devices = doc.createNestedArray("devices");
    for (auto& pair : spotlights) {
        JsonObject dev = devices.createNestedObject();
        dev["id"] = pair.second.id;
        dev["name"] = pair.second.name;
        dev["ip"] = pair.second.ip;
        dev["online"] = pair.second.online;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}
