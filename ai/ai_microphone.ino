#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "ESP_I2S.h"
#include <vector>

// WiFi credentials
const char* ssid = "EE3070_P1615_1";
const char* password = "EE3070P1615";

// API configuration
const char* deepSeekApiKey = "sk-fbf29f79869b4f168ddcc640f879a985";
const char* assemblyAIKey = "ca37c91925a4422d9c62746e5ea229d4";

// ThingSpeak configuration
const char* thingSpeakApiKey = "X5FBF34JGJGDE2M6";
const unsigned long thingSpeakChannelID = 3172003;

// Web server
WebServer server(80);

// Audio recording
#define I2S_MIC_SERIAL_CLOCK 5
#define I2S_MIC_LEFT_RIGHT_CLOCK 4 
#define I2S_MIC_SERIAL_DATA 6
#define BUTTON_PIN 0
#define LED_PIN 21

I2SClass recordingI2S;
std::vector<int16_t> audioBuffer;
bool isRecording = false;
const int SAMPLE_RATE = 16000;
const int BUFFER_SIZE = 512;
unsigned long recordingStartTime = 0;
const unsigned long MAX_RECORDING_TIME = 8000; // 8 seconds max

// System state
String currentTranscription = "";
String currentAIResponse = "";
String conversationHistory = "";
bool isProcessing = false;

