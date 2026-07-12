#include <Arduino.h>
#include <RadioLib.h>

// RAK3401 + RAK13302 wiring, supplied by the vendored board variant.
// The RAK13302's PA must be powered before the SX1262 is initialised.
SX1262 radio = new Module(SX126X_CS, SX126X_DIO1, SX126X_RESET, SX126X_BUSY, SPI1);

namespace {
constexpr float FrequencyMHz = 915.0f;
constexpr float BitRateKbps = 4.8f;
constexpr float DeviationKHz = 2.380371f;
constexpr float RxBandwidthKHz = 58.6f;
constexpr int8_t Sx1262OutputDbm = -9;
constexpr uint16_t PreambleBits = 16;
uint8_t SyncWord[] = {0xD3, 0x91};

constexpr uint8_t PacketMagic = 0xA5;
constexpr uint8_t PacketVersion = 0x01;
constexpr uint8_t PacketPing = 0x01;
constexpr uint8_t PacketPong = 0x02;
constexpr uint8_t PacketHello = 0x03;
constexpr size_t PacketOverhead = 6; // magic, version, type, sequence, length, CRC-16
constexpr size_t MaxPayload = 48;
constexpr size_t MaxPacket = PacketOverhead + MaxPayload;

uint8_t sequence = 0;

uint16_t crc16_ccitt(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for(uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021) :
                                   static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

size_t make_packet(uint8_t type, uint8_t seq, const char* text, uint8_t* out) {
    const size_t payload_length = min(strlen(text), MaxPayload);
    out[0] = PacketMagic;
    out[1] = PacketVersion;
    out[2] = type;
    out[3] = seq;
    out[4] = static_cast<uint8_t>(payload_length);
    memcpy(&out[5], text, payload_length);
    const uint16_t crc = crc16_ccitt(out, 5 + payload_length);
    out[5 + payload_length] = static_cast<uint8_t>(crc >> 8);
    out[6 + payload_length] = static_cast<uint8_t>(crc & 0xFF);
    return 7 + payload_length;
}

bool parse_packet(const uint8_t* data, size_t length, uint8_t& type, uint8_t& seq, char* text) {
    if(length < 7 || data[0] != PacketMagic || data[1] != PacketVersion) return false;
    const size_t payload_length = data[4];
    if(payload_length > MaxPayload || length != payload_length + 7) return false;
    const uint16_t received_crc = static_cast<uint16_t>(data[length - 2]) << 8 | data[length - 1];
    if(crc16_ccitt(data, length - 2) != received_crc) return false;
    type = data[2];
    seq = data[3];
    memcpy(text, &data[5], payload_length);
    text[payload_length] = '\0';
    return true;
}

void send_text(uint8_t type, const char* text) {
    uint8_t packet[MaxPacket];
    const uint8_t seq = sequence++;
    const size_t length = make_packet(type, seq, text, packet);
    const int state = radio.transmit(packet, length);
    Serial.printf("TX type=%u seq=%u text=%s result=%d\r\n", type, seq, text, state);
}

void receive_once() {
    uint8_t packet[MaxPacket];
    // RadioLib's SX1262::receive() (RadioLib 7.x) polls the wired DIO1 pin for
    // up to `timeout` ms, then calls readData(), which reads getPacketLength()
    // bytes (the true on-air length) into our buffer -- so packet[4] below is
    // populated from the actual received frame, not garbage.
    const int state = radio.receive(packet, sizeof(packet), 250);
    if(state == RADIOLIB_ERR_RX_TIMEOUT) return;
    if(state != RADIOLIB_ERR_NONE) {
        Serial.printf("RX radio error=%d\r\n", state);
        return;
    }

    uint8_t type = 0;
    uint8_t seq = 0;
    char text[MaxPayload + 1];
    const size_t length = packet[4] + 7;
    if(!parse_packet(packet, length, type, seq, text)) {
        Serial.printf("RX rejected\r\n");
        return;
    }

    Serial.printf("RX type=%u seq=%u text=%s RSSI=%.1f SNR=%.1f\r\n",
                  type,
                  seq,
                  text,
                  radio.getRSSI(),
                  radio.getSNR());
    if(type == PacketPing) send_text(PacketPong, "PONG");
}

void print_help() {
    Serial.println("Commands: h=help, t=send HELLO, p=send PING; RX remains active.");
}
} // namespace

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("RAK3401/RAK13302 915 MHz 2-FSK bench tester");

    pinMode(SX126X_POWER_EN, OUTPUT);
    digitalWrite(SX126X_POWER_EN, HIGH);
    delay(20);

    SPI1.begin();
    const int state = radio.beginFSK(
        FrequencyMHz,
        BitRateKbps,
        DeviationKHz,
        RxBandwidthKHz,
        Sx1262OutputDbm,
        PreambleBits,
        SX126X_DIO3_TCXO_VOLTAGE,
        false);
    if(state != RADIOLIB_ERR_NONE) {
        Serial.printf("Radio init failed: %d\r\n", state);
        while(true) delay(1000);
    }

    radio.setDio2AsRfSwitch(true);
    radio.setSyncWord(SyncWord, sizeof(SyncWord));
    radio.setDataShaping(RADIOLIB_SHAPING_NONE);
    radio.setEncoding(RADIOLIB_ENCODING_NRZ);
    radio.variablePacketLengthMode(RADIOLIB_SX126X_MAX_PACKET_LENGTH);

    Serial.printf("Ready: %.3f MHz, 2-FSK %.1f kbps, +/- %.3f kHz\r\n",
                  FrequencyMHz,
                  BitRateKbps,
                  DeviationKHz);
    print_help();
}

void loop() {
    if(Serial.available()) {
        switch(Serial.read()) {
        case 'h': print_help(); break;
        case 't': send_text(PacketHello, "HELLO"); break;
        case 'p': send_text(PacketPing, "PING"); break;
        default: break;
        }
    }
    receive_once();
}
