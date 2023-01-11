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

const int weightSensorDT = 0;
const int weightSensorSCK = 2;

enum Status {
    Waiting, Placed, Drinking, Finished, Failed
};
// Initial beerStatus
Status beerStatus = Waiting;

// Global drinking times
unsigned long startDrinkingTime;
unsigned long drinkingTime;
unsigned long finishDrinkingTime;

// Amount of milliseconds needed to pass to restart the game
const int restartGameTimer = 5000;

// Weight threshold variables
const int fullBeerThreshold = 300;      // Above this amount we consider a full beer is on the scale
const int emptyBeerThreshold = 200;      // Beneath this amount and above the emptyScaleThreshold we consider an empty beer is on the scale
const int emptyScaleThreshold = 100;     // Beneath this amount we consider the scale empty

String enteredName;
bool onlineGame;

void setCrossOrigin() {
  server.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  server.sendHeader(F("Access-Control-Max-Age"), F("86400"));
  server.sendHeader(F("Access-Control-Allow-Methods"), F("PUT,POST,GET,OPTIONS"));
  server.sendHeader(F("Access-Control-Allow-Headers"), F("*"));
};

// Serving Hello World
void getHelloWord() {
  setCrossOrigin();
  server.send(200, "text/json", "{\"name\": \"Hello world\"}");
}

void newBak() {
  setCrossOrigin();
  if (!server.hasArg("plain")) {
    server.send(200, "text/plain", "Body not received");
    return;
  }

  enteredName = server.arg("plain");

  onlineGame = true;

  server.send(200, "text/json", "{success: true}");
  Serial.println(enteredName);
}

void newBakOptions() {
  setCrossOrigin();
  server.send(200);
}

void restartBakTracker() {
  setCrossOrigin();
  beerStatus = Waiting;
  server.send(200, "text/json", "{success: true}");
}

void resetBakTracker() {
  setCrossOrigin();
  server.send(200, "text/json", "{success: true}");
  ESP.reset();
}

// Define routing
void restServerRouting() {
  server.on("/", HTTP_GET, []() {
      server.send(200, F("text/html"),
                  F("Welcome to the REST Web Server"));
  });
  server.on(F("/helloWorld"), HTTP_GET, getHelloWord);
  server.on(F("/bak"), HTTP_POST, newBak);
  server.on(F("/bak"), HTTP_OPTIONS, newBakOptions);
  server.on(F("/reset"), HTTP_GET, resetBakTracker);
  server.on(F("/restart"), HTTP_GET, restartBakTracker);
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
  scale.set_scale(63.8065116279);
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

float millisToSeconds(unsigned long millis) {
  return millis / 1000.f;
}

bool drinkingLoop() {
  float currentWeight = scale.get_units();
  Serial.println(currentWeight);
  if (currentWeight > emptyScaleThreshold) {   // Something has been placed on the scale
    beerStatus = Finished;
    finishDrinkingTime = millis();
//    if (currentWeight < emptyBeerThreshold) {  // Empty glass has been placed on the scale
//      beerStatus = Finished;
//    } else {                                  // Something heavier than empty glass has been placed on the scale
//      beerStatus = Failed;
//    }
    return false;
  }
  display.clear();
  drinkingTime = millis() - startDrinkingTime;
  display.drawString(90, 20, String(millisToSeconds(drinkingTime), 3));
  display.display();
  return true;
}

void updateBeerStatus() {
  if (beerStatus == Waiting) {           // Wait until beer is on the scale
    display.drawString(104, 20, "Place your beer!");

    if (onlineGame) {
      display.drawString(104, 10, enteredName);
    }

    if (scale.get_units() > fullBeerThreshold) {
      beerStatus = Placed;
    }
  } else if (beerStatus == Placed) {    // Beer is on the scale
    display.drawString(110, 20, "Let's start drinking!");
    if (scale.get_units() < emptyScaleThreshold) {
      beerStatus = Drinking;
    }
  } else if (beerStatus == Drinking) {    // Person is drinking the beer
    startDrinkingTime = millis();
    while (drinkingLoop()) {}
  } else if (beerStatus == Finished) {    // Person has finished drinking the beer
    // Display final time
    display.drawString(80, 10, "Finished");
    display.drawString(90, 20, "Your time: " + String(millisToSeconds(drinkingTime)));


    if(millis() - finishDrinkingTime >= restartGameTimer) {
      beerStatus = Waiting;
    }
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
  Serial.print(String(scale.get_units()));
  updateBeerStatus();
  display.display();
  delay(10);
}
