#include <Wire.h>
#include <BH1750.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <ThingSpeak.h>
#include "secrets.h"  // Keep this file private; do not upload it to GitHub.

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

unsigned long channelID = THINGSPEAK_CHANNEL_ID;
const char* apiKey = THINGSPEAK_WRITE_API_KEY;

WiFiClient client;

// =====================================================
//                    PIN CONFIGURATION
// =====================================================

#define LDR1_PIN       36
#define LDR2_PIN       33
#define LDR3_PIN       39

#define DS18B20_PIN    15

#define DHT_PIN        32
#define DHT_TYPE       DHT11

#define VOLTAGE_PIN    34
#define CURRENT_PIN    35

#define SDA_PIN        21
#define SCL_PIN        22

#define MOTOR_IN1      25
#define MOTOR_IN2      26

#define CAMERA_TRIGGER_PIN 4

// =====================================================
//                    OBJECTS
// =====================================================

BH1750 lightMeter;

OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);

DHT dht(DHT_PIN, DHT_TYPE);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================================
//                 SENSOR CALIBRATION
// =====================================================

float voltageRatio = 5.0;

float currentSensitivity = 0.100;

float currentZero = 2.50;


// =====================================================
//                    SETTINGS
// =====================================================

#define LIGHT_THRESHOLD 1500
#define ROTATE_40_MS 875


// =====================================================
//                    VARIABLES
// =====================================================

float panelVoltage = 0;
float panelCurrent = 0;
float panelPower = 0;

float panelTemperature = 0;

float ambientTemperature = 0;
float humidity = 0;

float lightLux = 0;

int ldr1 = 0;
int ldr2 = 0;
int ldr3 = 0;

float averageLDR = 0;


// =====================================================
//                      SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  ThingSpeak.begin(client);
  Serial.println("ThingSpeak Started");

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // Sensors
  ds18b20.begin();
  dht.begin();

  // BH1750
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE))
  {
    Serial.println("BH1750 OK");
  }
  else
  {
    Serial.println("BH1750 ERROR");
  }

  // LCD
  lcd.init();
  lcd.backlight();

  // Motor Driver
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);

  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);

  // Camera Trigger
  pinMode(CAMERA_TRIGGER_PIN, OUTPUT);
  digitalWrite(CAMERA_TRIGGER_PIN, HIGH);

  Serial.println("Camera Trigger: ON");

  // Startup display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SOLAR PANEL");
  lcd.setCursor(0, 1);
  lcd.print("MONITORING");

  delay(2000);

  lcd.clear();
  lcd.print("SYSTEM READY");

  delay(1500);
}


// =====================================================
//                  THINGSPEAK UPDATE
// =====================================================

void updateThingSpeak()
{
  ThingSpeak.setField(1, panelVoltage);
  ThingSpeak.setField(2, panelCurrent);
  ThingSpeak.setField(3, panelPower);
  ThingSpeak.setField(4, panelTemperature);
  ThingSpeak.setField(5, ambientTemperature);
  ThingSpeak.setField(6, humidity);
  ThingSpeak.setField(7, lightLux);

  int response = ThingSpeak.writeFields(channelID, apiKey);

  if (response == 200)
  {
    Serial.println("ThingSpeak Update: SUCCESS");
  }
  else
  {
    Serial.print("ThingSpeak Error: ");
    Serial.println(response);
  }
}


// =====================================================
//                  READ DS18B20
// =====================================================

void readDS18B20()
{
  ds18b20.requestTemperatures();

  panelTemperature =
    ds18b20.getTempCByIndex(0);

  if (panelTemperature == DEVICE_DISCONNECTED_C)
  {
    panelTemperature = -127;
  }
}


// =====================================================
//                    READ BH1750
// =====================================================

void readBH1750()
{
  lightLux = lightMeter.readLightLevel();

  if (lightLux < 0)
    lightLux = 0;
}


// =====================================================
//                     READ DHT11
// =====================================================

