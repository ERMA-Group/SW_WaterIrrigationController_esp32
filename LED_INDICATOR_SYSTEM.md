# LED Indicator System - Three-State Status Feedback

## Overview
The device now provides real-time status feedback through LED indicators with three distinct patterns corresponding to device state:

| State | Pattern | Frequency | Meaning |
|-------|---------|-----------|---------|
| **Reset/Setup Mode** | Fast Blink | ~4 blinks/sec (128ms toggle) | In setup mode waiting for SSID entry on provisioning portal |
| **Pairing Mode** | Slower Blink | ~2 blinks/sec (256ms toggle) | WiFi connected but not yet paired with server |
| **Connected** | Solid On | Always on | WiFi connected and actively communicating with server |

## Implementation Details

### State Machine
The LED indicator state is automatically determined in `Application::task128ms()` based on:

1. **Setup Mode Active** - If user held reset button for 5+ seconds and provisioning AP is active
2. **WiFi Connected** - If WiFi module has acquired an IP address via `wifi_.is_connected()`
3. **Server Communication** - Implied by WiFi connection; server sync tasks run automatically

### State Transitions

```
┌─────────────────────────────────────────────────────────┐
│  Device Boot / Reset Button Held 5+ seconds             │
│  → setup_mode_active_ = true                            │
│  → Provisioning AP starts                               │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
            RESET/SETUP MODE
            (Fast Blink ~4/sec)
            ├─ User connects to AP
            ├─ Enters WiFi SSID & password
            └─ Submits via HTTP portal
                   │
                   ▼
         Portal submission detected
         WiFi connect attempt begins
                   │
                   ▼
           PAIRING MODE
           (Blink ~2/sec)
           ├─ setup_mode_active_ still true
           ├─ WiFi credentials saved
           ├─ Connecting to home WiFi
           └─ Waiting for IP assignment
                   │
         ┌─────────┴──────────────────┐
         │                            │
    IP Acquired            Connection Failed
         │                    (remains in pairing)
         ▼
    CONNECTED STATE
    (Solid On)
    ├─ setup_mode_active_ → false
    ├─ WiFi connected (got_ip_ = true)
    ├─ Server telemetry starts
    └─ SSE stream connects
```

### Code Location
- [src/application.hpp](src/application.hpp#L69-L72) - LED state enum definition
- [src/application.cpp](src/application.cpp#L250-L320) - LED state machine (task128ms)
- [M_BSW_ESP32/inc/wifi.hpp](M_BSW_ESP32/inc/wifi.hpp#L46) - New `is_connected()` getter method
- [M_BSW_ESP32/src/wifi.cpp](M_BSW_ESP32/src/wifi.cpp#L233-L237) - Implementation

## Usage

### For End Users
Simply observe the LED to understand device status:
- **Blinking fast?** → Device in setup mode, ready for WiFi config
- **Blinking slowly?** → Trying to connect to WiFi
- **Solid/always on?** → Device online and communicating with server

### For Developers
Add logging or custom behavior based on LED state:

```cpp
// Check current state programmatically
if (app_.current_led_state_ == Application::LedIndicatorState::kConnected)
{
    // Device is fully operational
}
```

Access the LED state machine via the public member variable:
```cpp
Application::LedIndicatorState current_led_state_;
```

## Timing
- **Base timer period:** 128 ms (task128ms runs every ~128 milliseconds)
- **Reset mode:** Toggles every cycle → ~4 toggles/sec = ~4 complete cycles/sec
- **Pairing mode:** Toggles every 2 cycles → ~2 toggles/sec = ~2 complete cycles/sec
- **Connected:** LED held at HIGH state continuously

## Future Enhancements

Possible additional states:
- Error/Fault mode (fast rapid blink)
- Manual run active (different pattern)
- OTA update in progress (alternating pattern)

These can be easily added by extending `LedIndicatorState` enum and adding cases in the switch statement.

## Technical Notes

- LED hardware: See [src/bsw_cfg/gpio_cfg.hpp](src/bsw_cfg/gpio_cfg.hpp) for pin assignments
- GPIO control: Uses `app::gpio::led_status` which maps to configured LED GPIO
- Thread-safe: Runs in main scheduler context (128ms task)
- No blocking calls: LED updates are non-blocking toggle operations
