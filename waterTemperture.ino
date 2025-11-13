#include <ThingSpeak.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
// Connect your yellow pin to Pin12 on Arduino
#define ONE_WIRE_BUS 5
 
const char* ssid = "";
const char* password = "";
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature sensor 𠃌
DallasTemperature sensors(&oneWire);

unsigned long channelID = ; 
const char* writeApiKey = "";
WiFiClient client;
void setup(void)
{
  // initialize the Serial Monitor at a baud rate of 9600.
  Serial.begin(115200);
  sensors.begin();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Initialize ThingSpeak
  ThingSpeak.begin(client);
}
 
void loop(void){ 
  // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); 
  Serial.println("DONE");
  float temperatureC = sensors.getTempCByIndex(0);
  
  Serial.print("Celsius temperature: ");
  Serial.print(sensors.getTempCByIndex(0)); 
  if (temperatureC != DEVICE_DISCONNECTED_C) {
    // Set field in ThingSpeak
    ThingSpeak.setField(1, temperatureC);
    
    // Write to ThingSpeak
    int statusCode = ThingSpeak.writeFields(channelID, writeApiKey);
    
    if (statusCode == 200) {
      Serial.println("Data uploaded to ThingSpeak successfully!");
    } else {
      Serial.print("Error uploading to ThingSpeak. Status code: ");
      Serial.println(statusCode);
    }
  } else {
    Serial.println("Error: Could not read temperature data");
  }
  
  // Wait 20 seconds (ThingSpeak free account limit)
  delay(20000);
}