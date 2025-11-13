#include "ThingSpeak.h"

#define pH_SENSOR_PIN 11 // 定義GPIO腳位為GPIO34
#define VREF 3.3 // ESP32輸入電壓為3.3V
#define SCOUNT 30 // 取樣數
int analogBuffer[SCOUNT];
int analogBufferIndex = 0;
float averageVoltage = 0;
float pHValue = 0;
float pHslope = -4.8475;
float pHintercept = 18.30;
  
void ph_setup()
{
  Serial.begin(115200);
  pinMode(pH_SENSOR_PIN, INPUT);
}
  
float lastpHValue = -1; // 定義最後的pH值為無效(-1)
  
void ph_loop()
{
  // pH取樣

  analogBuffer[analogBufferIndex] = analogRead(pH_SENSOR_PIN);
  analogBufferIndex++;
  if(analogBufferIndex == SCOUNT)
  { 
    analogBufferIndex = 0;
  }
    
  // pH計算

  float sum = 0;
  for(int i = 0; i < SCOUNT; i++)
  {
    sum += analogBuffer[i];
  }
  averageVoltage = (sum / SCOUNT) * (float)VREF / 4095; // 轉換為電壓值，ESP32的ADC解析度是12位(4095)。

  pHValue = averageVoltage * pHslope + pHintercept;
    
  if (pHValue != lastpHValue) // 如果pH數值有更新，才會將結果Print出來。
  {
    lastpHValue = pHValue;
    Serial.print("pH Value: ");
    Serial.println(pHValue, 2);
  }
  ThingSpeak.setField(3, pHValue);

}