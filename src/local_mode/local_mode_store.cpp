#include "local_mode/local_mode_store.hpp"

#include <cstring>

extern "C" {
#include "nvs.h"
#include "nvs_flash.h"
}

#include "nvram.hpp"

namespace app::local_mode {

namespace {

constexpr const char* kNs = "local_mode_cfg";
constexpr const char* kKeySettings = "settings";

} // namespace

void set_defaults(LocalModeSettings& settings, uint32_t valve_count)
{
    settings = {};
    settings.schema_version = kCurrentSchemaVersion;
    settings.local_mode_enabled = 1U;

    const uint32_t max_valves = (valve_count <= kMaxValves) ? valve_count : kMaxValves;
    for (uint32_t valve = 0; valve < max_valves; ++valve)
    {
        for (uint32_t i = 0; i < kMaxProgramsPerValve; ++i)
        {
            auto& program = settings.valves[valve].programs[i];
            program.enabled = 0U;
            program.hour = 6U;
            program.minute = 0U;
            program.duration_sec = 900U;
            program.days_mask = 0x7FU;
        }
    }
}

bool validate_and_sanitize(LocalModeSettings& settings, uint32_t valve_count)
{
    if (settings.schema_version != kCurrentSchemaVersion)
    {
        return false;
    }

    // Local mode is always enabled by project decision.
    settings.local_mode_enabled = 1U;

    const uint32_t max_valves = (valve_count <= kMaxValves) ? valve_count : kMaxValves;
    for (uint32_t valve = 0; valve < max_valves; ++valve)
    {
        for (uint32_t i = 0; i < kMaxProgramsPerValve; ++i)
        {
            auto& program = settings.valves[valve].programs[i];
            program.enabled = (program.enabled != 0U) ? 1U : 0U;
            if (program.hour > 23U)
            {
                program.hour = 23U;
            }
            if (program.minute > 59U)
            {
                program.minute = 59U;
            }
            if (program.duration_sec < kMinDurationSec)
            {
                program.duration_sec = static_cast<uint16_t>(kMinDurationSec);
            }
            if (program.duration_sec > kMaxDurationSec)
            {
                program.duration_sec = static_cast<uint16_t>(kMaxDurationSec);
            }
            if (!is_valid_days_mask(program.days_mask))
            {
                program.days_mask = 0x7FU;
            }
        }
    }

    // Disable valves above configured count.
    for (uint32_t valve = max_valves; valve < kMaxValves; ++valve)
    {
        for (uint32_t i = 0; i < kMaxProgramsPerValve; ++i)
        {
            settings.valves[valve].programs[i].enabled = 0U;
        }
    }

    return true;
}

bool load(LocalModeSettings& out_settings, uint32_t valve_count)
{
    if (bsw::Nvram::system_init() != ESP_OK)
    {
        return false;
    }

    nvs_handle_t handle = 0;
    if (nvs_open(kNs, NVS_READWRITE, &handle) != ESP_OK)
    {
        return false;
    }

    LocalModeSettings loaded{};
    size_t required = sizeof(loaded);
    const esp_err_t err = nvs_get_blob(handle, kKeySettings, &loaded, &required);
    nvs_close(handle);

    if (err != ESP_OK || required != sizeof(loaded))
    {
        set_defaults(out_settings, valve_count);
        return save(out_settings, valve_count);
    }

    out_settings = loaded;
    if (!validate_and_sanitize(out_settings, valve_count))
    {
        set_defaults(out_settings, valve_count);
        return save(out_settings, valve_count);
    }

    return true;
}

bool save(const LocalModeSettings& settings, uint32_t valve_count)
{
    if (bsw::Nvram::system_init() != ESP_OK)
    {
        return false;
    }

    LocalModeSettings sanitized = settings;
    if (!validate_and_sanitize(sanitized, valve_count))
    {
        set_defaults(sanitized, valve_count);
    }

    nvs_handle_t handle = 0;
    if (nvs_open(kNs, NVS_READWRITE, &handle) != ESP_OK)
    {
        return false;
    }

    const esp_err_t set_err = nvs_set_blob(handle, kKeySettings, &sanitized, sizeof(sanitized));
    if (set_err != ESP_OK)
    {
        nvs_close(handle);
        return false;
    }

    const esp_err_t commit_err = nvs_commit(handle);
    nvs_close(handle);
    return commit_err == ESP_OK;
}

} // namespace app::local_mode
