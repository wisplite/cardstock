#include "SerialDebug.h"

enum State {
    IDLE, READING_HEADER, READING_PAYLOAD
};

struct PacketHeader {
    uint8_t command;
    uint16_t length;  // 2 bytes for length per protocol spec
    uint8_t checksum;
};

bool debugMode = false;

State currentState = IDLE;

uint8_t rxBuffer[1024];

uint8_t ack[] = {0xAA, 0x06, 0x00, 0x00, 0x00}; // 0x06 is the universal ACK response

namespace SerialDebug {

int handleSerialInput() {
    static PacketHeader currentHeader;
    static uint16_t bytesRead = 0;

    // Continue processing if we're in the middle of a packet, even if no bytes available
    while (Serial.available() > 0 || currentState == READING_PAYLOAD) {
        switch (currentState) {
            case IDLE:
                if (Serial.read() == 0xAA) {
                    Serial.println("Got magic byte 0xAA");
                    currentState = READING_HEADER;
                    bytesRead = 0;
                }
                break;
            case READING_HEADER:
                {
                    int avail = Serial.available();
                    if (avail < 4) {
                        Serial.print("Waiting for header bytes, have ");
                        Serial.print(avail);
                        Serial.println("/4");
                        break;
                    }
                    currentHeader.command = Serial.read();
                    // Read 2 bytes for length (little-endian)
                    uint8_t lengthLow = Serial.read();
                    uint8_t lengthHigh = Serial.read();
                    currentHeader.length = lengthLow | (lengthHigh << 8);
                    currentHeader.checksum = Serial.read();
                    Serial.print("Got header: cmd=0x");
                    Serial.print(currentHeader.command, HEX);
                    Serial.print(" len=");
                    Serial.print(currentHeader.length);
                    Serial.print(" checksum=0x");
                    Serial.println(currentHeader.checksum, HEX);
                    currentState = READING_PAYLOAD;
                    bytesRead = 0;
                }
                break;
            case READING_PAYLOAD:
                if (currentHeader.length == 0) {
                    Serial.println("No payload, processing immediately");
                    currentState = IDLE;
                    return processPacket(currentHeader, rxBuffer);
                } else if (Serial.available() >= currentHeader.length) {
                    Serial.print("Reading ");
                    Serial.print(currentHeader.length);
                    Serial.println(" bytes of payload");
                    Serial.readBytes(rxBuffer, currentHeader.length);
                    currentState = IDLE;
                    return processPacket(currentHeader, rxBuffer);
                } else {
                    Serial.print("Waiting for payload: have ");
                    Serial.print(Serial.available());
                    Serial.print("/");
                    Serial.println(currentHeader.length);
                }
                break;
        }
    }
    return -1;  // No data processed
}

int processPacket(PacketHeader& header, uint8_t* data) {
    switch (header.command) {
        case 0x01:
            // Handle command 0x01
            if (!debugMode) {
                return 0x00;
            }
            
            break;
        case 0x02:
            // Handle command 0x02
            if (!debugMode) {
                return 0x00;
            }
            break;
        case 0x03:
            // Handle command 0x03
            if (!debugMode) {
                return 0x00;
            }
            break;
        case 0x04: // Device wants to initiate dev mode
            debugMode = true;
            Serial.write(ack, sizeof(ack));
            return 0x04;
            break;
        case 0x05: // Device wants to exit dev mode
            debugMode = false;
            Serial.write(ack, sizeof(ack));
            return 0x05;
            break;
        default:
            return 0x00;
            break;
    }
}

bool getDebugMode() {
    return debugMode;
}

} // namespace SerialDebug