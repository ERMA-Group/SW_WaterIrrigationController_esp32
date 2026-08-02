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
constexpr const char* kKeySettingsSize = "settings_sz";

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

    // Check if the stored blob size matches current struct size.
    // If not (e.g. after kNumberOfWiredOutputs changed), migrate gracefully
    // by loading what we can and filling the rest with defaults.
    uint32_t stored_size = 0;
    const bool size_known = (nvs_get_u32(handle, kKeySettingsSize, &stored_size) == ESP_OK);

    LocalModeSettings loaded{};
    size_t required = sizeof(loaded);
    const esp_err_t err = nvs_get_blob(handle, kKeySettings, &loaded, &required);

    if (err != ESP_OK && err != ESP_ERR_NVS_INVALID_LENGTH)
    {
        nvs_close(handle);
        set_defaults(out_settings, valve_count);
        return save(out_settings, valve_count);
    }

    if (err == ESP_ERR_NVS_INVALID_LENGTH)
    {
        printf("Local mode settings blob too large, resetting.\n");
        nvs_close(handle);
        set_defaults(out_settings, valve_count);
        return save(out_settings, valve_count);
    }

    // If the stored blob is smaller than current struct (valve count increased),
    // the trailing valves will remain zero-initialized from the {} initializer.
    // If it's larger (valve count decreased), we truncate — validate_and_sanitize
    // will disable valves above the configured count anyway.
    if (size_known && stored_size != sizeof(loaded))
    {
        printf("Local mode settings blob size changed (%lu -> %u). Migrating.\n",
               static_cast<unsigned long>(stored_size),
               static_cast<unsigned int>(sizeof(loaded)));
    }

    nvs_close(handle);

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

    // Store the blob size so future loads can detect struct layout changes.
    nvs_set_u32(handle, kKeySettingsSize, static_cast<uint32_t>(sizeof(sanitized)));

    const esp_err_t commit_err = nvs_commit(handle);
    nvs_close(handle);
    return commit_err == ESP_OK;
}

} // namespace app::local_mode
