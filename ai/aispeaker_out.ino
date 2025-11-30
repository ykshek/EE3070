#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Audio.h"

// WiFi credentials
const char* ssid = "EE3070_P1615_1";
const char* password = "EE3070P1615";

// ThingSpeak configuration
const char* thingSpeakReadApiKey = "F0XQ7MYBDCBROOZ5";
const unsigned long thingSpeakChannelID = 3172003;

// Audio pins
#define I2S_DOUT 13
#define I2S_BCLK 12
#define I2S_LRC 11
#define LED_PIN 21

Audio audio;

// System state
unsigned long lastEntryId = 0;
unsigned long lastCheckTime = 0;
const unsigned long CHECK_INTERVAL = 3000;
bool isSpeaking = false;
unsigned long speechStartTime = 0;
const unsigned long SPEECH_TIMEOUT = 10000; // 10 second timeout

void connectToWiFi() {
  Serial.begin(115200);
  Serial.println("🔊 Speaker System Starting...");
  Serial.println("Connecting to WiFi: " + String(ssid));
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println("\n✅ WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void initializeAudio() {
  Serial.println("🎵 Initializing audio...");
  Serial.printf("I2S Pins - DOUT: %d, BCLK: %d, LRC: %d\n", I2S_DOUT, I2S_BCLK, I2S_LRC);
  
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(21); // MAX VOLUME
  
  Serial.println("✅ Audio system initialized");
}

String cleanThingSpeakResponse(String response) {
  Serial.println("🔧 Raw response: " + response);
  
  // Remove ALL special codes
  response.replace("P49", "");
  response.replace("p48", "");
  response.replace("W70", "");
  response.replace("W56", "");
  response.replace("V65", "");
  response.replace("Q70", "?");
  response.replace("P55", "'");
  response.replace("P67", ".");
  
  // Remove any remaining letter+number combinations
  String cleanText = "";
  for (int i = 0; i < response.length(); i++) {
    char c = response.charAt(i);
    
    // If it's a letter, check if it's followed by 2 digits
    if (isalpha(c) && i + 2 < response.length()) {
      char next1 = response.charAt(i + 1);
      char next2 = response.charAt(i + 2);
      
      if (isdigit(next1) && isdigit(next2)) {
        // Skip this letter and the two digits
        i += 2;
        continue;
      }
    }
    
    // Keep the character
    cleanText += c;
  }
  
  // Final cleanup
  cleanText.replace("  ", " ");
  cleanText.trim();
  
  Serial.println("✅ Cleaned: " + cleanText);
  return cleanText;
}

void checkThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected");
    return;
  }

  if (isSpeaking) {
    Serial.println("⏸️  Skipping check - currently speaking");
    return;
  }

  HTTPClient http;
  String url = "http://api.thingspeak.com/channels/" + String(thingSpeakChannelID) + "/feeds/last.json?api_key=" + String(thingSpeakReadApiKey);
  
  Serial.println("📡 Checking ThingSpeak...");
  http.begin(url);
  
  int httpCode = http.GET();
  Serial.println("📊 HTTP Code: " + String(httpCode));
  
  if (httpCode == 200) {
    String response = http.getString();
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      if (doc.containsKey("entry_id") && doc.containsKey("field2")) {
        unsigned long currentEntryId = doc["entry_id"];
        const char* aiResponse = doc["field2"];
        
        Serial.printf("📊 Entry IDs - Last: %lu, Current: %lu\n", lastEntryId, currentEntryId);
        
        // ONLY CHECK ENTRY ID
        if (currentEntryId > lastEntryId) {
          Serial.println("🎯 NEW ENTRY DETECTED!");
          lastEntryId = currentEntryId;
          
          if (aiResponse && strlen(aiResponse) > 0) {
            String cleanResponse = cleanThingSpeakResponse(String(aiResponse));
            
            if (cleanResponse.length() > 3) { // Only speak if we have meaningful text
              Serial.println("💬 Speaking: " + cleanResponse);
              
              // Speak the new message
              speakText(cleanResponse);
            } else {
              Serial.println("⚠️ Cleaned response too short: " + cleanResponse);
            }
          } else {
            Serial.println("⚠️ Empty AI response in new entry");
          }
        } else {
          Serial.println("ℹ️ No new entries");
        }
      } else {
        Serial.println("❌ Missing required fields in response");
      }
    } else {
      Serial.println("❌ JSON parsing error: " + String(error.c_str()));
    }
  } else {
    Serial.println("❌ ThingSpeak HTTP error: " + String(httpCode));
  }
  
  http.end();
}

void speakText(String text) {
  if (text.length() == 0) {
    Serial.println("❌ Cannot speak - no text");
    return;
  }
  
  if (isSpeaking) {
    Serial.println("❌ Already speaking, cannot start new speech");
    return;
  }
  
  Serial.println("🔊 STARTING SPEECH: " + text);
  digitalWrite(LED_PIN, HIGH);
  isSpeaking = true;
  speechStartTime = millis();
  
  // Limit text length
  if (text.length() > 150) {
    text = text.substring(0, 150);
  }
  
  // Simple URL encoding - only encode spaces
  text.replace(" ", "+");
  
  String ttsUrl = "http://translate.google.com/translate_tts?ie=UTF-8&q=" + text + "&tl=en&client=tw-ob";
  
  Serial.println("🎯 TTS URL generated");
  if (audio.connecttohost(ttsUrl.c_str())) {
    Serial.println("✅ TTS started successfully");
  } else {
    Serial.println("❌ TTS failed to start");
    digitalWrite(LED_PIN, LOW);
    isSpeaking = false;
  }
}

void audioEofSpeech(const char *info) {
  Serial.println("✅ FINISHED SPEAKING: " + String(info));
  digitalWrite(LED_PIN, LOW);
  isSpeaking = false;
  Serial.println("🔄 Speaking flag reset to false");
}

void audioInfo(const char *info) {
  Serial.printf("🔊 Audio info: %s\n", info);
}

void checkSpeechTimeout() {
  if (isSpeaking && millis() - speechStartTime > SPEECH_TIMEOUT) {
    Serial.println("⏰ SPEECH TIMEOUT - Resetting speaking flag");
    digitalWrite(LED_PIN, LOW);
    isSpeaking = false;
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Start with LED off
  
  connectToWiFi();
  initializeAudio();
  
  Serial.println("\n🎯 ESP32 #2 - Speaker System Ready");
  Serial.println("✅ WiFi: Connected to " + String(ssid));
  Serial.println("✅ Audio: Initialized"); 
  Serial.println("✅ ThingSpeak: Ready - Channel " + String(thingSpeakChannelID));
  Serial.println("🔊 Waiting for AI responses...");
  
  // Reset speaking flag to be safe
  isSpeaking = false;
  
  // Force first ThingSpeak check immediately
  lastCheckTime = millis() - CHECK_INTERVAL;
}

void loop() {
  audio.loop();
  
  // Check for speech timeout
  checkSpeechTimeout();
  
  // Check ThingSpeak every CHECK_INTERVAL milliseconds
  if (millis() - lastCheckTime >= CHECK_INTERVAL) {
    checkThingSpeak();
    lastCheckTime = millis();
  }
  
  delay(100);
}