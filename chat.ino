#include <WiFi.h>
#include <ArduinoGPTChat.h>
#include "Audio.h"

// Define I2S pins for audio output
#define I2S_DOUT 47
#define I2S_BCLK 48
#define I2S_LRC 45

// Define INMP441 microphone input pins (I2S standard mode)
#define I2S_MIC_SERIAL_CLOCK 5    // SCK - serial clock
#define I2S_MIC_LEFT_RIGHT_CLOCK 4 // WS - left/right clock
#define I2S_MIC_SERIAL_DATA 6     // SD - serial data

// Define boot button pin (GPIO0 is the boot button on most ESP32 boards)
#define BOOT_BUTTON_PIN 0

// Sample rate for recording
#define SAMPLE_RATE 8000

// I2S configuration for microphone (INMP441 settings)
#define I2S_MODE I2S_MODE_STD
#define I2S_BIT_WIDTH I2S_DATA_BIT_WIDTH_16BIT
#define I2S_SLOT_MODE I2S_SLOT_MODE_MONO
#define I2S_SLOT_MASK I2S_STD_SLOT_LEFT

// WiFi settings
const char* ssid     = "your_ssid";
const char* password = "your_password";

// Global audio variable declaration for TTS playback
Audio audio;

// API configurations
const char* deepSeekApiKey = "";
const char* deepSeekApiUrl = "https://api.deepseek.com";
const char* assemblyAIKey = "";
ArduinoGPTChat gptChat(deepSeekApiKey, deepSeekApiUrl);

// System prompt configuration
const char* systemPrompt = "Please answer questions briefly, responses should not exceed 30 words. Avoid lengthy explanations, provide key information directly.";

// Button handling variables
bool buttonPressed = false;
bool wasButtonPressed = false;

class TTSService {
private:
    bool _isPlaying = false;
    int _currentService = 0; // 0=Google, 1=VoiceRSS, 2=Beep
    
public:
    void playTTS(String text) {
        Serial.println("🔊 TTS Request: " + text);
        
        if (_isPlaying) {
            Serial.println("⚠️  TTS already playing, skipping...");
            return;
        }
        
        // Clean text for URL
        String cleanedText = cleanTextForTTS(text);
        
        bool success = false;
        
        // Try different TTS services in order
        for (int attempt = 0; attempt < 3 && !success; attempt++) {
            switch (_currentService) {
                case 0:
                    success = googleTTS(cleanedText);
                    if (!success) {
                        Serial.println("❌ Google TTS failed, trying next service...");
                        _currentService = 1;
                    }
                    break;
                    
                case 1:
                    success = voiceRSS_TTS(cleanedText);
                    if (!success) {
                        Serial.println("❌ VoiceRSS failed, trying beep...");
                        _currentService = 2;
                    }
                    break;
                    
                case 2:
                    success = playBeep();
                    break;
            }
        }
        
        if (success) {
            _isPlaying = true;
            Serial.println("✅ TTS playback started with service: " + String(_currentService));
        } else {
            Serial.println("❌ All TTS services failed");
        }
    }
    
private:
    String cleanTextForTTS(String text) {
        // Simple cleaning - only keep basic characters
        String cleaned = "";
        for (int i = 0; i < text.length() && cleaned.length() < 50; i++) {
            char c = text.charAt(i);
            if (isAlphaNumeric(c) || c == ' ') {
                cleaned += c;
            }
        }
        cleaned.replace(" ", "%20");
        return cleaned;
    }
    
    bool googleTTS(String text) {
        // Use a more reliable Google TTS URL format
        String ttsUrl = "http://translate.google.com/translate_tts?ie=UTF-8&tl=en&client=tw-ob&q=" + text;
        Serial.println("🎯 Trying Google TTS...");
        return audio.connecttospeech(text.c_str(), "en");
    }
    
