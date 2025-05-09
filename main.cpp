#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ESP8266httpUpdate.h>

const char* serverName = "https://script.google.com/macros/s/AKfycbw2o1bYFKg2MbuxucKNmnp7w_U49WAVnk8kARVGVCN2-OLjVRedp3skCbesEcM5-ug-/exec";  // your actual script URL


const int MotorPin = 14;
const int ValvePin = 10;
const int sensorPin = 5;
bool ValveStatus =HIGH;
int count = 0;
const int maxCycles = 100;
unsigned long lastTrigger = 0;
const unsigned long interval = 60000; // 1 minute

const String FirmwareVer={"1.2"}; 

#define URL_fw_Bin "https://raw.githubusercontent.com/heman728/Valve_Testing_FW/main/firmware.bin"
const char* fw_host = "https://raw.githubusercontent.com/heman728/Valve_Testing_FW/main/version.txt";
String payload="";

const char* ssids[] = {
  "DESIGNLAB",
  "Redmi Note",
  "Redmi"
};

const char* passwords[] = {
  "design@2023",
  "manishpatel",
  "hetalpatel"
};

const int numNetworks = sizeof(ssids) / sizeof(ssids[0]);
int currentNetwork = 0;
unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 30000; // 30 seconds

void connectToWiFi() {
  Serial.print("\nAttempting to connect to: ");
  Serial.println(ssids[currentNetwork]);
  
  WiFi.begin(ssids[currentNetwork], passwords[currentNetwork]);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nConnection failed");
    WiFi.disconnect();
    currentNetwork = (currentNetwork + 1) % numNetworks; // Move to next network
  }
}


void FirmwareUpdate()
{ Serial.print("Current Firmware version :");
  Serial.println(FirmwareVer);
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); //the magic line, use with caution
  client.connect(fw_host, 443);
  http.begin(client, fw_host); // Read Status from Cloud
  int httpCode = http.GET();
  Serial.print("httpCode: ");
  Serial.println(httpCode);
     if (httpCode == HTTP_CODE_OK) {
        payload = http.getString();
        Serial.print("Github Version: ");
        Serial.println(payload);
      }


  payload.trim();
  if(payload.equals(FirmwareVer) )
  {   
     Serial.println("Device already on latest firmware version"); 
  }
  else
  {

    Serial.println("New firmware detected");
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW); 
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, URL_fw_Bin);
        
    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    } 
  }
 }  


void setup() {
  Serial.begin(9600);
  pinMode(MotorPin, OUTPUT);
  pinMode(ValvePin, OUTPUT);
  pinMode(sensorPin, INPUT);
  digitalWrite(MotorPin, LOW);
  digitalWrite(ValvePin, LOW);

  connectToWiFi();
  FirmwareUpdate();
  
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastReconnectAttempt >= reconnectInterval) {
      lastReconnectAttempt = currentMillis;
      connectToWiFi();
    }
  }

  if (count < maxCycles && millis() - lastTrigger >= interval) {
    lastTrigger = millis();
    count++;
    ValveStatus = !ValveStatus;
    digitalWrite(ValvePin, ValveStatus);

    delay(2000);
    digitalWrite(MotorPin, HIGH);

    // 5-second delay before sensor monitoring
    delay(5000);

    int alert = 0;
    unsigned long startTime = millis();

    // Monitor sensor for 15 seconds
    while (millis() - startTime < 15000) {
      int sensorState = digitalRead(sensorPin);
      if (sensorState != ValveStatus) {
        alert = 1;
        break;
      }
      delay(100);  // short delay to reduce CPU usage
    }

    if (alert == 0) {
      delay(2000);
      digitalWrite(MotorPin, LOW);
      delay(2000);
      digitalWrite(ValvePin, LOW);
    }

    // Always post data, whether alert or not
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClientSecure client;
      client.setInsecure(); // skip SSL cert validation

      HTTPClient http;
      http.begin(client, serverName);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

      String postData = "motor=ON&valve=" + String(ValveStatus) + "&sensor=" + String(digitalRead(sensorPin)) + "&count=" + String(count) + "&alert=" + String(alert);
      int httpCode = http.POST(postData);
      String response = http.getString();

      Serial.printf("Cycle %d: Motor ON , Valve %d -> Sensor: %d | HTTP Code: %d\nResponse: %s\n", count, ValveStatus, digitalRead(sensorPin), httpCode, response.c_str());
      http.end();
    }

    if (alert == 1) {
      // Wait for button press on pin 4 (pulled low by default, pressed = LOW)
      while (digitalRead(4) != LOW) {
        delay(10);  // debounce wait
      }
    
      delay(2000);
      digitalWrite(MotorPin, LOW);
      delay(2000);
      digitalWrite(ValvePin, LOW);
      lastTrigger = millis();
    }
  }
}

