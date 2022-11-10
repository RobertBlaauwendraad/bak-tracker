// Display libraries
#include <Wire.h>
#include "SH1106.h"

// Initialize OLED display
SH1106 display(0x3C, D1, D2);

// Weight sensor libraries
#include <Arduino.h>
#include "HX711.h"

// Initialize scale
HX711 scale;

// WiFi libraries
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiManager.h>

// API Libraries
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// Initialize API
#define HTTP_REST_PORT 80
ESP8266WebServer server(HTTP_REST_PORT);

const int weightSensorDT = 12;
const int weightSensorSCK = 16;

enum Status {
    Waiting, Placed, Drinking, Finished, Failed
};
// Initial beerStatus
Status beerStatus = Waiting;

// Global drinking times
unsigned long startDrinkingTime;
unsigned long drinkingTime;

// Weight threshold variables
const int fullBeerThreshold = 300;      // Above this amount we consider a full beer is on the scale
const int emptyBeerThreshold = 200;      // Beneath this amount and above the emptyScaleThreshold we consider an empty beer is on the scale
const int emptyScaleThreshold = 100;     // Beneath this amount we consider the scale empty


// Serving Hello World
void getHelloWord() {
  server.send(200, "text/json", "{\"name\": \"Hello world\"}");
}

// Define routing
void restServerRouting() {
  server.on("/", HTTP_GET, []() {
      server.send(200, F("text/html"),
                  F("Welcome to the REST Web Server"));
  });
  server.on(F("/helloWorld"), HTTP_GET, getHelloWord);
}

void setup() {
  // Set serial monitor baud rate
  Serial.begin(115200);

  // Initialising the UI will init the display too.
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  // Initialize the scale
  scale.begin(weightSensorDT, weightSensorSCK);

  // Calibration factor gained by (measured_weight/actual_weight)
  scale.set_scale(106.46);
  scale.tare();

  WiFiManager wifiManager;
  wifiManager.autoConnect("Bak Tracker");

  restServerRouting();
  server.begin();

  if (MDNS.begin("baktracker")) {
    Serial.println("MDNS responder started");
  }
  MDNS.addService("http", "tcp", 80);
}

bool drinkingLoop() {
  float currentWeight = scale.get_units();
  Serial.println(currentWeight);
  if (currentWeight > emptyScaleThreshold) {   // Something has been placed on the scale
    beerStatus = Finished;
//    if (currentWeight < emptyBeerThreshold) {  // Empty glass has been placed on the scale
//      beerStatus = Finished;
//    } else {                                  // Something heavier than empty glass has been placed on the scale
//      beerStatus = Failed;
//    }
    return false;
  }
  display.clear();
  drinkingTime = millis() - startDrinkingTime;
  display.drawString(64, 22, String(drinkingTime));
  display.display();
  return true;
}

void updateBeerStatus() {
  if (beerStatus == Waiting) {           // Wait until beer is on the scale
    display.drawString(104, 16, "Place your beer!");
    if (scale.get_units() > fullBeerThreshold) {
      beerStatus = Placed;
    }
  } else if (beerStatus == Placed) {    // Beer is on the scale
    display.drawString(90, 16, "Let's start drinking!");
    if (scale.get_units() < emptyScaleThreshold) {
      beerStatus = Drinking;
    }
  } else if (beerStatus == Drinking) {    // Person is drinking the beer
    startDrinkingTime = millis();
    while (drinkingLoop()) {}
  } else if (beerStatus == Finished) {    // Person has finished drinking the beer
    // Display final time
    display.drawString(90, 16, "Finished");
    display.drawString(90, 30, String(drinkingTime));
  } else {
    display.drawString(90, 16, "Failed");
  }
}

void loop() {
  server.handleClient();
  MDNS.update();

//  if (WiFi.status() == WL_CONNECTED) {
//    WiFiClient client;
//    HTTPClient http;
//    String serverPath = "http://192.168.1.22:3000/";
//    http.begin(client, serverPath.c_str());
//    int httpResponseCode = http.GET();
//
//    if (httpResponseCode > 0) {
//      Serial.print("HTTP Response code: ");
//      Serial.println(httpResponseCode);
//      String payload = http.getString();
//      Serial.println(payload);
//    } else {
//      Serial.print("Error code: ");
//      Serial.println(httpResponseCode);
//    }
//    // Free resources
//    http.end();
//  } else {
//    Serial.println("WiFi Disconnected");
//  }
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(90, 50, String(scale.get_units()));
  updateBeerStatus();
  display.display();
  delay(10);
}
