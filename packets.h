#ifndef PACKETS_H
#define PACKETS_H

#include <stdint.h>   

static const uint8_t PROTOCOL_VERSION = 1; //update as needed

enum MsgType : uint8_t {
    MSG_CONTROL   = 1,   
    MSG_TELEMETRY = 2,   
};

struct __attribute__((packed)) ControlPacket {
    uint8_t version;    
    uint8_t type;       
    uint8_t seq;        

    int16_t throttle;   
    int16_t yaw;        
    int16_t roll;       
    int16_t pitch;      
    uint8_t flags;     
};

static const uint8_t FLAG_ARMED = (1 << 0);
static const uint8_t FLAG_STABILIZED = (1 << 1);

struct __attribute__((packed)) TelemetryPacket {
    uint8_t version;     
    uint8_t type;          
    int16_t altitude_cm;  
    uint16_t battery_mv;    
    uint8_t link_quality;  
    uint8_t flags;         
};

static const uint8_t BROADCAST_ADDR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static const uint8_t ESPNOW_CHANNEL = 1;  

static const uint32_t LINK_TIMEOUT_MS = 500;

#endif  