#pragma once

#include <cstdint>
#include <functional>
#include <string>

extern "C" {
#include "esp_http_server.h"
}

namespace app::local_mode {

class LocalModeWebServer {
public:
    LocalModeWebServer() = default;
    ~LocalModeWebServer() = default;

    void setStateCallback(const std::function<std::string()>& callback);
    void setProgramsGetCallback(const std::function<std::string()>& callback);
    void setProgramsCallback(const std::function<bool(const std::string&)>& callback);
    void setManualRunCallback(const std::function<bool(uint32_t, uint32_t)>& callback);
    void setStopRunCallback(const std::function<bool(uint32_t)>& callback);
    void setSyncNowCallback(const std::function<bool()>& callback);

    bool start(uint16_t port = 8080U);
    void stop();
    bool isRunning() const { return server_ != nullptr; }

private:
    httpd_handle_t server_ = nullptr;

    std::function<std::string()> state_callback_;
    std::function<std::string()> programs_get_callback_;
    std::function<bool(const std::string&)> programs_callback_;
    std::function<bool(uint32_t, uint32_t)> manual_run_callback_;
    std::function<bool(uint32_t)> stop_run_callback_;
    std::function<bool()> sync_now_callback_;

    static esp_err_t root_get_handler(httpd_req_t* req);
    static esp_err_t state_get_handler(httpd_req_t* req);
    static esp_err_t programs_get_handler(httpd_req_t* req);
    static esp_err_t programs_post_handler(httpd_req_t* req);
    static esp_err_t manual_run_post_handler(httpd_req_t* req);
    static esp_err_t stop_run_post_handler(httpd_req_t* req);
    static esp_err_t sync_now_post_handler(httpd_req_t* req);
};

} // namespace app::local_mode
