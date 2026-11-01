#include "storage.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "stm32h7xx_hal.h"

namespace mrover::storage {

namespace {

// must match the region cleared by the CM7 linker script
constexpr std::uint32_t SECTOR = FLASH_SECTOR_7;
constexpr std::uintptr_t SECTOR_ADDRESS = 0x080E0000;
static_assert(SECTOR_ADDRESS == FLASH_BANK1_BASE + SECTOR * FLASH_SECTOR_SIZE);

constexpr std::uint32_t MAGIC = 0x4; // chosen by fair dice roll. guaranteed to be random.
constexpr std::size_t FLASH_WORD_BYTES = 32;
constexpr std::size_t MAX_JSON_BYTES = FLASH_SECTOR_SIZE - FLASH_WORD_BYTES;

struct Header {
    std::uint32_t magic;
    std::uint32_t length;
    std::uint32_t crc;
};

using FlashWord = std::array<std::uint32_t, FLASH_WORD_BYTES / sizeof(std::uint32_t)>;
static_assert(sizeof(Header) <= sizeof(FlashWord));

constexpr std::uint32_t crc32(std::string_view bytes) {
    std::uint32_t crc = 0xFFFFFFFF;
    for (const char byte : bytes) {
        crc ^= static_cast<std::uint8_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) != 0 ? (crc >> 1) ^ 0xEDB88320 : crc >> 1;
        }
    }
    return ~crc;
}
static_assert(crc32("123456789") == 0xCBF43926);

class FlashUnlock {
public:
    FlashUnlock() : m_unlocked{HAL_FLASH_Unlock() == HAL_OK} {}
    ~FlashUnlock() { HAL_FLASH_Lock(); }
    FlashUnlock(const FlashUnlock &) = delete;
    FlashUnlock &operator=(const FlashUnlock &) = delete;

    [[nodiscard]] bool unlocked() const { return m_unlocked; }

private:
    bool m_unlocked;
};

FlashWord erased_word() {
    FlashWord word{};
    word.fill(0xFFFFFFFF);
    return word;
}

bool program(std::uintptr_t address, const FlashWord &word) {
    return HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, address, reinterpret_cast<std::uint32_t>(word.data())) ==
           HAL_OK;
}

} // namespace

std::optional<std::string> load() {
    SCB_InvalidateDCache_by_Addr(reinterpret_cast<void *>(SECTOR_ADDRESS),
                                 static_cast<std::int32_t>(FLASH_SECTOR_SIZE));

    Header header{};
    std::memcpy(&header, reinterpret_cast<const void *>(SECTOR_ADDRESS), sizeof header);
    if (header.magic != MAGIC || header.length > MAX_JSON_BYTES) {
        return std::nullopt;
    }
    const std::string_view json{reinterpret_cast<const char *>(SECTOR_ADDRESS + FLASH_WORD_BYTES), header.length};
    if (crc32(json) != header.crc) {
        return std::nullopt;
    }
    return std::string{json};
}

bool save(std::string_view json) {
    if (json.size() > MAX_JSON_BYTES) {
        return false;
    }
    if (const auto stored = load(); stored && *stored == json) {
        return true;
    }

    const FlashUnlock flash; // guard
    if (!flash.unlocked()) {
        return false;
    }

    __HAL_FLASH_CLEAR_FLAG_BANK1(FLASH_FLAG_ALL_ERRORS_BANK1);

    FLASH_EraseInitTypeDef erase{
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .Banks = FLASH_BANK_1,
        .Sector = SECTOR,
        .NbSectors = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
    };
    std::uint32_t failed_sector = 0;
    if (HAL_FLASHEx_Erase(&erase, &failed_sector) != HAL_OK) {
        return false;
    }

    for (std::size_t offset = 0; offset < json.size(); offset += FLASH_WORD_BYTES) {
        FlashWord word = erased_word();
        std::memcpy(word.data(), json.data() + offset, std::min(FLASH_WORD_BYTES, json.size() - offset));
        if (!program(SECTOR_ADDRESS + FLASH_WORD_BYTES + offset, word)) {
            return false;
        }
    }

    // header last
    const Header header{
        .magic = MAGIC,
        .length = static_cast<std::uint32_t>(json.size()),
        .crc = crc32(json),
    };
    FlashWord header_word = erased_word();
    std::memcpy(header_word.data(), &header, sizeof header);
    return program(SECTOR_ADDRESS, header_word);
}

} // namespace mrover::storage