void readDHT11()
{
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t))
    ambientTemperature = t;

  if (!isnan(h))
    humidity = h;
}


// =====================================================
//                      READ LDR
// =====================================================

void readLDR()
{
  ldr1 = analogRead(LDR1_PIN);
  ldr2 = analogRead(LDR2_PIN);
  ldr3 = analogRead(LDR3_PIN);

  averageLDR =
    (ldr1 + ldr2 + ldr3) / 3.0;
}


// =====================================================
//                 READ PANEL VOLTAGE
// =====================================================

void readVoltage()
{
  int raw = analogRead(VOLTAGE_PIN);

  float sensorVoltage =
    (raw / 4095.0) * 3.3;

  panelVoltage =
    sensorVoltage * voltageRatio;
}


// =====================================================
//                 READ PANEL CURRENT
// =====================================================

void readCurrent()
{
  int raw = analogRead(CURRENT_PIN);

  float sensorVoltage =
    (raw / 4095.0) * 3.3;

  panelCurrent =
    (sensorVoltage - currentZero)
    / currentSensitivity;

  // Remove small noise
  if (panelCurrent < 0)
    panelCurrent = 0;
}


// =====================================================
//                 CALCULATE POWER
// =====================================================

void calculatePower()
{
  panelPower =
    panelVoltage * panelCurrent;
}


// =====================================================
//                LDR DIRECTION
// =====================================================

String getDirection()
{
  int rightLight  = ldr1;   // GPIO36 = RIGHT
  int centerLight = ldr2;   // GPIO33 = CENTER
  int leftLight   = ldr3;   // GPIO39 = LEFT

  if(leftLight < LIGHT_THRESHOLD &&
     leftLight < centerLight &&
     leftLight < rightLight)
  {
    return "LEFT";
  }

  if(centerLight < LIGHT_THRESHOLD &&
     centerLight < leftLight &&
     centerLight < rightLight)
  {
    return "CENTER";
  }

  if(rightLight < LIGHT_THRESHOLD &&
     rightLight < centerLight &&
     rightLight < leftLight)
  {
    return "RIGHT";
  }

  return "NO LIGHT";
}


// =====================================================
//                 MOTOR CONTROL
// =====================================================

int currentPosition = 0;

// -1 = LEFT
//  0 = CENTER
// +1 = RIGHT


void stopMotor()
{
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
}


void motorLeft(int timeMs)
{
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);

  delay(timeMs);

  stopMotor();
}


void motorRight(int timeMs)
{
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, HIGH);

  delay(timeMs);

  stopMotor();
}


void goLeft()
{
  if(currentPosition == 0)
  {
    motorLeft(ROTATE_40_MS);
  }
  else if(currentPosition == 1)
  {
    motorLeft(ROTATE_40_MS * 2);
  }

  currentPosition = -1;
}


void goCenter()
{
  if(currentPosition == -1)
  {
    motorRight(ROTATE_40_MS);
  }
  else if(currentPosition == 1)
  {
    motorLeft(ROTATE_40_MS);
  }

  currentPosition = 0;
}


void goRight()
{
  if(currentPosition == 0)
  {
    motorRight(ROTATE_40_MS);
  }
  else if(currentPosition == -1)
  {
    motorRight(ROTATE_40_MS * 2);
  }

  currentPosition = 1;
}


// =====================================================
//                    SUN TRACKING
// =====================================================

