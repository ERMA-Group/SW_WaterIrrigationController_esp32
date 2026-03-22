# ESP32 Communication Implementation Guide

This document outlines how an ESP32 (running ESP-IDF) should communicate with the backend server. The system relies on **three main pillars**:
1. **Device Provisioning & Pairing** (Device ID + Password on label)
2. **Periodic Telemetry Sync** (every 30-60 seconds)
3. **Real-time Long Polling** (Instant manual commands)

## 1. Device Identity & Pairing

The backend no longer uses short-lived tokens. Every physical device must have a **Hardware ID** (e.g. `ERMA-1234`) and a **Device Password** (e.g. `BLUE-RAIN-42`) printed on its label.

### Factory Provisioning (Backend Side)
Before the device is sold, an Admin **must** inject the `Hardware ID` and `Device Password` into the `aqua_ecus` database via the Admin Portal on the website.

### User Claiming (Frontend Side) & The Secure PIN Window
The end-user inputs the `Hardware ID` and `Device Password` into the "Add Device" modal on the website. 
Upon credential validation, the website generates a **random 6-letter PIN** (e.g. `XRB9L2`) and starts a 10-minute validity timer. The user is instructed to type this PIN into the ESP32's local setup screen.

### ESP32 Responsibility & Pairing Sequence (Web-Generated PIN)
The ESP32 firmware **MUST** include a field for a 6-digit PIN on its local captive setup portal (e.g. `192.168.4.1`) where the user also enters their home Wi-Fi credentials.
1. When the user powers on or resets the device, the ESP32 broadcasts a setup Wi-Fi network.
2. The user connects their phone to this network and types the Web-Generated PIN into the ESP32 setup screen alongside their home Wi-Fi SSID and Password.
3. The ESP32 connects to the internet.
4. The ESP32 immediately makes a one-time HTTP POST request to `/api/device_claim.php` with the payload:
   `{ "hw_id": "ERMA-001", "pin": "XRB9L2" }`
5. The server validates the PIN against the active 10-minute web claims. If successful, the server permanently binds the device to the user's account and returns `{ "status": "success" }`.
6. Only after receiving `success` should the ESP32 begin its normal Pillar 2 (Telemetry) and Pillar 3 (Long Polling) loops.

---

## 2. Periodic Telemetry Sync (`device_sync.php`)

The ESP32 must report its physical state and retrieve its scheduled programming (like daily watering schedules) every 30-60 seconds.

**Endpoint:** `POST https://aqua.erma.sk/api/device_sync.php`

**Request Payload (JSON):**
```json
{
  "hw_id": "ERMA-1234",
  "status": "Online",
  "system_health": "OK",
  "fw_version": "1.2.3",
  "wifi_ssid": "MyHomeWiFi",
  "wifi_signal": -65,
  "ip_address": "192.168.1.50",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "reset_reason": "Power On",
  "valves_telemetry": [
    {"id": 0, "status": "closed", "battery": 100, "signal": 5, "type": "Radio"},
    {"id": 1, "status": "open", "battery": 95, "signal": 4, "type": "Wired"}
  ]
}
```

**Telemetry Fields:**
- `hw_id`: Unique hardware identifier (on label).
- `status`: Device state (e.g., `Online`, `Idle`, `Watering`).
- `system_health`: Overall diagnostic status (`OK`, `Warning`, `Error`).
- `fw_version`: Current firmware string.
- `wifi_ssid`: Connected network name.
- `wifi_signal`: RSSI in dBm (e.g., `-65`).
- `ip_address`: Local network IP.
- `mac_address`: Device physical MAC address.
- `reset_reason`: Why the device last rebooted.
- `valves_telemetry`: Array of connected hardware outputs.


**How Valves are Discovered:**
The ESP32 does not need to formally "register" its valves. Simply include them using their hardware **integer output index** (`0`, `1`, `2`...) in the `valves_telemetry` array. The server will:
- **Auto-create** any new valve it hasn't seen before (named `"Valve 1"`, `"Valve 2"` etc. — the user can rename them from the web dashboard).
- **Update telemetry** (`battery`, `signal`, `status`) for existing valves on every sync.
- **Reconcile the list** — any valve in the database that is NOT present in the current `valves_telemetry` payload will be automatically deleted. This means the database always mirrors exactly the hardware outputs the device physically has. If the device has 2 outputs, report 2 entries; if it has 4, report 4.

