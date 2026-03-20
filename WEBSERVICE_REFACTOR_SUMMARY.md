# WebService Architecture Refactor - Implementation Summary

## Overview
The web service communication has been completely refactored to align with the updated protocol and improve code maintainability. All communication with the backend server is now modular and easily expandable.

## ✅ Protocol Compliance Changes

### 1. **Device Claiming** (NEW)
**File:** [src/cloud/device_claim_client.hpp](src/cloud/device_claim_client.hpp)

The ESP32 can now handle the device pairing flow with backend PIN validation:

```cpp
// Example usage during setup:
auto result = app::cloud::DeviceClaimClient::Claim("ERMA-001", "XRB9L2");
if (result == DeviceClaimClient::ClaimResult::kSuccess) {
    // Device successfully claimed, can proceed with normal operation
}
```

**Supported Results:**
- `kSuccess` - Device claimed and bound to user account
- `kInvalidPin` - PIN expired or incorrect (> 10 minute window)
- `kAlreadyClaimed` - Device already bound to another account
- `kApiCallFailed` - Network/connection error
- `kParseError` - Server response parsing failed
- `kInvalidParam` - Invalid hw_id or pin parameter

### 2. **Valve Telemetry** (FIXED)
Device now reports valve IDs as numeric values (0, 1, 2, ...) instead of strings.

**Before:**
```json
{"valves_telemetry": [{"id": "v1", "status": "open", ...}]}
```

**After:**
```json
{"valves_telemetry": [{"id": 0, "status": "open", ...}]}
```

Backend automatically creates valve entries for any new numeric ID encountered.

### 3. **Stop Run Command** (NEW)
The server can now instantly stop valve operation via SSE:

```json
{"action": "stop_run", "valve_id": 0}
```

Application automatically:
- Closes specified valve
- Clears any running timer
- Syncs state back to cloud

## 📁 New Architecture

### WebService API Layer - Abstraction
**File:** [src/cloud/webservice_api.hpp](src/cloud/webservice_api.hpp) / [.cpp](src/cloud/webservice_api.cpp)

Common HTTP infrastructure for all API calls:
- Centralized HTTP client management
- SSL certificate bundling
- Error handling with Result enums
- Support for JSON POST/GET operations

```cpp
// Low-level API (used internally by higher-level clients)
WebServiceApi::Result result = WebServiceApi::PostJson(
    "https://aqua.erma.sk/api/example.php",
    request_body,
    response_body,
    15000  // timeout_ms
);
```

### Device Claim Client - Device Pairing
**File:** [src/cloud/device_claim_client.hpp](src/cloud/device_claim_client.hpp) / [.cpp](src/cloud/device_claim_client.cpp)

Handles one-time device pairing with web-generated PIN:
- Validates PIN within 10-minute validity window
- Binds device to user's account
- Provides meaningful error feedback

```cpp
const char* error_msg = DeviceClaimClient::ResultToString(result);
```

### Server Sync Client - Periodic & Real-time Commands
**File:** [src/cloud/server_sync_client.hpp](src/cloud/server_sync_client.hpp) / [.cpp](src/cloud/server_sync_client.cpp)

Handles dual-task communication:
- **Task 1:** Periodic device state sync (60-second polling)
- **Task 2:** Real-time SSE command stream (instant actions)

Updated Command Handlers:
```cpp
struct CommandHandlers {
    void* context = nullptr;
    void (*manual_run)(void* context, uint32_t valve_index, uint32_t duration_sec) = nullptr;
    void (*stop_run)(void* context, uint32_t valve_index) = nullptr;  // NEW
    void (*restart)(void* context) = nullptr;
    void (*update)(void* context, const char* firmware_url) = nullptr;
};
```

## 🔄 Updated Valve Indexing

**Protocol Change:** Valves are now 0-indexed as per backend specification.

**Updated Application Callbacks:**

```cpp
// Manual run command handler
void handleManualRunCommand(uint32_t valve_index, uint32_t duration_sec);

// NEW: Stop/cancel running valve
void handleStopRunCommand(uint32_t valve_index);
```

**Mapping:**
- Valve 0 (protocol) → Shift Register Output 0
- Supports future expansion: Valve 1 → Output 1, Valve 2 → Output 2, etc.

## 🚀 Future Extensibility

The new modular architecture makes it easy to add new endpoints:

1. **Create a new client class** in `src/cloud/`
2. **Inherit or use WebServiceApi** for HTTP operations
3. **Handle parsing** with cJSON
4. **Register callbacks** in application as needed

Example structure for future features:
```cpp
class ConfigurationClient {
    static Result GetConfiguration(...);
    static Result UpdateSettings(...);
};
```

## 📊 Build Information

- **Status:** ✅ Clean build, no errors
- **Target:** ESP32-S2 (ESP-IDF v5.5.1)
- **Output:** `build/SW_WaterIrrigationContoller.bin`
- **Binary Size:** 0x10f170 bytes (68.4% of partition)

## 🔗 Related Files Modified

- `src/application.hpp` - Added stop_run handler declarations
- `src/application.cpp` - Implemented stop_run callbacks, updated valve mapping
- `src/CMakeLists.txt` - Added new source files to build

## Migration Notes

If you have existing code calling the old valve interfaces:
- Change valve_index from `1` to `0` (if using first valve)
- All protocol calls now use 0-based indexing
- Application callbacks remain socket-compatible with server

