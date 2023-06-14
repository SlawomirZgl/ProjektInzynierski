/*
  The LCD circuit:
 * LCD RS pin to digital pin 12
 * LCD Enable pin to digital pin 11
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD R/W pin to ground
 * LCD VSS pin to ground
 * LCD VCC pin to 5V
 * 10K resistor:
 * ends to +5V and ground
 * wiper to LCD VO pin (pin 3)
*/

#include <LiquidCrystal.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <math.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>

// Piny używane przez wyświetlacz LCD
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Piny używane przez czujnik temperatury i wilgotności DHT 11
#define DHTPIN 7
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define BUTTON_PIN_ENTER 8
#define BUTTON_PIN_UP 13
#define BUTTON_PIN_DOWN 6

// Piny używane przez moduł GPS
static const int RXPin = 9, TXPin = 10;
static const uint32_t GPSBaud = 9600;
// Obiekt biblioteki TinyGPS++ 
TinyGPSPlus gps;
// Połączenie szeregowe z modułem GPS
SoftwareSerial ss(TXPin, RXPin);

Adafruit_BMP280 bmp280; 

void printLCDLine(const char * text, int lineNumber, int charNumber = 0);
void preFlightProcedure();

void setup() {
  Serial.begin(9600);
    
  // Ustawienie liczby wierszy oraz kolumn wyświetlacza LCD
  lcd.begin(16, 2);
  lcd.noCursor();
  
  // Zainicjowanie czujnkia DHT 11
  dht.begin();

  // Zainicjowanie czujnika bmp280
  if (!bmp280.begin(0x76)) 
  {
    Serial.println("Nie znaleziono czujnika - sprawdź połączenie!");
    while(true);
  }

  // Zainicjowanie przycisków
  pinMode(BUTTON_PIN_ENTER, INPUT_PULLUP);
  pinMode(BUTTON_PIN_UP, INPUT_PULLUP);
  pinMode(BUTTON_PIN_DOWN, INPUT_PULLUP);
  
  // Zainicjowanie połączenia szeregowego z GPS
  ss.begin(GPSBaud);
}

int buttonStateENTER = 0;
int buttonStateUP = 0;
int buttonStateDOWN = 0;
bool changePage = false;
double base_pressure = 1013.25;
String gpsTime = "00:00";

float temperature; 
float pressure;    
float altitude_;

float hum;
float temp;

double gpsDirection = 0.0;
double gpsVelocity = 0.0;
double gpsHeigth = 0.0;

bool pressureSet = false;

constexpr double METER_IN_FEET = 3.2808399;

void loop() {
  preFlightProcedure();
  readButtons(); 

  if (buttonStateENTER == LOW || buttonStateUP == LOW || buttonStateDOWN == LOW)
  {
    delay(250);
    changePage = !changePage;
    lcd.clear();
  }

  if(changePage){
    char line[17];
    snprintf(line, sizeof(line), "T:%i%cC wilg:%i%% ", static_cast<int>(temperature),static_cast<char>(223), static_cast<int>(hum));
    printLCDLine(line, 0);
    
    snprintf(line, sizeof(line), "cisn:%ihPa ", static_cast<int>(pressure));
    printLCDLine(line, 1);
  }
  else {
    char line[17];
    snprintf(line, sizeof(line), "ALT:%ift  ",  static_cast<int>(altitude_));
    printLCDLine(line, 0);

    char t[6];
    gpsTime.toCharArray(t, sizeof(t));
    printLCDLine(t, 0, 11);

    snprintf(line, sizeof(line), "V:%ikm/h  ", static_cast<int>(gpsVelocity));
    printLCDLine(line, 1, 0);
    
    snprintf(line, sizeof(line), "HDG:%i  ", static_cast<int>(gpsDirection));
    printLCDLine(line, 1, 9);
  }
  
  bool sent = false;
  // Odczyt danych z modułu GPS
  while (ss.available() > 0)
  {
    if (gps.encode(ss.read()) && !sent)
    {
      temperature = bmp280.readTemperature(); 
      pressure    = bmp280.readPressure() / 100;    
      altitude_   = bmp280.readAltitude(base_pressure) * METER_IN_FEET;
      hum = dht.readHumidity();
      temp = dht.readTemperature();

      gpsTime = getTime();
      gpsHeigth = gps.altitude.feet();
      gpsVelocity = gps.speed.kmph();
      gpsDirection = gps.course.deg();
      printInfoOnSerial();
      sent = true;
    }
  }
}

