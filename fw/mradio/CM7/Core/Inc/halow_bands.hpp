#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "mmwlan.h"

namespace mrover::halow {

// URC sub-bands, 902 to 910, 911 to 919 and 920 to 928 MHz
enum class SubBand : uint8_t { low, mid, high };
enum class Bandwidth : uint8_t { mhz1, mhz2, mhz4, mhz8 };

namespace detail {

// channel constructor helper
constexpr mmwlan_s1g_channel ch(uint32_t center_khz, uint8_t width_mhz) {
    const int16_t width_class = width_mhz == 1 ? 0 : width_mhz == 2 ? 1 : width_mhz == 4 ? 2 : 3;
    return mmwlan_s1g_channel{
        .centre_freq_hz = center_khz * 1000,
        .duty_cycle_sta = 10000,
        .duty_cycle_ap = 10000,
        .global_operating_class = static_cast<int16_t>(68 + width_class),
        .s1g_operating_class = static_cast<int16_t>(1 + width_class),
        .s1g_chan_num = static_cast<uint8_t>((center_khz - 902000) / 500),
        .bw_mhz = width_mhz,
        .max_tx_eirp_dbm = 36,
        .duty_cycle_omit_ctrl_resp = false,
        .pkt_spacing_us = 0,
        .airtime_min_us = 0,
        .airtime_max_us = 0,
    };
}

// operating channel, then its primaries
inline constexpr std::array LOW_1 = {ch(906000, 1)};
inline constexpr std::array LOW_2 = {ch(906000, 2), ch(905500, 1)};
inline constexpr std::array LOW_4 = {ch(906000, 4), ch(905000, 2), ch(904500, 1)};
inline constexpr std::array LOW_8 = {ch(906500, 8), ch(903500, 2), ch(903000, 1)};

inline constexpr std::array MID_1 = {ch(915000, 1)};
inline constexpr std::array MID_2 = {ch(915000, 2), ch(914500, 1)};
inline constexpr std::array MID_4 = {ch(915000, 4), ch(914000, 2), ch(913500, 1)};
inline constexpr std::array MID_8 = {ch(915000, 8), ch(912000, 2), ch(911500, 1)};

inline constexpr std::array HIGH_1 = {ch(924000, 1)};
inline constexpr std::array HIGH_2 = {ch(924000, 2), ch(923500, 1)};
inline constexpr std::array HIGH_4 = {ch(924000, 4), ch(923000, 2), ch(922500, 1)};
inline constexpr std::array HIGH_8 = {ch(924000, 8), ch(921000, 2), ch(920500, 1)};

// wraps a table as the US list that mmwlan_set_channel_list() takes
template <size_t N>
constexpr mmwlan_s1g_channel_list list(const std::array<mmwlan_s1g_channel, N> &channels) {
    return {.country_code = {'U', 'S', '\0'}, .num_channels = N, .channels = channels.data()};
}

// indexed by subband then bandwidth
inline constexpr std::array<std::array<mmwlan_s1g_channel_list, 4>, 3> LISTS = {{
    {list(LOW_1), list(LOW_2), list(LOW_4), list(LOW_8)},
    {list(MID_1), list(MID_2), list(MID_4), list(MID_8)},
    {list(HIGH_1), list(HIGH_2), list(HIGH_4), list(HIGH_8)},
}};

} // namespace detail

constexpr const mmwlan_s1g_channel_list &channel_list(SubBand band, Bandwidth bandwidth) {
    return detail::LISTS[static_cast<size_t>(band)][static_cast<size_t>(bandwidth)];
}

constexpr const mmwlan_s1g_channel &operating_channel(SubBand band, Bandwidth bandwidth) {
    return channel_list(band, bandwidth).channels[0];
}

} // namespace mrover::halow
