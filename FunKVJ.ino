#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <time.h>
#include <HTTPClient.h>
#include <NTPClient.h>

const char* ntpServer = "1.fi.pool.ntp.org";
const long timeZoneOffset = 10800;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, timeZoneOffset);
unsigned long lastMillis = 0;
const unsigned long interval = 10000;

const char* SSID = "";
const char* PASS = "";
const char* FUN = "https://funoulutalvikangas.fi/?controller=ajax&getentriescount=1&locationId=1";
const char* WILLAB = "https://weather.willab.fi/weather.json";


const int dataPin = 13;
const int clkPin = 19;
const int loadPin = 25;

byte bcd[] = {
  0b01111110, //0
  0b00110000, //1
  0b01101101, //2
  0b01111001, //3
  0b00110011, //4
  0b01011011, //5
  0b01011111, //6
  0b01110000, //7
  0b01111111, //8
  0b01111011, //9
  0b00000001, //-
  0b01100011  //°
  };

void sendData(byte addr, byte reg) {
  digitalWrite(loadPin, LOW); //loadPin must start low

  for (int i = 0; i < 8; i++) {
    digitalWrite(dataPin, addr & 0b10000000);
    digitalWrite(clkPin, HIGH);
    digitalWrite(clkPin, LOW);
    addr = addr << 1;
  }

  for (int i = 0; i < 8; i++) {
    digitalWrite(dataPin, reg & 0b10000000);
    digitalWrite(clkPin, HIGH);
    digitalWrite(clkPin, LOW);
    reg = reg << 1;
  }

  digitalWrite(loadPin, HIGH);  //loadPin must end high
}

float read(String url, String field){
  
  HTTPClient http;
  String response;

  http.begin(url);
  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_OK) {
    String response = http.getString();

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return 0;
    }
    return doc[field];

  } else {
    Serial.print("Error: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

void setup() {

  pinMode(13, OUTPUT);
  pinMode(19, OUTPUT);
  pinMode(25, OUTPUT);

  sendData(0x0C, 0x01); //wake up
  delay(1);
  sendData(0x0B, 0x07); //scan limit
  sendData(0x09, 0x00); //no decode mode
  sendData(0x0A, 0x00); //intensity

  for (int i = 1; i <= 8; i++) {
    sendData(i, 128);
  }

  Serial.begin(115200);
  while (!Serial);

  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  timeClient.begin();  
}

void loop() {
  int visitors;
  int temperature;

  timeClient.update();
  time_t currentTime = timeClient.getEpochTime();

  struct tm *localTime = localtime(&currentTime);
  if (millis() - lastMillis >= interval) {
    lastMillis = millis();
    int visitors = round(read(FUN, "entriesTotal"));
    int temperature = round(read(WILLAB, "tempnow"));
    if (visitors > 999){
      sendData(0x1, bcd[visitors / 1000]);
    } else {
      sendData(0x1,0);
    }
    if (visitors > 99){
      sendData(0x2, bcd[(visitors / 100) % 10]);
    } else {
      sendData(0x2,0);
    }
    if (visitors > 9){
      sendData(0x3, bcd[(visitors / 10) % 10]);
    } else {
      sendData(0x3,0);
    }
    sendData(0x04, bcd[visitors%10]);

    sendData(0x05, bcd[localTime->tm_hour / 10]);
    sendData(0x06, bcd[(localTime->tm_hour % 10)] + 128); //+128 for decimalpoint
    sendData(0x07, bcd[localTime->tm_min / 10]);
    sendData(0x08, bcd[localTime->tm_min % 10]);

    delay(5000);

    sendData(1, 0);
    sendData(2, 0);

    if (temperature < 0) {
      temperature = temperature * -1;
        if(temperature >= 10){
          sendData(1, bcd[10]);
          sendData(2, bcd[temperature]);
        } else {
          sendData(2, bcd[10]);
        }
    }
    if (temperature >= 10) {
      sendData(2, bcd[temperature / 10]);
    }
    sendData(3, bcd[temperature % 10]);
    sendData(4, bcd[11]);
  }

  if (Serial.available()) {
    int receivedInt = Serial.parseInt();
    if (receivedInt >= 0 & receivedInt <= 15) {
      sendData(0x0A, receivedInt);
      Serial.printf("Set brightness to: %d/32\n", receivedInt * 2 + 1);
    }
    else {
      Serial.println("Enter a valid number between 0-15");
    }
  }
}