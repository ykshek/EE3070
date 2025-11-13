
#include <WiFi.h>
#include "ThingSpeak.h"
#include "secrets.h"


WiFiClient client;
char* ssid = SECRET_SSID;
char* pass = SECRET_PASS;
int statusCode;

unsigned long counterChannelNumber = SECRET_CH_ID;
const char * myCounterReadAPIKey = SECRET_READ_APIKEY;
const char * myCounterWriteAPIKey = SECRET_WRITE_APIKEY;

// Loop init
unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

int writeStatus() {
  int statusCode = ThingSpeak.getLastReadStatus();
  if (statusCode == 200) Serial.println("Channel update successful.");
  else Serial.println("Problem updating channel. HTTP error code " + String(statusCode));
  return 0;
}

void IoT_setup() {

  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(client);  // Initialize ThingSpeak

  if(WiFi.status() != WL_CONNECTED){
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, pass); // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(5000);
    }
    Serial.println("\nConnected");
  }
}

void setup() {
  // put your setup code here, to run once:
  IoT_setup();
  air_setup();
  ph_setup();
  water_setup();

}

void loop() {
  // put your main code here, to run repeatedly:
  if ((millis() - lastTime) > timerDelay) {
    Serial.println();
    air_loop();
    ph_loop();
    water_loop();
    statusCode = ThingSpeak.writeFields(counterChannelNumber, myCounterWriteAPIKey);
    writeStatus();
    lastTime = millis();
  }
}
