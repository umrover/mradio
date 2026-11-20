#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace mrover::storage {

[[nodiscard]] std::optional<std::string> load();
// erasing stalls CM4, which runs from the same flash bank
[[nodiscard]] bool save(std::string_view json);

} // namespace mrover::storage