    bool voiceRSS_TTS(String text) {
        // VoiceRSS service (you'll need to sign up for free API key)
        const char* voiceRSSKey = ""; // Get from http://www.voicerss.org/
        if (strlen(voiceRSSKey) == 0) {
            return false; // No key configured
        }
        
        String ttsUrl = "http://api.voicerss.org/?key=" + String(voiceRSSKey) + 
                       "&hl=en-us&src=" + text + "&c=MP3&f=16khz_16bit_stereo";
        Serial.println("🎯 Trying VoiceRSS TTS...");
        return audio.connecttohost(ttsUrl.c_str());
    }
    
    bool playBeep() {
        // Fallback: Play simple beep sounds
        Serial.println("🎯 Playing beep sequence...");
        return audio.connecttospeech("Response ready", "en");
    }
    
public:
    void playbackFinished() {
        _isPlaying = false;
        Serial.println("✅ Playback completed");
    }
    
    bool isPlaying() {
        return _isPlaying;
    }
};

TTSService ttsService;

/*
// Audio status callbacks
void audio_info(const char *info){
    Serial.print("audio_info: "); 
    Serial.println(info);
}

void audio_eof_mp3(const char *info){
    Serial.print("eof_mp3: "); 
    Serial.println(info);
    ttsService.playbackFinished(); // Notify TTS service
}

void audio_showstation(const char *info){
    Serial.print("station: "); 
    Serial.println(info);
}

void audio_showstreamtitle(const char *info){
    Serial.print("streamtitle: "); 
    Serial.println(info);
}

void audio_showstreaminfo(const char *info){
    Serial.print("streaminfo: "); 
    Serial.println(info);
}
void audio_id3data(const char *info){
    Serial.print("id3data: "); 
    Serial.println(info);
}

void audio_lasthost(const char *info){
    Serial.print("lasthost: "); 
    Serial.println(info);
}

void audio_showcodec(const char *info){
    Serial.print("codec: "); 
    Serial.println(info);
}
*/
void reliableTTS(String text) {
    Serial.println("🔊 Using reliable TTS method...");
    
    // Limit text length
    if (text.length() > 100) {
        text = text.substring(0, 100);
    }
    
    // Use the built-in speech synthesis (more reliable than URLs)
    bool success = audio.connecttospeech(text.c_str(), "en");
    
    if (success) {
        Serial.println("✅ TTS started successfully");
    } else {
        Serial.println("❌ TTS failed to start");
        // Fallback to beep
        audio.connecttospeech("Done", "en");
    }
}

void ensureStableConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("📡 WiFi disconnected, reconnecting...");
        WiFi.reconnect();
        delay(2000);
    }
}
/*
void testSystem() {
    Serial.println("🧪 Testing system components...");
    
    // Test TTS
    Serial.println("Testing TTS...");
    ttsService.playTTS("System test successful");
    delay(5000);
}

void testDeepSeekAPI() {
    Serial.println("🧪 Testing DeepSeek API...");
    
    // Test with a simple message
    String testResponse = gptChat.sendMessage("Say 'API test successful' if you can hear me");
    
    if (testResponse != "") {
        Serial.println("✅ DeepSeek API Working!");
        Serial.println("Response: " + testResponse);
    } else {
        Serial.println("❌ DeepSeek API Failed - No response");
    }
}

*/
void testSpeaker() {
    //Serial.println("🔊 Testing speaker hardware...");
    
    // Test with different audio sources
    //Serial.println("1. Testing with MP3 URL...");
    //audio.connecttohost("https://www.soundhelix.com/examples/mp3/SoundHelix-Song-1.mp3");
    //delay(5000); // Play for 5 seconds
    
    Serial.println("2. Testing with Google TTS...");
    audio.connecttohost("http://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&q=Hello+this+is+a+speaker+test&tl=en");
    delay(5000);
    
    //Serial.println("3. Testing with beep sound...");
    //audio.connecttohost("https://www2.cs.uic.edu/~i101/SoundFiles/Beep16.wav");
    //delay(3000);
    
    //Serial.println("🔊 Speaker test completed");
}

