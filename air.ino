#include "Arduino.h"
#include <Wire.h>
#include "ThingSpeak.h"

#define PWM_PIN 18
#define FORWARD 17
#define BACKWARD 16
#define TEMP_PIN 1
#define SDA_PIN 8
#define SCL_PIN 9


// variable init
float currentTemp;
float targetTemp = 25.0;
float currentHumidity;
float difference;
int pwm = 0;


#define SHT30_ADDR 0x44

// Single-shot, high repeatability, no clock stretching
const uint8_t CMD_MEAS_HIGHREP[2] = {0x2C, 0x06};

bool sht30_read(float &temperatureC, float &humidityRH) {
  Wire.beginTransmission(SHT30_ADDR);
  Wire.write(CMD_MEAS_HIGHREP, 2);
  if (Wire.endTransmission() != 0) {
    return false;
  }

  delay(20); // wait for measurement

  if (Wire.requestFrom(SHT30_ADDR, (uint8_t)6) != 6) {
    return false;
  }

  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = Wire.read();

  uint16_t rawT  = (uint16_t(d[0]) << 8) | d[1];
  uint16_t rawRH = (uint16_t(d[3]) << 8) | d[4];

  temperatureC = -45.0f + 175.0f * (float)rawT / 65535.0f;
  humidityRH   = 100.0f * (float)rawRH / 65535.0f;

  return true;
}


void air_setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(PWM_PIN, OUTPUT);
  pinMode(FORWARD, OUTPUT);
  pinMode(BACKWARD, OUTPUT);
  pinMode(TEMP_PIN, INPUT);
  digitalWrite(FORWARD, HIGH);
  digitalWrite(BACKWARD, HIGH);
  Wire.begin(SDA_PIN, SCL_PIN);

}

void air_loop() {
  // put your main code here, to run repeatedly:
  String command = Serial.readStringUntil('/n');
  if (command.toFloat() != 0) targetTemp = command.toFloat();
  sht30_read(currentTemp,currentHumidity);

  Serial.print("Current (ºC): ");
  Serial.println(currentTemp);
  Serial.print("Target (ºC): ");
  Serial.println(targetTemp);
  difference = targetTemp - currentTemp;
  difference = constrain(difference * 70, -200, 255);

  if (difference > 0) {
    digitalWrite(FORWARD, HIGH);
    digitalWrite(BACKWARD, LOW);
    analogWrite(PWM_PIN, (int) difference);
    Serial.print("Mode: Heating, PWM = ");
    Serial.print((int) ((difference/255)*100));
    Serial.println("%");
  }
  if (difference < 0) {
    digitalWrite(FORWARD, LOW);
    digitalWrite(BACKWARD, HIGH);
    analogWrite(PWM_PIN, -(int) difference);
    Serial.print("Mode: Cooling, PWM = ");
    Serial.print(-(int) ((difference/200)*100));
    Serial.println("%");
  }
  if (difference == 0) {
    analogWrite(PWM_PIN, 0);
    Serial.print("Mode: Idle");
    Serial.println();
    lastTime -= 500;
  }

  Serial.print("Current Humidity: ");
  Serial.print(currentHumidity);
  Serial.println("%");
  ThingSpeak.setField(1, currentTemp);
  ThingSpeak.setField(2, currentHumidity);

}