void connectToWiFi() {
  Serial.begin(115200);
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void initializeAudio() {
  // Microphone input - INMP441 configuration
  recordingI2S.setPins(I2S_MIC_SERIAL_CLOCK, I2S_MIC_LEFT_RIGHT_CLOCK, -1, I2S_MIC_SERIAL_DATA);
  if (!recordingI2S.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, 
                         I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("Failed to initialize I2S microphone!");
  } else {
    Serial.println("INMP441 Microphone initialized successfully");
  }
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

void startRecording() {
  if (isProcessing) {
    Serial.println("System busy processing previous request");
    return;
  }
  
  Serial.println("🎤 Starting recording...");
  audioBuffer.clear();
  isRecording = true;
  recordingStartTime = millis();
  digitalWrite(LED_PIN, HIGH);
}

void stopRecording() {
  if (!isRecording) return;
  
  Serial.println("⏹️ Stopping recording...");
  isRecording = false;
  digitalWrite(LED_PIN, LOW);
  
  // Process recording
  processRecording();
}

void continueRecording() {
  if (!isRecording) return;
  
  // Check recording time limit
  if (millis() - recordingStartTime > MAX_RECORDING_TIME) {
    Serial.println("⏰ Recording time limit reached");
    stopRecording();
    return;
  }
  
  int16_t samples[BUFFER_SIZE];
  size_t bytesToRead = BUFFER_SIZE * sizeof(int16_t);
  size_t bytesRead = recordingI2S.readBytes((char*)samples, bytesToRead);
  
  if (bytesRead > 0) {
    size_t samplesRead = bytesRead / sizeof(int16_t);
    for (size_t i = 0; i < samplesRead; i++) {
      audioBuffer.push_back(samples[i]);
    }
    
    // Show recording progress
    if (audioBuffer.size() % (SAMPLE_RATE / 2) == 0) { // Every 0.5 seconds
      float recordingTime = (float)audioBuffer.size() / SAMPLE_RATE;
      Serial.printf("🔊 Recording: %.1f seconds, %d samples\n", recordingTime, audioBuffer.size());
    }
  }
}

String uploadToAssemblyAI() {
  if (audioBuffer.empty()) {
    Serial.println("No audio data to upload");
    return "";
  }

  Serial.printf("📤 Uploading %d samples to AssemblyAI...\n", audioBuffer.size());
  
  // Create WAV buffer
  size_t wavSize = 44 + (audioBuffer.size() * 2);
  uint8_t* wavBuffer = (uint8_t*)malloc(wavSize);
  
  if (!wavBuffer) {
    Serial.println("Failed to allocate WAV buffer");
    return "";
  }

  // Create WAV header
  uint8_t header[44] = {
    'R','I','F','F', 0,0,0,0, 'W','A','V','E', 'f','m','t',' ',
    16,0,0,0, 1,0, 1,0, 0,0,0,0, 0,0,0,0, 2,0, 16,0,
    'd','a','t','a', 0,0,0,0
  };

  uint32_t chunkSize = wavSize - 8;
  uint32_t sampleRate = SAMPLE_RATE;
  uint32_t byteRate = sampleRate * 2;
  uint32_t dataSize = audioBuffer.size() * 2;

  memcpy(&header[4], &chunkSize, 4);
  memcpy(&header[24], &sampleRate, 4);
  memcpy(&header[28], &byteRate, 4);
  memcpy(&header[40], &dataSize, 4);

  memcpy(wavBuffer, header, 44);
  memcpy(wavBuffer + 44, audioBuffer.data(), dataSize);

  // Upload to AssemblyAI
  HTTPClient http;
  http.begin("http://api.assemblyai.com/v2/upload");
  http.addHeader("Authorization", assemblyAIKey);
  http.addHeader("Content-Type", "application/octet-stream");
  
  int uploadCode = http.POST(wavBuffer, wavSize);
  free(wavBuffer);
  
  if (uploadCode != 200) {
    Serial.println("❌ Upload failed: " + String(uploadCode));
    http.end();
    return "";
  }
  
  String uploadResponse = http.getString();
  http.end();
  
  DynamicJsonDocument uploadDoc(1024);
  DeserializationError error = deserializeJson(uploadDoc, uploadResponse);
  
  if (error) {
    Serial.println("JSON parsing error: " + String(error.c_str()));
    return "";
  }
  
  const char* audioUrl = uploadDoc["upload_url"];
  if (!audioUrl) {
    Serial.println("No upload URL received");
    return "";
  }
  
  Serial.println("✅ Audio uploaded: " + String(audioUrl));
  return String(audioUrl);
}

String requestTranscription(String audioUrl) {
  Serial.println("🔄 Requesting transcription...");
  
  HTTPClient http;
  http.begin("http://api.assemblyai.com/v2/transcript");
  http.addHeader("Authorization", assemblyAIKey);
  http.addHeader("Content-Type", "application/json");
  
  String transcriptBody = "{\"audio_url\":\"" + audioUrl + "\"}";
  int transcriptCode = http.POST(transcriptBody);
  
  if (transcriptCode != 200) {
    Serial.println("❌ Transcription request failed: " + String(transcriptCode));
    http.end();
    return "";
  }
  
  String transcriptResponse = http.getString();
  http.end();
  
  DynamicJsonDocument transcriptRespDoc(1024);
  DeserializationError error = deserializeJson(transcriptRespDoc, transcriptResponse);
  
  if (error) {
    Serial.println("JSON parsing error: " + String(error.c_str()));
    return "";
  }
  
  const char* transcriptId = transcriptRespDoc["id"];
  if (!transcriptId) {
    Serial.println("No transcript ID received");
    return "";
  }
  
  Serial.println("📝 Transcript ID: " + String(transcriptId));
  return pollTranscriptionResult(String(transcriptId));
}

String pollTranscriptionResult(String transcriptId) {
  String statusUrl = "http://api.assemblyai.com/v2/transcript/" + transcriptId;
  unsigned long startTime = millis();
  const unsigned long timeout = 30000;
  
  Serial.println("⏳ Waiting for transcription...");
  
  while (millis() - startTime < timeout) {
    HTTPClient http;
    http.begin(statusUrl);
    http.addHeader("Authorization", assemblyAIKey);
    
    int statusCode = http.GET();
    
    if (statusCode == 200) {
      String response = http.getString();
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, response);
      
      if (error) {
        Serial.println("JSON parsing error: " + String(error.c_str()));
        http.end();
        continue;
      }
      
      const char* status = doc["status"];
      Serial.println("📊 Status: " + String(status));
      
      if (strcmp(status, "completed") == 0) {
        const char* text = doc["text"];
        Serial.println("✅ Transcription: " + String(text));
        http.end();
        return String(text);
      } else if (strcmp(status, "error") == 0) {
        const char* errorMsg = doc["error"];
        Serial.println("❌ Transcription error: " + String(errorMsg));
        http.end();
        return "";
      }
    }
    
    http.end();
    delay(2000); // Wait 2 seconds before polling again
  }
  
  Serial.println("⏰ Transcription timeout");
  return "";
}

String getAIResponse(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    return "Error: WiFi not connected";
  }

  HTTPClient http;
  String url = "https://api.deepseek.com/v1/chat/completions";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(deepSeekApiKey));

  String payload = "{\"model\": \"deepseek-chat\", \"messages\": [{\"role\": \"user\", \"content\": \"" + message + "\"}], \"max_tokens\": 300}";
  
  Serial.println("🤖 Sending to DeepSeek AI...");
  int httpResponseCode = http.POST(payload);
  String responseText = "";

  if (httpResponseCode == 200) {
    String response = http.getString();
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      const char* content = doc["choices"][0]["message"]["content"];
      responseText = String(content);
      
      // Clean up response
      responseText.replace("\n", " ");
      Serial.println("🤖 AI Response: " + responseText);
    } else {
      Serial.println("JSON parsing error: " + String(error.c_str()));
      responseText = "Error parsing AI response";
    }
  } else {
    Serial.println("❌ HTTP Error: " + String(httpResponseCode));
    responseText = "Sorry, I couldn't connect to the AI service.";
  }
  
  http.end();
  return responseText;
}

