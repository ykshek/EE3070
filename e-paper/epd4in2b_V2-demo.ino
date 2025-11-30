/* Includes ------------------------------------------------------------------*/
#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "imagedata.h"
#include <stdlib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ThingSpeak.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
//SENSORS:
//WATER TEMP
//#define ONE_WIRE_BUS 5
// Setup a oneWire instance to communicate with any OneWire devices
//OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature sensor 𠃌
//DallasTemperature sensors(&oneWire);


#define VREF 3.3 // ESP32輸入電壓為3.3V
#define SCOUNT 30 // 取樣數
int analogBuffer[SCOUNT];
int analogBufferIndex = 0;
float averageVoltage = 0;
float pHValue = 0;
float pHslope = -4.8475;
float pHintercept = 18.30;
String currentPetName = "";
String CleanedName ="";

const char* ssid = "EE3070_P1615_1";  //EE3070_P1615_1
const char* password = "EE3070P1615";  //EE3070P1615

unsigned long channelID1 = 3076216;
const char* readApiKey1 = "LQF80J9858PAMGL3"; //water temp

unsigned long channelID2 = 3150871; 
const char* readApiKey2 = "NZ3XXPOL0ODMR2AT"; // feed

unsigned long channelID3 = 3076216; // 
const char* readApiKey3 = "LQF80J9858PAMGL3"; // air

unsigned long channelID4 = 3076216; //
const char* readApiKey4 = "LQF80J9858PAMGL3"; //ph

unsigned long channelID5 = 3160211;
const char* readApiKey5 = "5OEXPL7X2NGEMHI4"; //INPUTNAME

WiFiClient client;
String url = "https://api.thingspeak.com/channels/" + 
               String(channelID1) + 
               "/feeds/last.json?api_key=" + 
               String(readApiKey1);

/* Entry point ----------------------------------------------------------------*/
void setup()
{
  //SENSORS:PROVIDE INFO
  //Serial.begin(115200);
  // WATER TEMP
  //sensors.begin();
  DEV_Module_Init();
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  ThingSpeak.begin(client);
  
  //sensors.requestTemperatures(); 
  //float watertempF = sensors.getTempCByIndex(0);
  //Serial.print(sensors.getTempCByIndex(0)); 
  Serial.println("First to start: What is your tortoise's name?");
  boolean go = false;
  while (!go){
  HTTPClient http;
  String url = "https://api.thingspeak.com/channels/" + 
               String(channelID1) + 
               "/feeds/last.json?api_key=" + 
               String(readApiKey1);
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    const char* field2 = doc["field2"];
    int entryId = doc["entry_id"];
    if (field2 != nullptr && String(field2) != "null" && String(field2) != currentPetName) {
      currentPetName = String(field2);
      CleanedName = currentPetName.substring(5);
      CleanedName.trim();
      Serial.println("React Processed New Name!");
      Serial.println("Entry ID: " + String(entryId));
      Serial.println("Tortoise's Name: " + currentPetName);
      Serial.println("-------------------");
      go = true;
      // Add your e-paper display code here
      // displayPetName(currentPetName);
    }
  } 
  else {
    Serial.println("❌ HTTP Error: " + String(httpCode));
  }
  
  http.end();
    /*
    if (Serial.available() > 0) {
    inputName = Serial.readStringUntil('\n');
    inputName.trim();
    if (inputName.length() > 0) {
        Serial.print("Your tortoise's name is: ");
        Serial.println(inputName);
        
      }
  }
  */

}
}



/* The main loop -------------------------------------------------------------*/
void loop()
{
  HTTPClient http;
  String url = "https://api.thingspeak.com/channels/" + 
               String(channelID5) + 
               "/feeds/last.json?api_key=" + 
               String(readApiKey5);
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    const char* field2 = doc["field2"];
    int entryId = doc["entry_id"];
    if (field2 != nullptr && String(field2) != "null" && String(field2) != currentPetName) {
      currentPetName = String(field2);
      CleanedName = currentPetName.substring(5);
      CleanedName.trim();
      Serial.println("React Processed New Name!");
      Serial.println("Entry ID: " + String(entryId));
      Serial.println("Tortoise's Name: " + currentPetName);
      Serial.println("-------------------");
    }
  } 
  else {
    Serial.println("❌ HTTP Error: " + String(httpCode));
  }
  
  http.end();
  delay(10);
  float WATERtemperature = ThingSpeak.readFloatField(channelID1, 4, readApiKey1);
  int statusCode1 = ThingSpeak.getLastReadStatus();
  String watertempS = String(WATERtemperature)+"C";
  const char* watertemp = watertempS.c_str();
    if (statusCode1 == 200) {
    Serial.print("Last temperature reading: ");
    Serial.print(WATERtemperature);
    Serial.println(" °C");
  } else {
    Serial.print("Error reading from ThingSpeak. Status: ");
    Serial.println(statusCode1);
  }

 float food = ThingSpeak.readFloatField(channelID2, 1, readApiKey2);
 float water = ThingSpeak.readFloatField(channelID2, 2, readApiKey2);
  int statusCode2 = ThingSpeak.getLastReadStatus();
 String foodS = String(food)+"g";
 String waterS = String(water)+"g";
 const char* FOOD = foodS.c_str();
 const char* WATER = waterS.c_str();
