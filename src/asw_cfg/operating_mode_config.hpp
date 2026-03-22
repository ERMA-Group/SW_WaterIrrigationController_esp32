#pragma once

namespace app::mode_cfg {

enum class OperatingMode {
    Standard = 0,
    PureMqtt = 1,
};

bool load(OperatingMode& out);
bool save(OperatingMode mode);

} // namespace app::mode_cfg
