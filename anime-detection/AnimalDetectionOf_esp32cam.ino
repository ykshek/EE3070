#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>
#include <ArduinoJson.h>

const char* WIFI_SSID = "YEUNGHOME";
const char* WIFI_PASS = "21780260";

WebServer server(80);

// Detection results storage
String lastDetection = "No animals detected";
float lastConfidence = 0.0;
String detectedAnimal = "None";
int detectionCount = 0;
bool turtleAlert = false;

static auto loRes = esp32cam::Resolution::find(320, 240);
static auto hiRes = esp32cam::Resolution::find(800, 600);

void serveJpg() {
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("Capture Fail");
    server.send(503, "", "");
    return;
  }

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

void handleJpgLo() {
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  serveJpg();
}

void handleJpgHi() {
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

void handleStream() {
  WiFiClient client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println("Access-Control-Allow-Origin: *");
  client.println();
  
  while (client.connected()) {
    auto frame = esp32cam::capture();
    if (frame == nullptr) {
      continue;
    }
    
    client.println("--frame");
    client.println("Content-Type: image/jpeg");
    client.println("Content-Length: " + String(frame->size()));
    client.println();
    frame->writeTo(client);
    client.println();
    
    delay(50);
  }
}

void handleDetectionData() {
  StaticJsonDocument<500> doc;
  doc["animal"] = detectedAnimal;
  doc["confidence"] = lastConfidence;
  doc["count"] = detectionCount;
  doc["status"] = lastDetection;
  doc["turtleAlert"] = turtleAlert;
  doc["timestamp"] = millis();
  
  String response;
  serializeJson(doc, response);
  
  server.send(200, "application/json", response);
}

void handleUpdateDetection() {
  if (server.hasArg("animal") && server.hasArg("confidence")) {
    detectedAnimal = server.arg("animal");
    lastConfidence = server.arg("confidence").toFloat();
    detectionCount++;
    turtleAlert = (detectedAnimal == "turtle" || detectedAnimal == "tortoise");
    
    lastDetection = detectedAnimal + " detected (" + String(lastConfidence * 100, 1) + "%)";
    
    Serial.println("UPDATE: " + lastDetection);
    
    server.send(200, "application/json", "{\"status\":\"updated\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"missing parameters\"}");
  }
}

void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
      <title>ESP32-CAM Accurate Animal Detection</title>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
          body { 
              font-family: Arial, sans-serif; 
              text-align: center; 
              background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
              margin: 0;
              padding: 20px;
              min-height: 100vh;
          }
          .container {
              max-width: 900px;
              margin: 0 auto;
              background: white;
              padding: 25px;
              border-radius: 15px;
              box-shadow: 0 10px 30px rgba(0,0,0,0.2);
          }
          .header {
              background: linear-gradient(135deg, #4CAF50, #45a049);
              color: white;
              padding: 20px;
              border-radius: 10px;
              margin-bottom: 20px;
          }
          .video-container {
              margin: 25px 0;
              position: relative;
          }
          img, video {
              max-width: 100%;
              border: 3px solid #333;
              border-radius: 10px;
              box-shadow: 0 5px 15px rgba(0,0,0,0.1);
          }
          .detection-info {
              background: #e8f5e8;
              padding: 20px;
              border-radius: 10px;
              margin: 15px 0;
              border-left: 5px solid #4CAF50;
              transition: all 0.3s ease;
          }
          .turtle-alert {
              background: linear-gradient(135deg, #ffeaa7, #fab1a0);
              border-left: 5px solid #e17055;
              animation: alertPulse 2s infinite;
          }
          .no-detection {
              background: #f8f9fa;
              border-left: 5px solid #6c757d;
          }
          @keyframes alertPulse {
              0% { transform: scale(1); box-shadow: 0 0 20px rgba(255,193,7,0.5); }
              50% { transform: scale(1.02); box-shadow: 0 0 30px rgba(255,193,7,0.8); }
              100% { transform: scale(1); box-shadow: 0 0 20px rgba(255,193,7,0.5); }
          }
          .stats-grid {
              display: grid;
              grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
              gap: 15px;
              margin: 25px 0;
          }
          .stat-card {
              background: linear-gradient(135deg, #74b9ff, #0984e3);
              color: white;
              padding: 20px;
              border-radius: 10px;
              box-shadow: 0 5px 15px rgba(0,0,0,0.1);
          }
          .stat-card h3 {
              margin: 0 0 10px 0;
              font-size: 14px;
              opacity: 0.9;
          }
          .stat-card p {
              margin: 0;
              font-size: 24px;
              font-weight: bold;
          }
          .animal-icon {
              font-size: 48px;
              margin: 10px 0;
          }
          .controls {
              margin: 25px 0;
              display: flex;
              justify-content: center;
              gap: 10px;
              flex-wrap: wrap;
          }
          button {
              background: linear-gradient(135deg, #667eea, #764ba2);
              color: white;
              border: none;
              padding: 12px 25px;
              border-radius: 25px;
              cursor: pointer;
              font-size: 16px;
              font-weight: bold;
              transition: all 0.3s ease;
              box-shadow: 0 5px 15px rgba(0,0,0,0.2);
          }
          button:hover {
              transform: translateY(-2px);
              box-shadow: 0 8px 20px rgba(0,0,0,0.3);
          }
          .confidence-bar {
              width: 100%;
              height: 20px;
              background: #f0f0f0;
              border-radius: 10px;
              margin: 10px 0;
              overflow: hidden;
          }
          .confidence-fill {
              height: 100%;
              background: linear-gradient(90deg, #4CAF50, #8BC34A);
              border-radius: 10px;
              transition: width 0.5s ease;
          }
          .high-confidence { background: linear-gradient(90deg, #4CAF50, #8BC34A); }
          .medium-confidence { background: linear-gradient(90deg, #FFC107, #FF9800); }
          .low-confidence { background: linear-gradient(90deg, #F44336, #E91E63); }
      </style>
  </head>
  <body>
      <div class="container">
          <div class="header">
              <h1>🐾 ESP32-CAM Animal Detection</h1>
              <p>Accurate real-time animal detection with turtle alerts</p>
          </div>
          
          <div class="stats-grid">
              <div class="stat-card">
                  <h3>CURRENT ANIMAL</h3>
                  <div class="animal-icon" id="animalIcon">🐾</div>
                  <p id="animal">None</p>
              </div>
              <div class="stat-card">
                  <h3>CONFIDENCE</h3>
                  <p id="confidence">0%</p>
                  <div class="confidence-bar">
                      <div id="confidenceBar" class="confidence-fill" style="width: 0%"></div>
                  </div>
              </div>
              <div class="stat-card">
                  <h3>TOTAL DETECTIONS</h3>
                  <p id="count">0</p>
              </div>
          </div>
          
          <div id="detectionInfo" class="detection-info no-detection">
              <h2>Status: <span id="status">Waiting for detection...</span></h2>
              <p id="detectionTime">Last updated: Never</p>
          </div>
          
          <div class="controls">
              <button onclick="switchView('image')">📷 Static Image</button>
              <button onclick="switchView('stream')">🎥 Live Stream</button>
              <button onclick="refreshData()">🔄 Refresh</button>
          </div>
          
          <div class="video-container">
              <img id="imageView" src="/cam-hi.jpg" style="display:none;" alt="Static Image">
              <img id="streamView" src="/stream" style="display:none;" alt="Live Stream">
          </div>
          
          <div style="margin-top: 20px; color: #666; font-size: 14px;">
              <p>Detecting: Cats, Dogs, Birds, Turtles, Tortoises, and other animals</p>
              <p>🐢 Turtle detections will trigger special alerts</p>
          </div>
      </div>

      <script>
          let currentView = 'stream';
          const animalIcons = {
              'cat': '🐱', 'dog': '🐶', 'bird': '🐦', 'turtle': '🐢', 'tortoise': '🐢',
              'horse': '🐴', 'cow': '🐮', 'elephant': '🐘', 'bear': '🐻', 'zebra': '🦓',
              'giraffe': '🦒', 'sheep': '🐑', 'none': '🐾'
          };
          
          function switchView(view) {
              document.getElementById('imageView').style.display = 'none';
              document.getElementById('streamView').style.display = 'none';
              
              if (view === 'image') {
                  document.getElementById('imageView').style.display = 'block';
                  currentView = 'image';
              } else {
                  document.getElementById('streamView').style.display = 'block';
                  currentView = 'stream';
              }
          }
          
          function updateDetectionInfo() {
              fetch('/detection')
                  .then(response => response.json())
                  .then(data => {
                      const animal = data.animal.toLowerCase();
                      const confidence = data.confidence * 100;
                      
                      // Update basic info
                      document.getElementById('animal').textContent = data.animal;
                      document.getElementById('confidence').textContent = confidence.toFixed(1) + '%';
                      document.getElementById('count').textContent = data.count;
                      document.getElementById('status').textContent = data.status;
                      document.getElementById('detectionTime').textContent = 'Last updated: ' + new Date().toLocaleTimeString();
                      
                      // Update animal icon
                      const icon = animalIcons[animal] || animalIcons['none'];
                      document.getElementById('animalIcon').textContent = icon;
                      
                      // Update confidence bar
                      const confidenceBar = document.getElementById('confidenceBar');
                      confidenceBar.style.width = confidence + '%';
                      confidenceBar.className = 'confidence-fill ' + 
                          (confidence > 70 ? 'high-confidence' : 
                           confidence > 50 ? 'medium-confidence' : 'low-confidence');
                      
                      // Update detection box style
                      const detectionDiv = document.getElementById('detectionInfo');
                      if (data.turtleAlert) {
                          detectionDiv.className = 'detection-info turtle-alert';
                          detectionDiv.innerHTML = `
                              <h2>🚨 TURTLE ALERT! 🚨</h2>
                              <p>Turtle detected with ${confidence.toFixed(1)}% confidence!</p>
                              <p><strong>Last updated: ${new Date().toLocaleTimeString()}</strong></p>
                          `;
                      } else if (data.animal !== 'None') {
                          detectionDiv.className = 'detection-info';
                          detectionDiv.innerHTML = `
                              <h2>Animal Detected: ${data.animal}</h2>
                              <p>Confidence: ${confidence.toFixed(1)}% | Total detections: ${data.count}</p>
                              <p><strong>Last updated: ${new Date().toLocaleTimeString()}</strong></p>
                          `;
                      } else {
                          detectionDiv.className = 'detection-info no-detection';
                          detectionDiv.innerHTML = `
                              <h2>Status: No animals detected</h2>
                              <p>Waiting for animal detection...</p>
                              <p><strong>Last updated: ${new Date().toLocaleTimeString()}</strong></p>
                          `;
                      }
                  })
                  .catch(error => {
                      console.error('Error fetching detection data:', error);
                  });
          }
          
          function refreshData() {
              updateDetectionInfo();
              if (currentView === 'image') {
                  document.getElementById('imageView').src = '/cam-hi.jpg?t=' + new Date().getTime();
              }
          }
          
          // Initial setup
          switchView('stream');
          updateDetectionInfo();
          setInterval(updateDetectionInfo, 1500); // Update every 1.5 seconds
          setInterval(() => {
              if (currentView === 'image') {
                  document.getElementById('imageView').src = '/cam-hi.jpg?t=' + new Date().getTime();
              }
          }, 3000);
      </script>
  </body>
  </html>
  )rawliteral";
  
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);

    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  Serial.println();
  Serial.println("✅ Accurate Animal Detection Camera Ready!");
  Serial.print("🌐 Web Interface: http://");
  Serial.println(WiFi.localIP());
  Serial.println("📱 Open this URL on any device to view detections");

  server.on("/", handleRoot);
  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/cam-hi.jpg", handleJpgHi);
  server.on("/stream", handleStream);
  server.on("/detection", handleDetectionData);
  server.on("/update", handleUpdateDetection);

  server.begin();
}

void loop() {
  server.handleClient();
}