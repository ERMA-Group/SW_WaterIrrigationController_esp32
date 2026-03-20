# ESP32 Communication Implementation Guide

This document outlines how an ESP32 (running ESP-IDF) should communicate with the backend server. The system relies on **three main pillars**:
1. **Device Provisioning & Pairing** (Device ID + Password on label)
2. **Periodic Telemetry Sync** (every 30-60 seconds)
3. **Real-time Server-Sent Events (SSE)** (Instant commands)

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
6. Only after receiving `success` should the ESP32 begin its normal Pillar 2 (Telemetry) and Pillar 3 (SSE) loops.

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
  "valves_telemetry": [
    {"id": 0, "status": "closed", "battery": 100, "signal": 5, "type": "Radio"},
    {"id": 1, "status": "open", "battery": 95, "signal": 4, "type": "Wired"}
  ]
}
```

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

## 3. Real-time Commands (`commands.php`)

To support instant manual watering triggers without waiting for the 60-second telemetry polling, the ESP32 must maintain a continuous Server-Sent Events (SSE) connection.

**Endpoint:** `GET https://aqua.erma.sk/api/commands.php?hw_id=ERMA-1234`

### How SSE Works
SSE is just a standard HTTP GET request that the server never closes. The server sends standard text lines starting with `event:` and `data:`, followed by a blank line `\n\n`.

**Example Server Stream:**
```text
event: connected
data: {"status":"connected","hw_id":"ERMA-1234","server_time":1710931200}

event: ping
data: {"server_time":1710931215}

event: command
data: {"action":"manual_run","valve_id":0,"duration_sec":300,"run_until":"2026-03-20 15:30:00"}

event: command
data: {"action":"stop_run","valve_id":0}

event: command
data: {"action":"restart"}

event: command
data: {"action":"factory_reset"}

event: command
data: {"action":"update","firmware_url":"https://aqua.erma.sk/firmware.bin"}
```

### Supported SSE Commands
The server will push these JSON payloads into `data:` under `event: command`:
1. **Manual Run:** `{"action":"manual_run", "valve_id":0, "duration_sec":300, "run_until":"Y-m-d H:i:s"}` -> The ESP32 should instantly open valve `0` and set a hardware timer to close it after `duration_sec` seconds.
2. **Stop Run:** `{"action":"stop_run", "valve_id":0}` -> The ESP32 should instantly close valve `0` and abort its running manual timer.
3. **Restart:** `{"action":"restart"}` -> The ESP32 should cleanly reboot (`esp_restart()`).
4. **Factory Reset:** `{"action":"factory_reset"}` -> The ESP32 should clear its Non-Volatile Storage (forgetting saved Wi-Fi credentials) and then reboot. Note: This is actively triggered by the server when the user unbinds the device.
5. **Firmware Update:** `{"action":"update", "firmware_url":"..."}` -> The ESP32 should start an OTA update using the provided URL.

### ESP32 ESP-IDF Implementation Guide for SSE

1. **Create the HTTP Client:**
   Initialize `esp_http_client_config_t` pointing to `commands.php?hw_id=<your_device_id>`.
   Set `is_async = false` and `timeout_ms = 30000` (or higher, since the connection is kept alive).

2. **Open the Connection:**
   Call `esp_http_client_open()` to initiate the request.
   Call `esp_http_client_fetch_headers()` to read the `200 OK` response headers.

3. **Read the Stream in a Loop:**
   Create a loop using `esp_http_client_read()`. 
   Read chunks of text into a local buffer line-by-line looking for `\n`.

4. **Parsing the Stream:**
   - If the line starts with `event: command`, prepare to execute the next data line.
   - If the line starts with `data: `, parse the JSON payload remaining on that line.
   - Example parsing logic:
     ```c
     if (strncmp(line, "data: ", 6) == 0) {
         cJSON *json = cJSON_Parse(line + 6);
         if (json) {
             cJSON *action = cJSON_GetObjectItem(json, "action");
             if (action && strcmp(action->valuestring, "manual_run") == 0) {
                 // Open the specified valve for the specified duration immediately!
             }
             cJSON_Delete(json);
         }
     }
     ```

5. **Heartbeats and Reconnection:**
   The server sends an `event: ping` every 15 seconds. 
   If `esp_http_client_read()` blocks for more than 45 seconds without receiving any data, assume the TCP connection dropped (e.g. Wi-Fi loss, NAT timeout), close the client, and immediately try to reconnect in an infinite fallback loop.

## Summary Architecture

* **Task 1 (Sync Task):** Wakes every 60s -> HTTP POST to `device_sync.php` -> Downloads broad schedule -> Sleeps.
* **Task 2 (SSE Task):** Infinite loop -> HTTP GET to `commands.php` -> Blocks on read -> Instantly fires callbacks when `event: command` arrives -> Reconnects if read fails.