void readButtons()
{
  buttonStateENTER = digitalRead(BUTTON_PIN_ENTER);
  buttonStateUP = digitalRead(BUTTON_PIN_UP);
  buttonStateDOWN = digitalRead(BUTTON_PIN_DOWN);
}

void preFlightProcedure()
{
  while(!pressureSet)
  {
    readButtons();
    char line[17];
    snprintf(line, sizeof(line), "Wprowadz QNH:");
    printLCDLine(line, 0);
    snprintf(line, sizeof(line), "QNH:%ihPa  ", 
    static_cast<int>(base_pressure));
    printLCDLine(line, 1);
    delay(150);
    
    if (buttonStateENTER == LOW)
    {
      snprintf(line, sizeof(line), "Czy potwierdzasz");
      printLCDLine(line, 0);
      snprintf(line, sizeof(line), "QNH:%ihPa?",
      static_cast<int>(base_pressure));
      printLCDLine(line, 1);
      delay(500);
      while (true)
      {
        readButtons();
        delay(150);
        if (buttonStateENTER == LOW)
        {
          pressureSet = true;
          break;
        }
        if (buttonStateUP == LOW)
        {
          base_pressure++;
          break;
        }
        if (buttonStateDOWN == LOW)
        {
          base_pressure--;
          break;
        }
      }
      lcd.clear();
      continue;
    }
    if (buttonStateUP == LOW)
    {
      base_pressure++;
      continue;
    }
    if (buttonStateDOWN == LOW)
    {
      base_pressure--;
      continue;
    }
  }
}

void printInfoOnSerial()
{
  gpsInfo();
  barometricInfo();
  humidityInfo();
  Serial.println();
}

void printLCDLine(const char * text, int lineNumber, int charNumber)
{
    lcd.setCursor(charNumber, lineNumber);
    lcd.print(text);
}

String getTime()
{
  char timeRead[6];

  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10)
    {
      if (gps.time.minute() < 10)
      {
        snprintf(timeRead, sizeof(timeRead), "0%i:0%i", gps.time.hour(), gps.time.minute());
      }
      else
      {
        snprintf(timeRead, sizeof(timeRead), "0%i:%i", gps.time.hour(), gps.time.minute()); 
      }
    }
    else
    {
      if (gps.time.minute() < 10)
      {
        snprintf(timeRead, sizeof(timeRead), "%i:0%i", gps.time.hour(), gps.time.minute());
      }
      else
      {
        snprintf(timeRead, sizeof(timeRead), "%i:%i", gps.time.hour(), gps.time.minute()); 
      }
    }
  }
  else
  {
    snprintf(timeRead, sizeof(timeRead), "ERROR");
  }
       
  return timeRead;
}

void gpsInfo()
{
  // Date/Time;
  printDate();
  Serial.print(F(" "));
  printTime();
  Serial.print(F(";"));

  // Location
  printLocation();
  Serial.print(F(";"));

  Serial.print(gpsHeigth, 1);
  Serial.print(F(";"));

  Serial.print(gpsDirection);
  Serial.print(F(";"));

  Serial.print(gpsVelocity);
  Serial.print(F(";"));
  
}

void printDate()
{
  if (gps.date.isValid())
  {
    if (gps.date.value() == 0)
    {
      Serial.print(F("2000-1-1"));
      return;
    }

    Serial.print(gps.date.year());
    Serial.print(F("-"));
    Serial.print(gps.date.month());
    Serial.print(F("-"));
    Serial.print(gps.date.day());
  }
  else
  {
    Serial.print(F("2000-1-1"));
  } 
}

void printTime()
{
  // HH:mm::ss.cc
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
//    Serial.print(F("."));
//    if (gps.time.centisecond() < 10) Serial.print(F("0"));
//    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("00:00:00.00"));
  }
}

void printLocation()
{
  if (gps.location.isValid())
  {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(";"));
    Serial.print(gps.location.lng(), 6);
  }
  else
  {
    Serial.print(F("0;0"));
  }
}

void barometricInfo()
{
  Serial.print(temperature, 1);
  Serial.print(F(";"));
  Serial.print(pressure);
  Serial.print(F(";"));
  Serial.print(altitude_);
  Serial.print(F(";"));
}

void humidityInfo()
{
  Serial.print(temp, 1);
  Serial.print(F(";"));
  Serial.print(hum, 1);
  Serial.print(F(";"));
}
