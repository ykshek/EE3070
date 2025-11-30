#include <ThingSpeak.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Connect your yellow pin to Pin5
#define ONE_WIRE_BUS 5

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature sensor 𠃌
DallasTemperature sensors(&oneWire);

void water_setup(void)
{
  // initialize the Serial Monitor at a baud rate of 9600.
  sensors.begin();
}
 
void water_loop(void){ 
  // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  sensors.requestTemperatures(); 
  float watertempC = sensors.getTempCByIndex(0);
  //if (watertempC != DEVICE_DISCONNECTED_C) {
  if (true) {  
    // Set field in ThingSpeak
    Serial.print("Water Temperature (ºC): ");
    Serial.println(watertempC);
    ThingSpeak.setField(4, watertempC);
  } 
  else {
    Serial.println("Error: Could not read temperature data");
  }

}