**Response Payload (JSON):**
```json
{
  "server_time": 1710931200,
  "hw_id": "ERMA-1234",
  "valve_commands": [
    {
      "id": 0,
      "connection_type": "Wired",
      "operating_mode": "Custom",
      "use_humidity_sensor": true,
      "humidity_threshold": 50,
      "use_weather_forecast": false,
      "rain_chance_threshold": 30,
      "programs": [
        {"time": "06:00", "duration": 15}
      ]
    }
  ]
}
```

**ESP32 Implementation Notes:**
- Use `esp_http_client` to send the POST request in a FreeRTOS task.
- Parse the JSON response using `cJSON`.
- Download the `config` object and update local watering schedules.

---

## 3. Real-time Commands (`commands.php`) via Long Polling

To support instant manual watering triggers without waiting for the 60-second telemetry polling, the ESP32 should use a **Long Polling** mechanism. Unlike a permanent stream, the device makes a standard HTTP request, and the server waits (up to 20 seconds) for a command before responding and closing the connection.

**Endpoint:** `GET https://aqua.erma.sk/api/commands.php?hw_id=ERMA-1234`

### How Long Polling Works
1. The ESP32 sends a standard HTTP GET request to `/api/commands.php?hw_id=...`.
2. The server receives the request and checks the database for any pending (`sent=0`) commands.
3. If no command is found, the server **waits** (e.g., 20 seconds), checking the DB every 250ms.
4. If a command appears, the server returns it immediately (JSON array) and closes the connection.
5. If 20 seconds pass with no command, the server returns `{"status":"no_commands"}` and closes the connection.
6. **Important**: As soon as the connection closes (either via a command or a timeout), the ESP32 should **immediately** start a new request to wait for the next command.

**Example Response (Command Found):**
```json
{
  "status": "success",
  "commands": [
    { "action": "manual_run", "valve_id": 1, "duration_sec": 300 }
  ]
}
```

**Example Response (Timeout / No Commands):**
```json
{
  "status": "no_commands"
}
```

**Connection Resilience:**
If the HTTP request fails due to Wi-Fi drop, the ESP32 should wait 2-5 seconds and retry the polling request. This ensures the system is self-healing.

### Supported Commands
The server will return these JSON objects inside the `commands` array:
1. **Manual Run:** `{"action":"manual_run", "valve_id":0, "duration_sec":300, "run_until":"Y-m-d H:i:s"}` -> The ESP32 should instantly open valve `0` and set a hardware timer to close it after `duration_sec` seconds.
2. **Stop Run:** `{"action":"stop_run", "valve_id":0}` -> The ESP32 should instantly close valve `0` and abort its running manual timer.
3. **Restart:** `{"action":"restart"}` -> The ESP32 should cleanly reboot (`esp_restart()`).
4. **Factory Reset:** `{"action":"factory_reset"}` -> The ESP32 should clear its Non-Volatile Storage (forgetting saved Wi-Fi credentials) and then reboot. Note: This is actively triggered by the server when the user unbinds the device.
5. **Firmware Update:** `{"action":"update"}` -> The ESP32 should start an OTA update using its locally stored firmware URL.

## Summary Architecture

* **Task 1 (Sync Task):** Wakes every 60s -> HTTP POST to `device_sync.php` -> Downloads broad schedule -> Sleeps.
* **Task 2 (Command Task):** Infinite loop -> HTTP GET to `commands.php` (Long Polling) -> Blocks on read -> Instantly executes commands if any arrive -> Immediately re-polls.

---

## Factory Reset & Re-pairing

If a device is factory reset (internal state cleared, Wi-Fi lost), it will return to **Setup Mode** (AP).

1. The device remains "Bound" to the user's account on the server.
2. To re-connect it, the user must follow the standard **Pairing Flow** again.
3. The server allows re-pairing a device that is already bound to the *same* user (it will generate a fresh PIN).
4. Once the device receives the new PIN through its setup page, it performs the `device_claim.php` handshake.
5. After a successful claim, the server returns `{"status": "success"}` and the device can resume normal sync operations.

**Security Note:** If a device is reset but the user *doesn't* initiate a new pairing, the device will stay in AP mode and won't be able to sync data until the owner re-validates the connection via a new PIN. This prevents unauthorized access if a device is physically reset by a third party.