void trackSun()
{
  int rightLight  = ldr1;   // GPIO36 = RIGHT
  int centerLight = ldr2;   // GPIO33 = CENTER
  int leftLight   = ldr3;   // GPIO39 = LEFT


  bool leftDetected   = (leftLight < LIGHT_THRESHOLD);
  bool centerDetected = (centerLight < LIGHT_THRESHOLD);
  bool rightDetected  = (rightLight < LIGHT_THRESHOLD);


  Serial.println("---- LDR TRACKING ----");

  Serial.print("LEFT   : ");
  Serial.println(leftLight);

  Serial.print("CENTER : ");
  Serial.println(centerLight);

  Serial.print("RIGHT  : ");
  Serial.println(rightLight);


  // Only LEFT detects light

  if(leftDetected && !centerDetected && !rightDetected)
  {
    Serial.println("LIGHT -> LEFT");
    goLeft();
  }


  // Only RIGHT detects light

  else if(rightDetected && !centerDetected && !leftDetected)
  {
    Serial.println("LIGHT -> RIGHT");
    goRight();
  }


  // CENTER detects light

  else if(centerDetected)
  {
    Serial.println("LIGHT -> CENTER");
    goCenter();
  }


  // LEFT + RIGHT both detect

  else if(leftDetected && rightDetected)
  {
    Serial.println("LEFT + RIGHT -> STOP");
    stopMotor();
  }


  // No LDR detects

  else
  {
    Serial.println("NO LIGHT -> STOP");
    stopMotor();
  }
}


// =====================================================
//                    SERIAL MONITOR
// =====================================================

void printSerial()
{
  Serial.println();
  Serial.println("========== SOLAR DATA ==========");

  Serial.print("Voltage       : ");
  Serial.print(panelVoltage, 2);
  Serial.println(" V");

  Serial.print("Current       : ");
  Serial.print(panelCurrent, 2);
  Serial.println(" A");

  Serial.print("Power         : ");
  Serial.print(panelPower, 2);
  Serial.println(" W");

  Serial.print("BH1750        : ");
  Serial.print(lightLux, 1);
  Serial.println(" Lux");

  Serial.print("Panel Temp    : ");
  Serial.print(panelTemperature, 1);
  Serial.println(" C");

  Serial.print("DHT Temp      : ");
  Serial.print(ambientTemperature, 1);
  Serial.println(" C");

  Serial.print("Humidity      : ");
  Serial.print(humidity, 1);
  Serial.println(" %");

  Serial.print("LDR1          : ");
  Serial.println(ldr1);

  Serial.print("LDR2          : ");
  Serial.println(ldr2);

  Serial.print("LDR3          : ");
  Serial.println(ldr3);

  Serial.println("================================");
}


// =====================================================
//                     LCD 1
// =====================================================

void lcdScreen1()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("V:");
  lcd.print(panelVoltage, 1);
  lcd.print(" I:");
  lcd.print(panelCurrent, 1);

  lcd.setCursor(0, 1);
  lcd.print("Power:");
  lcd.print(panelPower, 1);
  lcd.print("W");

  delay(2000);
}


// =====================================================
//                     LCD 2
// =====================================================

void lcdScreen2()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Panel Temp:");
  lcd.print(panelTemperature, 1);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Panel Lig:");
  lcd.print(lightLux, 0);
  lcd.print("Lx");

  delay(2000);
}


// =====================================================
//                     LCD 3
// =====================================================

void lcdScreen3()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Env Temp:");
  lcd.print(ambientTemperature, 1);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("Env Hum:");
  lcd.print(humidity, 0);
  lcd.print("%");

  delay(2000);
}


// =====================================================
//                     LCD 4
// =====================================================

void lcdScreen4()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("L1:");
  lcd.print(ldr1);
  lcd.print(" L2:");
  lcd.print(ldr2);

  lcd.setCursor(0, 1);
  lcd.print("L3:");
  lcd.print(ldr3);

  delay(2000);
}


// =====================================================
//                     LCD 5
// =====================================================

void lcdScreen5()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Direction:");

  lcd.setCursor(0, 1);
  lcd.print(getDirection());

  delay(2000);
}


// =====================================================
//                       LOOP
// =====================================================

void loop()
{
  // Read sensors

  readDS18B20();

  readBH1750();

  readDHT11();

  readLDR();

  trackSun();

  readVoltage();

  readCurrent();


  // Calculate panel output

  calculatePower();


  // Serial monitor

  printSerial();


  // LCD

  lcdScreen1();

  lcdScreen2();

  lcdScreen3();

  lcdScreen4();

  lcdScreen5();


  // ThingSpeak

  updateThingSpeak();
}