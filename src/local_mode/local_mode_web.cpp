#include "local_mode/local_mode_web.hpp"

#include <cstdlib>
#include <string>

namespace app::local_mode {

namespace {

bool read_request_body(httpd_req_t* req, std::string& body, size_t max_len)
{
    const int total_len = req->content_len;
    if (total_len <= 0 || static_cast<size_t>(total_len) > max_len)
    {
        return false;
    }

    body.clear();
    body.resize(static_cast<size_t>(total_len));

    int received = 0;
    while (received < total_len)
    {
        const int r = httpd_req_recv(req, body.data() + received, total_len - received);
        if (r <= 0)
        {
            if (r == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            return false;
        }
        received += r;
    }

    return true;
}

std::string form_value(const std::string& body, const char* key)
{
    const std::string token = std::string(key) + "=";
    const size_t start_pos = body.find(token);
    if (start_pos == std::string::npos)
    {
        return "";
    }

    size_t start = start_pos + token.size();
    size_t end = body.find('&', start);
    if (end == std::string::npos)
    {
        end = body.size();
    }

    return body.substr(start, end - start);
}

} // namespace

void LocalModeWebServer::setStateCallback(const std::function<std::string()>& callback)
{
    state_callback_ = callback;
}

void LocalModeWebServer::setProgramsGetCallback(const std::function<std::string()>& callback)
{
    programs_get_callback_ = callback;
}

void LocalModeWebServer::setProgramsCallback(const std::function<bool(const std::string&)>& callback)
{
    programs_callback_ = callback;
}

void LocalModeWebServer::setManualRunCallback(const std::function<bool(uint32_t, uint32_t)>& callback)
{
    manual_run_callback_ = callback;
}

void LocalModeWebServer::setStopRunCallback(const std::function<bool(uint32_t)>& callback)
{
    stop_run_callback_ = callback;
}

void LocalModeWebServer::setSyncNowCallback(const std::function<bool()>& callback)
{
    sync_now_callback_ = callback;
}

bool LocalModeWebServer::start(uint16_t port)
{
    if (server_ != nullptr)
    {
        return true;
    }

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = port;
    cfg.max_uri_handlers = 16;

    if (httpd_start(&server_, &cfg) != ESP_OK)
    {
        server_ = nullptr;
        return false;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = &LocalModeWebServer::root_get_handler,
        .user_ctx = this,
    };

    httpd_uri_t local_mode_root_uri = {
        .uri = "/local_mode",
        .method = HTTP_GET,
        .handler = &LocalModeWebServer::root_get_handler,
        .user_ctx = this,
    };

    httpd_uri_t state_uri = {
        .uri = "/state",
        .method = HTTP_GET,
        .handler = &LocalModeWebServer::state_get_handler,
        .user_ctx = this,
    };

    httpd_uri_t programs_uri = {
        .uri = "/programs",
        .method = HTTP_POST,
        .handler = &LocalModeWebServer::programs_post_handler,
        .user_ctx = this,
    };

    httpd_uri_t programs_get_uri = {
        .uri = "/programs",
        .method = HTTP_GET,
        .handler = &LocalModeWebServer::programs_get_handler,
        .user_ctx = this,
    };

    httpd_uri_t manual_run_uri = {
        .uri = "/manual_run",
        .method = HTTP_POST,
        .handler = &LocalModeWebServer::manual_run_post_handler,
        .user_ctx = this,
    };

    httpd_uri_t stop_run_uri = {
        .uri = "/stop_run",
        .method = HTTP_POST,
        .handler = &LocalModeWebServer::stop_run_post_handler,
        .user_ctx = this,
    };

    httpd_uri_t sync_now_uri = {
        .uri = "/sync_now",
        .method = HTTP_POST,
        .handler = &LocalModeWebServer::sync_now_post_handler,
        .user_ctx = this,
    };

    httpd_uri_t lm_state_uri = {
        .uri = "/local_mode/state",
        .method = HTTP_GET,
        .handler = &LocalModeWebServer::state_get_handler,
        .user_ctx = this,
    };

    httpd_uri_t lm_programs_get_uri = {
        .uri = "/local_mode/programs",
        .method = HTTP_GET,
        .handler = &LocalModeWebServer::programs_get_handler,
        .user_ctx = this,
    };

    httpd_uri_t lm_programs_post_uri = {
        .uri = "/local_mode/programs",
        .method = HTTP_POST,
        .handler = &LocalModeWebServer::programs_post_handler,
        .user_ctx = this,
    };

    httpd_uri_t lm_manual_run_uri = {
        .uri = "/local_mode/manual_run",
        .method = HTTP_POST,
        .handler = &LocalModeWebServer::manual_run_post_handler,
        .user_ctx = this,
    };

    httpd_uri_t lm_stop_run_uri = {
        .uri = "/local_mode/stop_run",
        .method = HTTP_POST,
        .handler = &LocalModeWebServer::stop_run_post_handler,
        .user_ctx = this,
    };

    httpd_register_uri_handler(server_, &root_uri);
    httpd_register_uri_handler(server_, &local_mode_root_uri);
    httpd_register_uri_handler(server_, &state_uri);
    httpd_register_uri_handler(server_, &programs_get_uri);
    httpd_register_uri_handler(server_, &programs_uri);
    httpd_register_uri_handler(server_, &manual_run_uri);
    httpd_register_uri_handler(server_, &stop_run_uri);
    httpd_register_uri_handler(server_, &sync_now_uri);
    httpd_register_uri_handler(server_, &lm_state_uri);
    httpd_register_uri_handler(server_, &lm_programs_get_uri);
    httpd_register_uri_handler(server_, &lm_programs_post_uri);
    httpd_register_uri_handler(server_, &lm_manual_run_uri);
    httpd_register_uri_handler(server_, &lm_stop_run_uri);

    return true;
}

void LocalModeWebServer::stop()
{
    if (server_ != nullptr)
    {
        httpd_stop(server_);
        server_ = nullptr;
    }
}

esp_err_t LocalModeWebServer::root_get_handler(httpd_req_t* req)
{
        const char* html = R"HTML(
<!doctype html>
<html>
<head>
    <meta charset='utf-8'>
    <meta name='viewport' content='width=device-width,initial-scale=1'>
    <title>ERMA Group Irrigation Controller</title>
    <style>
        body { font-family: Arial, sans-serif; max-width: 1100px; margin: 24px auto; padding: 0 16px; background:#f0f4f8; }
        h2 { color:#f74040; margin-bottom: 6px; }
        .hint { color:#334155; margin-top:0; }
        .brand { display:flex; align-items:center; gap:10px; margin-bottom:8px; }
        .logo { display:block; width:100%; max-width:260px; height:auto; object-fit:contain; margin:0 0 8px 0; }
        .brand-title { font-size:18px; font-weight:700; color:#f74040; }
        .sync-status { margin:8px 0 14px 0; font-size:13px; color:#0f172a; }
        .sync-local { color:#b45309; font-weight:700; }
        .sync-cloud { color:#166534; font-weight:700; }
        .notice { margin:8px 0 14px 0; font-size:13px; color:#0f172a; }
        .notice.ok { color:#166534; }
        .notice.err { color:#b91c1c; }
        .row { display:flex; gap:10px; align-items:center; flex-wrap:wrap; }
        .card { background:#fff; border:1px solid #cbd5e1; border-radius:12px; padding:12px; margin-bottom:14px; box-shadow:0 6px 24px rgba(2,6,23,0.08); }
        .valve-title { display:flex; justify-content:space-between; align-items:center; }
        .status-open { color:#15803d; font-weight:700; }
        .status-closed { color:#b91c1c; font-weight:700; }
        button { padding:8px 11px; border-radius:8px; border:0; background:#f74040; color:#fff; font-weight:700; cursor:pointer; }
        button.secondary { background:#334155; }
        input, select { padding:6px; border:1px solid #94a3b8; border-radius:8px; }
        table { width:100%; border-collapse: collapse; margin-top:10px; }
        th, td { border-bottom:1px solid #e2e8f0; padding:6px 4px; text-align:left; font-size:12px; }
    </style>
</head>
<body>
    <div class='brand'>
        <div>
            <img class='logo' alt='ERMA Group' src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAMwAAABICAYAAAC6Axo8AAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAA2ZpVFh0WE1MOmNvbS5hZG9iZS54bXAAAAAAADw/eHBhY2tldCBiZWdpbj0i77u/IiBpZD0iVzVNME1wQ2VoaUh6cmVTek5UY3prYzlkIj8+IDx4OnhtcG1ldGEgeG1sbnM6eD0iYWRvYmU6bnM6bWV0YS8iIHg6eG1wdGs9IkFkb2JlIFhNUCBDb3JlIDUuMy1jMDExIDY2LjE0NTY2MSwgMjAxMi8wMi8wNi0xNDo1NjoyNyAgICAgICAgIj4gPHJkZjpSREYgeG1sbnM6cmRmPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5LzAyLzIyLXJkZi1zeW50YXgtbnMjIj4gPHJkZjpEZXNjcmlwdGlvbiByZGY6YWJvdXQ9IiIgeG1sbnM6eG1wTU09Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC9tbS8iIHhtbG5zOnN0UmVmPSJodHRwOi8vbnMuYWRvYmUuY29tL3hhcC8xLjAvc1R5cGUvUmVzb3VyY2VSZWYjIiB4bWxuczp4bXA9Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC8iIHhtcE1NOk9yaWdpbmFsRG9jdW1lbnRJRD0ieG1wLmRpZDpCOTYwQTAyNzRDODFFQTExQThGN0Q2QUM3NThGMzQ1RSIgeG1wTU06RG9jdW1lbnRJRD0ieG1wLmRpZDo3Q0Q1QUU1QkM2RjgxMUVEQUFGOEVFQTZEMzc1RjFFRCIgeG1wTU06SW5zdGFuY2VJRD0ieG1wLmlpZDo3Q0Q1QUU1QUM2RjgxMUVEQUFGOEVFQTZEMzc1RjFFRCIgeG1wOkNyZWF0b3JUb29sPSJBZG9iZSBQaG90b3Nob3AgQ1M2IChXaW5kb3dzKSI+IDx4bXBNTTpEZXJpdmVkRnJvbSBzdFJlZjppbnN0YW5jZUlEPSJ4bXAuaWlkOkZEM0IwMDRCQ0I5NkVBMTE4QjIyQkUyNDQyOEU0RTNFIiBzdFJlZjpkb2N1bWVudElEPSJ4bXAuZGlkOkI5NjBBMDI3NEM4MUVBMTFBOEY3RDZBQzc1OEYzNDVFIi8+IDwvcmRmOkRlc2NyaXB0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'>
            <h2>Local Valve Control</h2>
        </div>
    </div>

    <div class='card row'>
        <button id='save_all_btn' class='secondary'>Save All Programs</button>
        <button id='sync_now_btn' class='secondary'>Sync Now</button>
        <button id='refresh_btn' class='secondary'>Refresh</button>
    </div>

    <div id='sync_status' class='sync-status'>Schedule source: loading...</div>
    <div id='notice' class='notice'></div>

    <div id='valves_container'></div>

    <script>
        let state = null;

        function q(id) { return document.getElementById(id); }
        function enc(obj) { return Object.keys(obj).map(k => encodeURIComponent(k) + '=' + encodeURIComponent(obj[k])).join('&'); }

        function setNotice(msg, isError) {
            q('notice').textContent = msg || '';
            q('notice').className = isError ? 'notice err' : 'notice ok';
        }

        async function postWithFallback(endpoints, options) {
            let lastErrorText = '';
            for (const ep of endpoints) {
                try {
                    const resp = await fetch(ep, options);
                    if (resp.ok) {
                        return resp;
                    }
                    lastErrorText = await resp.text();
                    if (!(resp.status === 404 || /nothing matches the given URI/i.test(lastErrorText || ''))) {
                        throw new Error(lastErrorText || `HTTP ${resp.status}`);
                    }
                } catch (e) {
                    if (e instanceof Error) {
                        lastErrorText = e.message;
                    }
                }
            }
            throw new Error(lastErrorText || 'No matching endpoint available');
        }

        function createProgramRow(valveId, p) {
            return `
            <tr>
                <td>${p.index + 1}</td>
                <td><input type='checkbox' data-v='${valveId}' data-p='${p.index}' data-k='enabled' ${p.enabled ? 'checked' : ''}></td>
                <td><input type='number' min='0' max='23' value='${p.hour}' data-v='${valveId}' data-p='${p.index}' data-k='hour' style='width:62px'></td>
                <td><input type='number' min='0' max='59' value='${p.minute}' data-v='${valveId}' data-p='${p.index}' data-k='minute' style='width:62px'></td>
                <td><input type='number' min='1' max='86400' value='${p.duration_sec}' data-v='${valveId}' data-p='${p.index}' data-k='duration_sec' style='width:92px'></td>
            </tr>`;
        }

        function render() {
            if (!state || !Array.isArray(state.valves)) {
                q('valves_container').innerHTML = '';
                return;
            }

            const source = state.schedule_source || 'unknown';
            const pending = !!state.local_programs_pending_upload;
            const cls = pending ? 'sync-local' : 'sync-cloud';
            const label = pending
                ? 'Local pending upload (will be sent on next cloud sync)'
                : 'Cloud authoritative (mirrored locally)';

            if (state.local_mode_boot_selected) {
                q('sync_status').innerHTML = "Mode: <span class='sync-cloud'>Local AP Mode</span> - cloud sync controls are hidden.";
                q('sync_now_btn').style.display = 'none';
            } else {
                q('sync_status').innerHTML = `Schedule source: <span class='${cls}'>${label}</span> [${source}]`;
                q('sync_now_btn').style.display = state.cloud_available ? '' : 'none';
            }

            const html = state.valves.map(v => {
                const statusClass = v.status === 'open' ? 'status-open' : 'status-closed';
                const rows = (v.programs || []).map(p => createProgramRow(v.id, p)).join('');
                return `
                    <div class='card'>
                        <div class='valve-title'>
                            <strong>Valve ${v.id + 1}</strong>
                            <span class='${statusClass}'>${v.status} (remaining ${v.manual_remaining_sec || 0}s)</span>
                        </div>
                        <div class='row' style='margin-top:8px'>
                            <input type='number' min='1' value='300' id='dur_${v.id}' style='width:100px'>[s]
                            <button onclick='manualStart(${v.id})'>Start</button>
                            <button class='secondary' onclick='manualStop(${v.id})'>Stop</button>
                            <button class='secondary' onclick='saveValve(${v.id})'>Save Valve Programs</button>
                        </div>
                        <table>
                            <thead><tr><th>#</th><th>Enabled</th><th>Hour</th><th>Minute</th><th>Duration(s)</th></tr></thead>
                            <tbody>${rows}</tbody>
                        </table>
                    </div>`;
            }).join('');

            q('valves_container').innerHTML = html;
        }

        function collectValvePrograms(valveId) {
            const items = [];
            for (let i = 0; i < 8; i++) {
                const map = {};
                map.index = i;
                ['enabled', 'hour', 'minute', 'duration_sec'].forEach(k => {
                    const el = document.querySelector(`[data-v='${valveId}'][data-p='${i}'][data-k='${k}']`);
                    if (!el) return;
                    if (k === 'enabled') map[k] = el.checked ? 1 : 0;
                    else map[k] = Number(el.value || 0);
                });
                map.days_mask = 0x7F;
                items.push(map);
            }
            return items;
        }

        async function fetchStateText() {
            const endpoints = ['/local_mode/state', '/state'];
            for (const ep of endpoints) {
                try {
                    const resp = await fetch(ep);
                    if (!resp.ok) {
                        continue;
                    }
                    const text = await resp.text();
                    const first = (text || '').trim().charAt(0);
                    if (first === '{' || first === '[') {
                        return text;
                    }
                } catch (e) {
                }
            }
            throw new Error('State endpoint did not return JSON');
        }

        async function refresh() {
            try {
                const text = await fetchStateText();
                state = JSON.parse(text);
                render();
                setNotice('');
            } catch (e) {
                setNotice('Failed to load state: ' + e, true);
            }
        }

        async function postPrograms(payload, okMessage) {
            await postWithFallback(['/local_mode/programs', '/programs'], {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });
            setNotice(okMessage || 'Programs saved.', false);
            await refresh();
        }

        async function saveValve(valveId) {
            try {
                await postPrograms({
                    local_mode_enabled: 1,
                    valves: [{ id: valveId, programs: collectValvePrograms(valveId) }]
                }, `Valve ${valveId + 1} programs saved.`);
            } catch (e) {
                setNotice('Save failed: ' + e, true);
            }
        }

        async function saveAll() {
            const valves = (state.valves || []).map(v => ({ id: v.id, programs: collectValvePrograms(v.id) }));
            try {
                await postPrograms({
                    local_mode_enabled: 1,
                    valves
                }, 'All programs saved.');
            } catch (e) {
                setNotice('Save failed: ' + e, true);
            }
        }

        async function manualStart(valveId) {
            const rawDuration = Number(q(`dur_${valveId}`).value || 0);
            const duration = Number.isFinite(rawDuration) ? Math.max(1, Math.floor(rawDuration)) : 0;
            if (duration <= 0) {
                setNotice('Invalid duration.', true);
                return;
            }

            try {
                await postWithFallback(['/local_mode/manual_run', '/manual_run'], {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: enc({ valve_index: valveId, duration_sec: duration })
                });
                setNotice(`Valve ${valveId + 1} started for ${duration}s.`, false);
                await refresh();
            } catch (e) {
                setNotice('Start failed: ' + e, true);
            }
        }

        async function manualStop(valveId) {
            try {
                await postWithFallback(['/local_mode/stop_run', '/stop_run'], {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: enc({ valve_index: valveId })
                });
                setNotice(`Valve ${valveId + 1} stopped.`, false);
                await refresh();
            } catch (e) {
                setNotice('Stop failed: ' + e, true);
            }
        }

        async function syncNow() {
            try {
                await postWithFallback(['/sync_now', '/local_mode/sync_now'], {
                    method: 'POST'
                });
                setNotice('Sync requested.', false);
                await refresh();
            } catch (e) {
                setNotice('Sync failed: ' + e, true);
            }
        }

        window.manualStart = manualStart;
        window.manualStop = manualStop;
        window.saveValve = saveValve;

        q('save_all_btn').addEventListener('click', saveAll);
        q('sync_now_btn').addEventListener('click', syncNow);
        q('refresh_btn').addEventListener('click', refresh);
        refresh();
    </script>
</body>
</html>
)HTML";

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

esp_err_t LocalModeWebServer::state_get_handler(httpd_req_t* req)
{
    auto* self = static_cast<LocalModeWebServer*>(req->user_ctx);
    if (self == nullptr || !self->state_callback_)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "State callback not set");
        return ESP_FAIL;
    }

    const std::string payload = self->state_callback_();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, payload.c_str(), payload.size());
}

esp_err_t LocalModeWebServer::programs_get_handler(httpd_req_t* req)
{
    auto* self = static_cast<LocalModeWebServer*>(req->user_ctx);
    if (self == nullptr || !self->programs_get_callback_)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Programs callback not set");
        return ESP_FAIL;
    }

    const std::string payload = self->programs_get_callback_();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, payload.c_str(), payload.size());
}

esp_err_t LocalModeWebServer::programs_post_handler(httpd_req_t* req)
{
    auto* self = static_cast<LocalModeWebServer*>(req->user_ctx);
    if (self == nullptr || !self->programs_callback_)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Programs callback not set");
        return ESP_FAIL;
    }

    std::string body;
    if (!read_request_body(req, body, 8192U))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_FAIL;
    }

    if (!self->programs_callback_(body))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid programs payload");
        return ESP_FAIL;
    }

    return httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
}

esp_err_t LocalModeWebServer::manual_run_post_handler(httpd_req_t* req)
{
    auto* self = static_cast<LocalModeWebServer*>(req->user_ctx);
    if (self == nullptr || !self->manual_run_callback_)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Manual run callback not set");
        return ESP_FAIL;
    }

    std::string body;
    if (!read_request_body(req, body, 512U))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_FAIL;
    }

    const std::string valve_text = form_value(body, "valve_index");
    const std::string duration_text = form_value(body, "duration_sec");
    if (valve_text.empty() || duration_text.empty())
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing valve_index or duration_sec");
        return ESP_FAIL;
    }

    const uint32_t valve_index = static_cast<uint32_t>(std::strtoul(valve_text.c_str(), nullptr, 10));
    const uint32_t duration_sec = static_cast<uint32_t>(std::strtoul(duration_text.c_str(), nullptr, 10));
    if (!self->manual_run_callback_(valve_index, duration_sec))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Manual run rejected");
        return ESP_FAIL;
    }

    return httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
}

esp_err_t LocalModeWebServer::stop_run_post_handler(httpd_req_t* req)
{
    auto* self = static_cast<LocalModeWebServer*>(req->user_ctx);
    if (self == nullptr || !self->stop_run_callback_)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Stop run callback not set");
        return ESP_FAIL;
    }

    std::string body;
    if (!read_request_body(req, body, 512U))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_FAIL;
    }

    const std::string valve_text = form_value(body, "valve_index");
    if (valve_text.empty())
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing valve_index");
        return ESP_FAIL;
    }

    const uint32_t valve_index = static_cast<uint32_t>(std::strtoul(valve_text.c_str(), nullptr, 10));
    if (!self->stop_run_callback_(valve_index))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Stop run rejected");
        return ESP_FAIL;
    }

    return httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
}

esp_err_t LocalModeWebServer::sync_now_post_handler(httpd_req_t* req)
{
    auto* self = static_cast<LocalModeWebServer*>(req->user_ctx);
    if (self == nullptr || !self->sync_now_callback_)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Sync callback not set");
        return ESP_FAIL;
    }

    if (!self->sync_now_callback_())
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Sync not available");
        return ESP_FAIL;
    }

    return httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
}

} // namespace app::local_mode
