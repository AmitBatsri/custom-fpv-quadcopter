static uint8_t  tx_seq = 0;             

static const int LQ_WINDOW = 20;        
static bool      lq_history[LQ_WINDOW]; 
static int       lq_index = 0;
static bool      lq_filled = false;     

static const uint32_t TELEM_TIMEOUT_MS = 1000;
static uint32_t last_telem_time = 0;

static void record_send_result(bool ok) {
    lq_history[lq_index] = ok;
    lq_index++;
    if (lq_index >= LQ_WINDOW) {
        lq_index = 0;
        lq_filled = true;
    }

    int count = lq_filled ? LQ_WINDOW : lq_index;
    if (count == 0) { link_quality = 0; return; }

    int good = 0;
    for (int i = 0; i < count; i++) if (lq_history[i]) good++;
    link_quality = (good * 100) / count;
}

void on_data_sent(const uint8_t *mac, esp_now_send_status_t status) {
    record_send_result(status == ESP_NOW_SEND_SUCCESS);
}

void on_data_received(const uint8_t *mac, const uint8_t *data, int len) {
    if (len < (int)sizeof(TelemetryPacket)) return;   

    TelemetryPacket t;
    memcpy(&t, data, sizeof(t));

    if (t.version != PROTOCOL_VERSION) return;        
    if (t.type != MSG_TELEMETRY) return;        

    altitude = t.altitude_cm / 100.0f;
    battery = t.battery_mv / 1000.0f;

    last_telem_time = millis();
    link_lost = false;                               
}

bool espnow_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();               

    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW init failed");
        return false;                 
    }

    esp_now_register_send_cb(on_data_sent);
    esp_now_register_recv_cb(on_data_received);

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer)); 

    memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("Failed to add broadcast peer");
        return false;
    }

    for (int i = 0; i < LQ_WINDOW; i++) lq_history[i] = false;

    Serial.println("ESP-NOW ready");
    return true;
}

void send_control_packet() {
    ControlPacket pkt;
    pkt.version = PROTOCOL_VERSION;
    pkt.type = MSG_CONTROL;
    pkt.seq = tx_seq++;        
    pkt.throttle = (int16_t)throttle;
    pkt.yaw = (int16_t)yaw;
    pkt.roll = (int16_t)roll;
    pkt.pitch = (int16_t)pitch;

    pkt.flags = 0;
    if (armed) pkt.flags |= FLAG_ARMED;
    if (stabilized) pkt.flags |= FLAG_STABILIZED;

    esp_now_send(BROADCAST_ADDR, (uint8_t*)&pkt, sizeof(pkt));

    if (millis() - last_telem_time > TELEM_TIMEOUT_MS) {
        link_lost = true;
    }
}