bool sendToThingSpeak(String transcription, String aiResponse) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  String url = "http://api.thingspeak.com/update";
  http.begin(url);
  
  String postData = "api_key=" + String(thingSpeakApiKey) +
                   "&field1=" + urlEncode(transcription) +
                   "&field2=" + urlEncode(aiResponse);
  
  Serial.println("📡 Sending to ThingSpeak...");
  int httpCode = http.POST(postData);
  http.end();
  
  if (httpCode == 200) {
    Serial.println("✅ Data sent to ThingSpeak successfully");
    return true;
  } else {
    Serial.println("❌ Failed to send data to ThingSpeak: " + String(httpCode));
    return false;
  }
}

String urlEncode(String str) {
  String encodedString = "";
  char c;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encodedString += c;
    } else if (c == ' ') {
      encodedString += '+';
    } else {
      encodedString += '%';
      char hex1 = (c >> 4) & 0xF;
      char hex2 = (c & 0xF);
      encodedString += (hex1 < 10) ? (hex1 + '0') : (hex1 - 10 + 'A');
      encodedString += (hex2 < 10) ? (hex2 + '0') : (hex2 - 10 + 'A');
    }
  }
  return encodedString;
}

void processRecording() {
  isProcessing = true;
  Serial.println("🔄 Processing recording...");
  
  // Step 1: Upload to AssemblyAI
  String audioUrl = uploadToAssemblyAI();
  
  if (audioUrl.length() > 0) {
    // Step 2: Get transcription
    String transcription = requestTranscription(audioUrl);
    
    if (transcription.length() > 2) {
      currentTranscription = transcription;
      Serial.println("🎯 Transcription: " + transcription);
      
      // Add to conversation history for web display
      conversationHistory += "<div class='user-msg'>You: " + transcription + "</div>";
      
      // Step 3: Get AI response
      String aiResponse = getAIResponse(transcription);
      currentAIResponse = aiResponse;
      
      // Add AI response to conversation history
      conversationHistory += "<div class='ai-msg'>AI: " + aiResponse + "</div>";
      
      // Step 4: Send to ThingSpeak
      if (sendToThingSpeak(transcription, aiResponse)) {
        Serial.println("✅ Complete! Conversation sent to ThingSpeak");
      }
      
    } else {
      Serial.println("❌ Transcription failed");
      currentTranscription = "Speech recognition failed";
      currentAIResponse = "Please try speaking again clearly";
      conversationHistory += "<div class='error-msg'>Error: Could not understand speech</div>";
    }
  } else {
    Serial.println("❌ Audio upload failed");
    currentTranscription = "Audio upload failed";
    currentAIResponse = "Please check your internet connection";
    conversationHistory += "<div class='error-msg'>Error: Audio upload failed</div>";
  }
  
  isProcessing = false;
}

