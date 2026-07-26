#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>         
#include "packets.h"          

struct LeftJoystickOutput {
    float nx;    
    float ny;    
};

struct RightJoystickOutput {
    int roll;    
    int pitch;  
};

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

const int LEFT_CENTER_X = 1920;
const int LEFT_CENTER_Y = 1897;
const int RIGHT_CENTER_X = 1928;
const int RIGHT_CENTER_Y = 1927;
const float DEADBAND = 0.025;
const int MAX_THROTTLE_INCREMENT = 2;
const int MAX_VAL = 4095;
const int MIN_VAL = 0;

const int DISPLAY_INTERVAL = 200;   
const int SEND_INTERVAL = 20;    //~50 Hz
const int DEBUG_INTERVAL = 200;   

const int JOYSTICK_LX = D2;   
const int JOYSTICK_LY = D3;   
const int JOYSTICK_RX = D1;   
const int JOYSTICK_RY = D0;   
const int ARM_PIN = D8;   
const int MODE_PIN = D9;   

int throttle = 0;
int yaw = 0;
int roll = 0;
int pitch = 0;
bool armed = false;
bool stabilized = true;

unsigned long last_display_update = 0;
unsigned long last_send_time = 0;
unsigned long last_debug_time = 0;

float altitude = 0.0;
float battery = 0.0;
int link_quality = 0;
bool link_lost = false;    

bool display_ok = false;
bool espnow_ok  = false;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
    Serial.begin(115200);

    pinMode(ARM_PIN, INPUT_PULLUP);
    pinMode(MODE_PIN, INPUT_PULLUP);

    display_ok = display_init();
    espnow_ok = espnow_init();

    if (!display_ok) Serial.println("WARN: display init failed - continuing without display");
    if (!espnow_ok) Serial.println("ERROR: ESP-NOW init failed - no link to drone");
}

void loop() {
    unsigned long now = millis();

    int raw_lx = analogRead(JOYSTICK_LX);
    int raw_ly = analogRead(JOYSTICK_LY);
    int raw_rx = analogRead(JOYSTICK_RX);
    int raw_ry = analogRead(JOYSTICK_RY);

    LeftJoystickOutput out = JoystickOut(raw_lx, raw_ly);
    RightJoystickOutput rout = RightJoystickOut(raw_rx, raw_ry);

    armed = (digitalRead(ARM_PIN) == LOW);
    stabilized = (digitalRead(MODE_PIN) == HIGH);


    if (armed) {
        throttle = throttle_set(out.ny, throttle);
    } else {
        throttle = 0;
    }
    yaw = yaw_set(out.nx);
    roll = rout.roll;
    pitch = rout.pitch;

    if (espnow_ok && (now - last_send_time >= SEND_INTERVAL)) {
        last_send_time = now;
        send_control_packet();
    }

    if (display_ok && (now - last_display_update >= DISPLAY_INTERVAL)) {
        last_display_update = now;
        if (armed)
            display_flight_mode();
        else
            display_test_mode();
    }
    if (now - last_debug_time >= DEBUG_INTERVAL) {
        last_debug_time = now;
        Serial.print("THR: ");  Serial.print(throttle);
        Serial.print(" YAW: "); Serial.print(yaw);
        Serial.print(" ROL: "); Serial.print(roll);
        Serial.print(" PIT: "); Serial.print(pitch);
        Serial.print(" ARM: "); Serial.print(armed);
        Serial.print(" MOD: "); Serial.println(stabilized);
    }
}