#pragma once

#include <cstdint>

#include "local_mode/local_mode_types.hpp"

namespace app::local_mode {

bool load(LocalModeSettings& out_settings, uint32_t valve_count);
bool save(const LocalModeSettings& settings, uint32_t valve_count);

} // namespace app::local_mode
