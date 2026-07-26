// ============================================================================
//  Drone.ino  --  Drone-side receiver (test / "prove the link" phase)
//
//  Runs on the drone-side ESP32 (test XIAO now; ESP32-S3-WROOM on the board
//  later). For THIS phase it:
//    * receives ControlPackets over ESP-NOW broadcast
//    * validates them (protocol version, message type)
//    * implements the FAILSAFE: if no valid packet arrives within
//      LINK_TIMEOUT_MS, it treats the link as lost and zeroes throttle
//    * sends fake telemetry back so we can test the return link + display
//    * prints received values to Serial for debugging
//
//  LATER (once the board is built) this file grows a UART bridge to the STM32:
//  forward control data to the flight controller, receive real telemetry back,
//  and on failsafe command the STM32 to cut throttle. Not in this phase.
//
//  packets.h must be copied into THIS sketch folder too (identical to the
//  controller's copy).
// ============================================================================

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>          // for esp_wifi_set_channel()
#include "packets.h"           // shared packet definitions

// ---------------------------------------------------------------------------
//  Received control state (the "latest command" from the controller)
// ---------------------------------------------------------------------------
volatile ControlPacket latest;          // most recent valid control packet
volatile bool     have_packet = false;  // have we ever received one?
volatile uint32_t last_packet_time = 0; // millis() of last valid packet

// Failsafe state
bool failsafe_active = false;

// Decoded control values actually used (after failsafe is applied)
int  throttle = 0;
int  yaw = 0, roll = 0, pitch = 0;
bool armed = false, stabilized = true;

// Sequence tracking (to count dropped packets for diagnostics)
uint8_t last_seq = 0;
bool     seq_valid = false;
uint32_t dropped_count = 0;

// Telemetry send timing
static const uint32_t TELEM_SEND_INTERVAL = 100;   // ms (~10 Hz)
uint32_t last_telem_send = 0;

// Debug print timing
static const uint32_t DBG_INTERVAL = 200;          // ms
uint32_t last_dbg = 0;

// ---------------------------------------------------------------------------
//  Receive callback -- fires when any ESP-NOW frame arrives.
//  Keep it SHORT (runs in WiFi context): validate, copy, timestamp. All the
//  acting-on-it happens in loop().
// ---------------------------------------------------------------------------
void on_data_received(const uint8_t *mac, const uint8_t *data, int len) {
    if (len < (int)sizeof(ControlPacket)) return;    // too short

    ControlPacket pkt;
    memcpy(&pkt, data, sizeof(pkt));

    if (pkt.version != PROTOCOL_VERSION) return;     // firmware mismatch
    if (pkt.type    != MSG_CONTROL)      return;     // ignore non-control (e.g. our own telemetry)

    // Count dropped packets from the sequence gap (diagnostics only)
    if (seq_valid) {
        uint8_t expected = (uint8_t)(last_seq + 1);
        if (pkt.seq != expected) {
            // how many we appear to have missed (wraps handled by uint8 math)
            dropped_count += (uint8_t)(pkt.seq - expected);
        }
    }
    last_seq = pkt.seq;
    seq_valid = true;

    latest = pkt;
    have_packet = true;
    last_packet_time = millis();
}

// ---------------------------------------------------------------------------
//  Send callback (optional; kept for symmetry / debugging)
// ---------------------------------------------------------------------------
void on_data_sent(const uint8_t *mac, esp_now_send_status_t status) {
    // Nothing needed here for the test phase.
}

// ---------------------------------------------------------------------------
//  apply_failsafe -- THE SAFETY-CRITICAL FUNCTION.
//  If we haven't heard a valid control packet within LINK_TIMEOUT_MS, cut
//  throttle and mark disarmed. This is what prevents a flyaway if the
//  controller loses power or goes out of range.
// ---------------------------------------------------------------------------
void apply_failsafe() {
    uint32_t now = millis();

    bool stale = (!have_packet) || (now - last_packet_time > LINK_TIMEOUT_MS);

    if (stale) {
        // LINK LOST -> safe state
        failsafe_active = true;
        throttle   = 0;          // <-- the important part: motors to idle
        yaw = roll = pitch = 0;
        armed      = false;      // force disarm
        // stabilized left as-is; when re-armed the controller state takes over
    } else {
        // Link OK -> use the latest received command
        failsafe_active = false;
        throttle   = latest.throttle;
        yaw        = latest.yaw;
        roll       = latest.roll;
        pitch      = latest.pitch;
        armed      = (latest.flags & FLAG_ARMED)      != 0;
        stabilized = (latest.flags & FLAG_STABILIZED) != 0;

        // Extra guard: even with a live link, never allow throttle while the
        // controller reports disarmed.
        if (!armed) throttle = 0;
    }
}

// ---------------------------------------------------------------------------
//  send_telemetry -- fake values for the test phase (real values come from the
//  STM32 over UART later).
// ---------------------------------------------------------------------------
void send_telemetry() {
    TelemetryPacket t;
    t.version      = PROTOCOL_VERSION;
    t.type         = MSG_TELEMETRY;
    t.altitude_cm  = 150;          // fake 1.50 m
    t.battery_mv   = 14200;        // fake 14.2 V
    t.link_quality = failsafe_active ? 0 : 100;   // crude for now
    t.flags        = 0;

    esp_now_send(BROADCAST_ADDR, (uint8_t*)&t, sizeof(t));
}

// ---------------------------------------------------------------------------
//  Setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);   // same channel as controller

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        // On the drone, failed comms is serious, but we still don't hang -- we
        // stay in failsafe (throttle 0) which is the safe state anyway.
        return;
    }

    esp_now_register_recv_cb(on_data_received);
    esp_now_register_send_cb(on_data_sent);

    // Add broadcast peer so we can send telemetry
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

// ---------------------------------------------------------------------------
//  Loop
// ---------------------------------------------------------------------------
void loop() {
    uint32_t now = millis();

    // 1) Always apply failsafe first -- this decides throttle/arm every loop
    //    based on whether the link is fresh. This runs regardless of whether a
    //    new packet arrived, so a lost link is caught even when the callback
    //    stops firing.
    apply_failsafe();

    // 2) (LATER) forward throttle/yaw/roll/pitch/armed to the STM32 over UART.
    //    For now we just have the decoded values in the globals above.

    // 3) Send telemetry back periodically (non-blocking)
    if (now - last_telem_send >= TELEM_SEND_INTERVAL) {
        last_telem_send = now;
        send_telemetry();
    }

    // 4) Debug print (non-blocking)
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