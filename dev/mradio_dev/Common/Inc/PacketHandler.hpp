#include <cstdint>

#define MTU 1536 // ETH header + IP payload + VLAN + CRC

// enum representing packet type
enum class PacketType : uint8_t {
    CAMERA = 0,
    TELEMETRY = 1,
    COMMAND = 2
};

// class for handling packing and parsing IP packets
class PacketHandler {
public:
    // default packet handler constructor
    PacketHandler() = default;

    // groups two packets into grouped_buf and returns the resulting size, returns UINT16_MAX on failure
    uint16_t groupPackets(uint8_t* buf1, uint16_t buf1_size, uint8_t* buf2, uint16_t buf2_size, uint8_t* grouped_buf);

    // compresses buf and returns the resulting size, returns UINT16_MAX on failure
    uint16_t compress(uint8_t* buf);

    // parses packet_in and places the parsed packet payload in payload_out, returns the size of payload_out, returns UINT16_MAX on failure
    uint16_t parsePacket(const uint8_t* packet_in, uint16_t length, const uint8_t* payload_out);

    // packs an IP packet into out_buf, returns resulting length, returns UINT16_MAX on failure
    uint16_t packPacket(uint8_t* out_buf,
                        uint32_t src_ip,
                        uint32_t dst_ip,
                        uint16_t src_port,
                        uint16_t dst_port,
                        PacketType type,
                        const uint8_t* payload,
                        uint16_t payload_len);
};