void simpleTTS(String text) {
    Serial.println("🔊 Speaking: " + text);
    
    // Clean and limit text
    text.replace("'", "");
    text.replace("\"", "");
    if (text.length() > 80) {
        text = text.substring(0, 80);
    }
    
    // Use the most reliable method
    bool success = audio.connecttospeech(text.c_str(), "en");
    
    if (success) {
        Serial.println("✅ TTS started");
        
        // Wait for playback to complete
        unsigned long startTime = millis();
        while (audio.isRunning() && (millis() - startTime < 10000)) {
            audio.loop();
            delay(10);
        }
        
        if (audio.isRunning()) {
            Serial.println("⏹️ Stopping long playback");
            audio.stopSong();
        }
    } else {
        Serial.println("❌ TTS failed");
        // Fallback - just play a tone
        audio.connecttospeech("OK", "en");
    }
}

void setup() {
  // Initialize serial port
  Serial.begin(115200);
  delay(1000); // Give serial port some time to initialize

  Serial.println("\n\n----- Voice Assistant with AssemblyAI -----");

  // Initialize boot button
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    Serial.print('.');
    delay(1000);
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    //testSpeaker();
    
    //testSystem();
    //testDeepSeekAPI();
    

    // Set I2S output pins (for TTS playback)
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(100);
    audio.setBufsize(1024, 512); // Larger buffers for stability
    audio.setConnectionTimeout(10000, 10000); // 10 second timeouts

 
    

    // Set system prompt
    gptChat.setSystemPrompt(systemPrompt);

    // Configure AssemblyAI
    gptChat.setAssemblyAIConfig(assemblyAIKey);

    // Initialize recording with microphone pins and I2S configuration
    gptChat.initializeRecording(I2S_MIC_SERIAL_CLOCK, I2S_MIC_LEFT_RIGHT_CLOCK, I2S_MIC_SERIAL_DATA,
                               SAMPLE_RATE, I2S_MODE, I2S_BIT_WIDTH, I2S_SLOT_MODE, I2S_SLOT_MASK);

    Serial.println("\n----- System Ready with AssemblyAI STT-----");
    Serial.println("Hold BOOT button to record speech, release to send to deepseek");
  } else {
    Serial.println("\nFailed to connect to WiFi. Please check network credentials and retry.");
  }
}

void loop() {
  // Handle audio loop (TTS playback)
  audio.loop();

  // Only process new commands if audio is not playing
  if (!ttsService.isPlaying()) {
  // Handle boot button for push-to-talk
  buttonPressed = (digitalRead(BOOT_BUTTON_PIN) == LOW); // LOW when pressed (pull-up resistor)

  // Button pressed - start recording
  if (buttonPressed && !wasButtonPressed && !gptChat.isRecording()) {
    Serial.println("\n----- Recording Started (Hold button) -----");
    if (gptChat.startRecording()) {
      Serial.println("Speak now... (release button to stop)");
      wasButtonPressed = true;
    }
  }
  // Button released - stop recording and process
  else if (!buttonPressed && wasButtonPressed && gptChat.isRecording()) {
    Serial.println("\n----- Recording Stopped -----");
    String transcribedText = gptChat.assemblyAISpeechToTextFromRecording();

    if (transcribedText.length() > 0) {
      Serial.println("\nRecognition result: " + transcribedText);
      Serial.println("\nSending recognition result to deepseek...");

      // Send message to DeepSeek
      String response = gptChat.sendMessage(transcribedText);

      if (response != "") {
        Serial.print("DeepSeek: ");
        Serial.println(response);
        //simpleTTS(response);
        
        // Convert reply to speech
        if (response.length() > 0) {
          Serial.println("Converting text to speech...");
          ensureStableConnection();
          reliableTTS(response);
        }
      } else {
        Serial.println("Failed to get DeepSeek response");
      }
    } else {
      Serial.println("Failed to recognize text or an error occurred.");
      Serial.println("Clear speech may not have been detected, please try again.");
    }

    wasButtonPressed = false;
  }
}
  // Continue recording while button is held
  if (buttonPressed && gptChat.isRecording()) {
    gptChat.continueRecording();
  }
  // Update button state
  if (!buttonPressed && !gptChat.isRecording()) {
    wasButtonPressed = false;
  }


  delay(10); // Small delay to prevent CPU overload
}