// Web Server Handlers
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>AI Voice Assistant - Live</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { 
            font-family: Arial, sans-serif; 
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            min-height: 100vh;
        }
        .container { 
            background: rgba(255,255,255,0.1); 
            backdrop-filter: blur(10px);
            padding: 20px; 
            border-radius: 15px; 
            margin-bottom: 20px;
            max-width: 800px;
            margin: 0 auto;
        }
        .status { 
            padding: 15px; 
            border-radius: 10px; 
            margin: 10px 0; 
            font-weight: bold;
            text-align: center;
            font-size: 18px;
        }
        .idle { background: #4CAF50; }
        .recording { background: #FF9800; animation: pulse 1s infinite; }
        .processing { background: #2196F3; animation: pulse 1s infinite; }
        .error { background: #f44336; }
        
        @keyframes pulse {
            0% { opacity: 1; }
            50% { opacity: 0.7; }
            100% { opacity: 1; }
        }
        
        .conversation { 
            background: rgba(255,255,255,0.9);
            color: #333;
            padding: 20px;
            border-radius: 10px;
            min-height: 300px;
            max-height: 500px;
            overflow-y: auto;
            margin: 20px 0;
        }
        .user-msg { 
            background: #E3F2FD; 
            padding: 10px 15px;
            margin: 10px 0;
            border-radius: 10px;
            border-left: 4px solid #2196F3;
        }
        .ai-msg { 
            background: #F3E5F5; 
            padding: 10px 15px;
            margin: 10px 0;
            border-radius: 10px;
            border-left: 4px solid #9C27B0;
        }
        .error-msg { 
            background: #FFEBEE; 
            padding: 10px 15px;
            margin: 10px 0;
            border-radius: 10px;
            border-left: 4px solid #f44336;
            color: #c62828;
        }
        button { 
            padding: 15px 30px; 
            margin: 10px; 
            border: none; 
            border-radius: 25px; 
            cursor: pointer;
            font-size: 16px;
            background: #FF5722;
            color: white;
            transition: all 0.3s;
            font-weight: bold;
        }
        button:hover {
            background: #E64A19;
            transform: scale(1.05);
        }
        button:disabled {
            background: #cccccc;
            cursor: not-allowed;
            transform: none;
        }
        .controls { text-align: center; margin: 20px 0; }
        h1, h2, h3 { text-align: center; }
        .info-box {
            background: rgba(255,255,255,0.2);
            padding: 15px;
            border-radius: 10px;
            margin: 10px 0;
        }
        .timestamp {
            font-size: 12px;
            color: #666;
            text-align: right;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🎤 AI Voice Assistant</h1>
        <h3>Live Conversation - Real Speech to Text</h3>
        
        <div id="status" class="status idle">Status: Ready to Record</div>
        
        <div class="controls">
            <button onclick="startRecording()" id="recordBtn">🎤 Start Recording</button>
            <button onclick="stopRecording()" id="stopBtn" disabled>⏹️ Stop Recording</button>
            <button onclick="clearConversation()">🗑️ Clear Chat</button>
        </div>
        
        <div class="info-box">
            <strong>Instructions:</strong><br>
            1. Click "Start Recording" or press the physical button<br>
            2. Speak clearly into the microphone<br>
            3. Click "Stop Recording" when done<br>
            4. Watch real-time transcription and AI response!
        </div>
        
        <div class="container">
            <h3>Live Conversation</h3>
            <div id="conversation" class="conversation">
                <div class="ai-msg">AI: Hello! I'm ready to help. Press the record button and speak to me! 🎤</div>
            </div>
        </div>
        
        <div class="container">
            <h3>System Information</h3>
            <div class="info-box">
                <strong>IP Address:</strong> )rawliteral" + WiFi.localIP().toString() + R"rawliteral(<br>
                <strong>Microphone:</strong> INMP441 (Active)<br>
                <strong>AI Service:</strong> DeepSeek API<br>
                <strong>Speech-to-Text:</strong> AssemblyAI<br>
                <strong>Last Update:</strong> <span id="lastUpdate">Just now</span>
            </div>
        </div>
    </div>

    <script>
        function updateStatus(status, text) {
            const statusDiv = document.getElementById('status');
            statusDiv.className = 'status ' + status;
            statusDiv.textContent = 'Status: ' + text;
            
            // Update buttons
            if (status === 'recording') {
                document.getElementById('recordBtn').disabled = true;
                document.getElementById('stopBtn').disabled = false;
            } else if (status === 'processing') {
                document.getElementById('recordBtn').disabled = true;
                document.getElementById('stopBtn').disabled = true;
            } else {
                document.getElementById('recordBtn').disabled = false;
                document.getElementById('stopBtn').disabled = true;
            }
        }
        
        function addMessage(type, text) {
            const conversation = document.getElementById('conversation');
            const messageDiv = document.createElement('div');
            messageDiv.className = type + '-msg';
            
            const timestamp = new Date().toLocaleTimeString();
            messageDiv.innerHTML = (type === 'user' ? '🎤 You: ' : '🤖 AI: ') + text + 
                                 '<div class="timestamp">' + timestamp + '</div>';
            
            conversation.appendChild(messageDiv);
            conversation.scrollTop = conversation.scrollHeight;
            updateLastUpdate();
        }
        
        function updateLastUpdate() {
            document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
        }
        
        function startRecording() {
            fetch('/start-recording')
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        updateStatus('recording', 'Recording... Speak now!');
                        addMessage('user', '🔴 Recording in progress...');
                    }
                })
                .catch(error => {
                    console.error('Error:', error);
                    updateStatus('error', 'Failed to start recording');
                });
        }
        
        function stopRecording() {
            updateStatus('processing', 'Processing speech...');
            
            fetch('/stop-recording')
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        // Update conversation with actual transcription and AI response
                        const conversation = document.getElementById('conversation');
                        // Remove the "Recording in progress" message
                        if (conversation.lastChild) {
                            conversation.removeChild(conversation.lastChild);
                        }
                        
                        addMessage('user', data.transcription);
                        addMessage('ai', data.ai_response);
                        updateStatus('idle', 'Ready to record');
                    }
                })
                .catch(error => {
                    console.error('Error:', error);
                    updateStatus('error', 'Processing failed');
                });
        }
        
        function clearConversation() {
            fetch('/clear-conversation')
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        document.getElementById('conversation').innerHTML = 
                            '<div class="ai-msg">AI: Conversation cleared. Ready for your questions! 🎤</div>';
                        updateLastUpdate();
                    }
                });
        }
        
        // Auto-update status every second
        setInterval(() => {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    if (data.status !== 'recording' && data.status !== 'processing') {
                        document.getElementById('status').textContent = 'Status: ' + data.status;
                    }
                });
        }, 1000);
    </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleStartRecording() {
  startRecording();
  server.send(200, "application/json", "{\"success\": true}");
}

