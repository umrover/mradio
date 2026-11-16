#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace mrover::storage {

[[nodiscard]] std::optional<std::string> load();
// erasing stalls CM7 fetches from flash bank 1
[[nodiscard]] bool save(std::string_view json);

} // namespace mrover::storage
