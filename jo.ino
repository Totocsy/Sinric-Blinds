#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "SinricPro.h"
#include "SinricProBlinds.h"

#define WIFI_SSID            "your Wifi"          // don't change for Wokwi simulation!
#define WIFI_PASS            "    "                 // don't change for Wokwi simulation
#define APP_KEY              "    "    // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET           "    "  // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define BLINDS_ID            "    "  // Should look like "5dc1564130xxxxxxxxxxxxxx"
#define BAUD_RATE            115200                 // Change baudrate to your need
#define FULL_DISTANCE_STEPS  1000                   // Number of steps required to open / close the blind completely
#define STEPS_PER_REVOLUTION 200                    // Steps per revolution - is used to calculate the speed
#define RPM                  60                     // Revolutions per minute
#define DIR_PIN              12
#define STEP_PIN             14

bool powerState = true;  // assume device is turned on

int currentPosition     = 0;  // assume blinds are closed
int destinationPosition = 0;

enum class StepperDirection {
    close = -1,
    idle  = 0,
    open  = 1
};

StepperDirection currentDirection = StepperDirection::idle;

// Converts a percentage into absolute steps.
int positionToSteps(int position) {
    return map(position, 0, 100, 0, FULL_DISTANCE_STEPS);
}

// Converts absolute steps into a percentage.
int stepsToPosition(int steps) {
    return map(steps, 0, FULL_DISTANCE_STEPS, 0, 100);
}

// performs a step in the current direction and updates currentPosition
void doStep() {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(1);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(1);
    currentPosition += (int)currentDirection;
}

// set a direction
void setDirection(StepperDirection direction) {
    if (direction == StepperDirection::close) digitalWrite(DIR_PIN, HIGH);
    if (direction == StepperDirection::open) digitalWrite(DIR_PIN, LOW);
    currentDirection = direction;
}

// handles the stepper motor
void handleStepper() {
    if (currentDirection == StepperDirection::idle) return;
    if (!powerState) return;

    static unsigned long lastMillis;
    unsigned long        currentMillis = millis();
    unsigned long        millisToWait  = (60 * 1000) / (STEPS_PER_REVOLUTION * RPM);
    if (lastMillis && currentMillis - lastMillis < millisToWait) return;
    lastMillis = currentMillis;

    doStep();

    if (currentPosition == destinationPosition) setDirection(StepperDirection::idle);
}

void setupStepper() {
    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
}

bool onPowerState(const String &deviceId, bool &state) {
    Serial.printf("Device %s power turned %s \r\n", deviceId.c_str(), state ? "on" : "off");
    powerState = state;
    if (!powerState) {
        destinationPosition = currentPosition;
        currentDirection         = StepperDirection::idle;
    }
    return true;  // request handled properly
}

// open = 100 / close = 0
bool onRangeValue(const String &deviceId, int &position) {
    if (!powerState) {
        Serial.println("Device is turned off. Please turn on first!");
        return true;
    }

    destinationPosition = positionToSteps(position);

    if (destinationPosition > currentPosition) {
        setDirection(StepperDirection::open);
    }

    if (destinationPosition < currentPosition) {
        setDirection(StepperDirection::close);
    }

    return true;  // request handled properly
}

void setupWiFi() {
    Serial.println("Connecting WiFi");
    WiFi.begin(WIFI_SSID , WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
    }
    Serial.println("connected!");
}

void setupSinricPro() {
    SinricProBlinds &myBlinds = SinricPro[BLINDS_ID];
    myBlinds.onPowerState(onPowerState);
    myBlinds.onRangeValue(onRangeValue);

    SinricPro.onConnected([]() { Serial.printf("Connected to SinricPro\r\n"); });
    SinricPro.onDisconnected([]() { Serial.printf("Disconnected from SinricPro\r\n"); });
    SinricPro.begin(APP_KEY, APP_SECRET);
    Serial.println("Connecting SinricPro...");
}

void setup() {
    Serial.begin(BAUD_RATE);
    setupStepper();
    setupWiFi();
    setupSinricPro();
}

void loop() {
    SinricPro.handle();
    handleStepper();
  
}
