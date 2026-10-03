#include <cstdint>

// enum representing the radio role
enum class NetworkRole : uint8_t {
    AP = 0,
    STATION = 1
};

// enum representing desired operating frequency
enum class WifiFreq : uint8_t {
    WIFI_2_4_GHz = 0,
    WIFI_5_GHz = 1
};

// class for interfacing with IF573 wifi module
class IF573 {
private:
    NetworkRole m_role{NetworkRole::STATION}; // radio role
    SD_HandleTypeDef* m_hsd; // pointer to sdio handle

public:
    // default constructor
    IF573() = default;

    // initialize member variables and module
    void init(NetworkRole role, SD_HandleTypeDef* hsd);

    // sets up the access point, returns true for success and false for failure
    bool setup_ap(WifiFreq freq);

    // connects to ap, returns true for success and false for failure
    bool connect_to_ap(WifiFreq freq);

    // disconnects from ap
    void disconnect_ap();

    // open both a udp and tcp socket, returns true for success and false for failure
    bool open_sockets(const char* ip_addr, uint16_t port);

    // closes the udp and tcp sockets
    void close_sockets();

    // sends len bytes of data from buf, returns true for success and false for failure
    bool send(const uint8_t* buf, uint16_t len);

    // receives up to max_len bytes into buf and returns the number of bytes received, returns UINT16_MAX on failure
    uint16_t receive(uint8_t* buf, uint16_t max_len);
};