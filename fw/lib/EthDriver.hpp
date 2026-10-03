#include "RingBuffer.hpp"

// class for interfacing with ethernet peripherals
class EthDriver {
private:
    RingBuffer* m_rx_buf; // buffer for received packets
    RingBuffer* m_tx_buf; // buffer for grouped + compressed packets
    ETH_HandleTypeDef* m_heth; // pointer to ethernet handle

public:
    // default constructor
    EthDriver() = default;

    // initialize member variables and ethernet peripherals
    void init(RingBuffer* rx_buf, RingBuffer* tx_buf, ETH_HandleTypeDef* heth);

    // receives a packet when interrupt is triggered and places packet into rx_buf, returns true for success and false for failure
    bool handleReceive();

    // transmits a grouped and compressed packet from tx_buf, returns true for success and false for failure
    bool handleTransmit();
};