if (statusCode2 == 200) {
    Serial.print("Last food reading: ");
    Serial.print(food);
    Serial.println("g");
    Serial.print("Last water reading: ");
    Serial.print(water);
    Serial.println("g");
  } else {
    Serial.print("Error reading from ThingSpeak. Status: ");
    Serial.println(statusCode2);
  }
  float airtemp = ThingSpeak.readFloatField(channelID3, 1, readApiKey3);
  float humidity = ThingSpeak.readFloatField(channelID3, 2, readApiKey3);
  int statusCode3 = ThingSpeak.getLastReadStatus();
  String airtempS = String(airtemp)+"C";
  String humidityS = String(humidity)+"%";
  const char* AirT = airtempS.c_str();
  const char* AirH = humidityS.c_str();
  if (statusCode3 == 200) {
    Serial.print("Last airtemp reading: ");
    Serial.print(airtemp);
    Serial.println("C");
    Serial.print("Last humidity reading: ");
    Serial.print(humidity);
    Serial.println("%");
  } else {
    Serial.print("Error reading from ThingSpeak. Status: ");
    Serial.println(statusCode3);
  }


  float PH = ThingSpeak.readFloatField(channelID4, 3, readApiKey4);
  int statusCode4 = ThingSpeak.getLastReadStatus();
  String PHs = String(PH);
  const char* PHvalue = PHs.c_str();