void handleStopRecording() {
  stopRecording();
  String json = "{\"success\": true, \"transcription\": \"" + currentTranscription + "\", \"ai_response\": \"" + currentAIResponse + "\"}";
  server.send(200, "application/json", json);
}

void handleStatus() {
  String status = "Ready";
  if (isRecording) status = "Recording...";
  if (isProcessing) status = "Processing...";
  
  server.send(200, "application/json", "{\"status\": \"" + status + "\"}");
}

void handleClearConversation() {
  conversationHistory = "<div class='ai-msg'>AI: Conversation cleared. Ready for your questions! 🎤</div>";
  server.send(200, "application/json", "{\"success\": true}");
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/start-recording", handleStartRecording);
  server.on("/stop-recording", handleStopRecording);
  server.on("/status", handleStatus);
  server.on("/clear-conversation", handleClearConversation);
  server.begin();
  Serial.println("Web server started");
}

void setup() {
  connectToWiFi();
  initializeAudio();
  setupWebServer();
  
  Serial.println("\n=== ESP32 #1 - AI Processor Ready ===");
  Serial.println("Web interface: http://" + WiFi.localIP().toString());
  Serial.println("Press the button to start recording");
  Serial.println("Real microphone STT enabled with AssemblyAI");
}

void loop() {
  server.handleClient();
  
  // Handle physical button
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(BUTTON_PIN);
  
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    if (!isRecording && !isProcessing) {
      startRecording();
    }
  }
  
  if (currentButtonState == HIGH && lastButtonState == LOW) {
    if (isRecording) {
      stopRecording();
    }
  }
  
  lastButtonState = currentButtonState;
  
  // Continue recording if active
  if (isRecording) {
    continueRecording();
  }
  
  delay(10);
}