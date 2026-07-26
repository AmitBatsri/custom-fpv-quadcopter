#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>        
#include "packets.h"    

volatile ControlPacket latest;          
volatile bool have_packet = false;  
volatile uint32_t last_packet_time = 0; 

bool failsafe_active = false;

int  throttle = 0;
int  yaw = 0, roll = 0, pitch = 0;
bool armed = false, stabilized = true;

uint8_t last_seq = 0;
bool     seq_valid = false;
uint32_t dropped_count = 0;

static const uint32_t TELEM_SEND_INTERVAL = 100;   
uint32_t last_telem_send = 0;

static const uint32_t DBG_INTERVAL = 200;       
uint32_t last_dbg = 0;

void on_data_received(const uint8_t *mac, const uint8_t *data, int len) {
    if (len < (int)sizeof(ControlPacket)) return;  

    ControlPacket pkt;
    memcpy(&pkt, data, sizeof(pkt));

    if (pkt.version != PROTOCOL_VERSION) return;     
    if (pkt.type != MSG_CONTROL) return;     

    if (seq_valid) {
        uint8_t expected = (uint8_t)(last_seq + 1);
        if (pkt.seq != expected) {
            dropped_count += (uint8_t)(pkt.seq - expected);
        }
    }
    last_seq = pkt.seq;
    seq_valid = true;

    latest = pkt;
    have_packet = true;
    last_packet_time = millis();
}

void on_data_sent(const uint8_t *mac, esp_now_send_status_t status) {
}

void apply_failsafe() {
    uint32_t now = millis();

    bool stale = (!have_packet) || (now - last_packet_time > LINK_TIMEOUT_MS);

    if (stale) {
        failsafe_active = true;
        throttle = 0; 
        yaw = roll = pitch = 0;
        armed = false;    
    } else {
        failsafe_active = false;
        throttle = latest.throttle;
        yaw = latest.yaw;
        roll = latest.roll;
        pitch = latest.pitch;
        armed = (latest.flags & FLAG_ARMED) != 0;
        stabilized = (latest.flags & FLAG_STABILIZED) != 0;
        if (!armed) throttle = 0;
    }
}

void send_telemetry() {
    TelemetryPacket t;
    t.version = PROTOCOL_VERSION;
    t.type = MSG_TELEMETRY;
    t.altitude_cm = 150;      
    t.battery_mv = 14200;      
    t.link_quality = failsafe_active ? 0 : 100;  
    t.flags = 0;
    
    esp_now_send(BROADCAST_ADDR, (uint8_t*)&t, sizeof(t));
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);  

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return;
    }

    esp_now_register_recv_cb(on_data_received);
    esp_now_register_send_cb(on_data_sent);

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("Failed to add broadcast peer");
        return;
    }

    Serial.println("Drone receiver ready (broadcast)");
}

void loop() {
    uint32_t now = millis();
    apply_failsafe();

    if (now - last_telem_send >= TELEM_SEND_INTERVAL) {
        last_telem_send = now;
        send_telemetry();
    }

    if (now - last_dbg >= DBG_INTERVAL) {
        last_dbg = now;
        if (failsafe_active) {
            Serial.print("[FAILSAFE] link lost - throttle forced 0   dropped=");
            Serial.println(dropped_count);
        } else {
            Serial.print("THR: ");  Serial.print(throttle);
            Serial.print(" YAW: "); Serial.print(yaw);
            Serial.print(" ROL: "); Serial.print(roll);
            Serial.print(" PIT: "); Serial.print(pitch);
            Serial.print(" ARM: "); Serial.print(armed);
            Serial.print(" MOD: "); Serial.print(stabilized);
            Serial.print(" DROP: "); Serial.println(dropped_count);
        }
    }
}