if (statusCode4 == 200) {
    Serial.print("Last PH reading: ");
    Serial.print(PH);
    
  } else {
    Serial.print("Error reading from ThingSpeak. Status: ");
    Serial.println(statusCode4);
  }


  
  //printf("EPD_4IN2B_V2_test Demo\r\n");
 

  //printf("e-Paper Init and Clear...\r\n");
  EPD_4IN2B_V2_Init();
  EPD_4IN2B_V2_Clear();
  DEV_Delay_ms(500);
  

  //Create a new image cache named IMAGE_BW and fill it with white
  UBYTE *BlackImage, *RYImage; // Red or Yellow
  UWORD Imagesize = ((EPD_4IN2B_V2_WIDTH % 8 == 0) ? (EPD_4IN2B_V2_WIDTH / 8 ) : (EPD_4IN2B_V2_WIDTH / 8 + 1)) * EPD_4IN2B_V2_HEIGHT;
  if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
    printf("Failed to apply for black memory...\r\n");
    while(1);
  }
  if ((RYImage = (UBYTE *)malloc(Imagesize)) == NULL) {
    printf("Failed to apply for red memory...\r\n");
    while(1);
  }
  //printf("NewImage:BlackImage and RYImage\r\n");
  Paint_NewImage(BlackImage, EPD_4IN2B_V2_WIDTH, EPD_4IN2B_V2_HEIGHT, 180, WHITE);
  Paint_NewImage(RYImage, EPD_4IN2B_V2_WIDTH, EPD_4IN2B_V2_HEIGHT, 180, WHITE);

  //Select Image
  //Paint_SelectImage(BlackImage);
  //Paint_Clear(WHITE);
  //Paint_SelectImage(RYImage);
  //Paint_Clear(WHITE);

  /*
  #if 1   // show image for array    
  printf("show image for array\r\n");
  EPD_4IN2B_V2_Display(gImage_4in2bc_b, gImage_4in2bc_ry);
  DEV_Delay_ms(2000);
  #endif
  */

  #if 1   // Drawing on the image
  /*Horizontal screen*/
  //1.Draw black image
  //printf("Draw black image\r\n");
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);

  //Paint_DrawNum(10, 50, 987654321, &Font16, WHITE, BLACK);

  Paint_DrawString_EN(135, 0, "Turtlopia", &Font20, WHITE, BLACK);
  Paint_DrawLine(0, 20, 400, 20, BLACK, DOT_PIXEL_2X2, LINE_STYLE_DOTTED);

    //Tank or other suggestion and remainder
    //Paint_DrawString_EN(0, 258, "Tank Cleaning:", &Font16, WHITE, BLACK);
    //Water info
    Paint_DrawString_EN(0, 25, "Water Temperture:", &Font16, WHITE, BLACK);
    Paint_DrawString_EN(187, 25, watertemp, &Font16, WHITE, BLACK);  //Default
    Paint_DrawString_EN(0, 45, "Water PH:", &Font16, WHITE, BLACK);
    Paint_DrawString_EN(100, 45, PHvalue, &Font16, WHITE, BLACK);  //Default
    //Air info
    Paint_DrawString_EN(0, 65, "Air Humidity:", &Font16, WHITE, BLACK);
    Paint_DrawString_EN(145, 65, AirH , &Font16, WHITE, BLACK); //Default
    Paint_DrawString_EN(0, 85, "Air Temperature:", &Font16, WHITE, BLACK);
    Paint_DrawString_EN(175, 85, AirT, &Font16, WHITE, BLACK); //Default
    //Weight and feeding monitor
    //Paint_DrawString_EN(0, 105, "Weight:", &Font16, WHITE, BLACK);
    //Paint_DrawString_EN(75, 105, "1kg", &Font16, WHITE, BLACK); //Default
    Paint_DrawString_EN(10, 110, "Feeding:", &Font16, WHITE, BLACK);
    Paint_DrawString_EN(20, 153, FOOD, &Font16, WHITE, BLACK); //Default
    Paint_DrawString_EN(135, 110, "Water:", &Font16, WHITE, BLACK);
    Paint_DrawString_EN(140, 153, WATER, &Font16, WHITE, BLACK); //Default

    Paint_DrawLine(260, 20, 260, 195, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(0, 195, 400, 195, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawString_EN(0, 200, "Suggestion:", &Font16, WHITE, BLACK);
    
    
    
    //Pet info
    Paint_DrawString_EN(265, 25, "Your Turtle:", &Font16, WHITE, BLACK);
    Paint_DrawCircle(325, 70, 15, BLACK,DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(325, 110, 25, BLACK,DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(295, 100, 9, BLACK,DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(355, 100, 9, BLACK,DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(305, 135, 9, BLACK,DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(345, 135, 9, BLACK,DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawString_EN(265, 155, "Name:", &Font16, WHITE, BLACK);
    const char* name = CleanedName.c_str();
    Paint_DrawString_EN(290, 170, name, &Font20, WHITE, BLACK);
    
    
  //2.Draw red image
  //printf("Draw red image\r\n");
  Paint_SelectImage(RYImage);
  Paint_Clear(WHITE);
  Paint_DrawLine(10, 135, 10, 190, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
  Paint_DrawLine(110, 135, 110, 190, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
  Paint_DrawLine(10, 190, 110, 190, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
  //water
  Paint_DrawLine(130, 135, 130, 190, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
  Paint_DrawLine(230, 135, 230, 190, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
  Paint_DrawLine(130, 190, 230, 190, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);

  //Paint_DrawString_EN(0, 275, "RAMINDER:NEED TO CLEAN", &Font24, WHITE, BLACK);
  if ((WATERtemperature<=27)&&(WATERtemperature>=29)){
      Paint_DrawString_EN(0, 220, "Water temperature now is good!", &Font20, WHITE, BLACK);
  }
  else if (WATERtemperature<27){
    Paint_DrawString_EN(0, 220, "Water temperature too low!", &Font20, WHITE, BLACK);
  }
  else{
  Paint_DrawString_EN(0, 220, "Water temperature too high!", &Font20, WHITE, BLACK);
  }

  if (PH>=6.5){
    if (PH<=8){
      Paint_DrawString_EN(0, 240, "Water PH now is good!", &Font20, WHITE, BLACK);
    }
    else{
      Paint_DrawString_EN(0, 240, "Water PH too high!", &Font20, WHITE, BLACK);
    }
  }
  else{
    Paint_DrawString_EN(0, 240, "Water PH too low!", &Font20, WHITE, BLACK);
  }

  if (airtemp >= 26){
    if(airtemp <=30){
      Paint_DrawString_EN(0, 260, "Air temperature now is good!", &Font20, WHITE, BLACK);
    }
    else{
      Paint_DrawString_EN(0, 260, "Air temperature too high!", &Font20, WHITE, BLACK);
    }
  }
  else{
    Paint_DrawString_EN(0, 260, "Air temperature too low!", &Font20, WHITE, BLACK);
  }

  if (humidity>=40){
    if (humidity<=60){
      Paint_DrawString_EN(0, 280, "Air humidity now is good!", &Font20, WHITE, BLACK);
    }
    else{
      Paint_DrawString_EN(0, 280, "Air humidity too high!", &Font20, WHITE, BLACK);
    }
  }
  else{
    Paint_DrawString_EN(0, 280, "Air humidity too low!", &Font20, WHITE, BLACK);
  }


  //printf("EPD_Display\r\n");
  EPD_4IN2B_V2_Display(BlackImage, RYImage);
  DEV_Delay_ms(2000);
  #endif

  //printf("Clear...\r\n");
  //EPD_4IN2B_V2_Clear();

  //printf("Goto Sleep...\r\n");
  EPD_4IN2B_V2_Sleep();
  free(BlackImage);
  free(RYImage);
  BlackImage = NULL;
  RYImage = NULL;
  delay(100000);
  //DEV_Delay_ms(10000);
}



