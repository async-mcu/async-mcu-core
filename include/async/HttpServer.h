#pragma once

#include <esp_http_server.h>
#include <esp_log.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <cJSON.h>

// --- Базовые типы и интерфейсы ---

namespace async {

class HttpJson {
public:
    virtual ~HttpJson() = default;
    // Возвращаем bool для проверки валидности JSON
    virtual bool fromJson(const std::string& json) = 0;
    virtual std::string toJson() const = 0;
};

class IHttpSession {
public:
    virtual ~IHttpSession() = default;
    virtual void set(const std::string& key, const std::string& value) = 0;
    virtual std::string get(const std::string& key) = 0;
    virtual bool has(const std::string& key) = 0;
};

enum class WsEventType { CONNECT, TEXT, BINARY, DISCONNECT };

struct WsRequest {
    WsEventType type;
    std::vector<uint8_t> data;
    httpd_req_t* raw_req;
};

class WsResponse {
    httpd_req_t* _req;
public:
    WsResponse(httpd_req_t* req) : _req(req) {}
    esp_err_t send(const std::string& text);
    esp_err_t sendBinary(const uint8_t* data, size_t len);
};

class HttpRequest {
    httpd_req_t* _req;
    bool _is_valid = true;
    std::string _cached_body;
    mutable std::shared_ptr<IHttpSession> _session;

public:
    HttpRequest(httpd_req_t* req) : _req(req) {}

    std::string header(const std::string& name) const;
    std::string cookie(const std::string& name) const;
    std::string query(const std::string& param) const;
    std::string method() const;
    std::string url() const { return std::string(_req->uri); }
    std::string body();
    
    template<typename T>
    T as() {
        static_assert(std::is_base_of<HttpJson, T>::value, "T must inherit from HttpJson");
        T obj;
        std::string b = body();
        if (b.empty() || !obj.fromJson(b)) _is_valid = false;
        return obj;
    }

    bool isValid() const { return _is_valid; }
    IHttpSession& getSession();
    httpd_req_t* raw() { return _req; }
};

class HttpResponse {
    httpd_req_t* _req;
public:
    HttpResponse(httpd_req_t* req) : _req(req) {}

    HttpResponse& status(int code);
    HttpResponse& header(const std::string& name, const std::string& value);
    HttpResponse& type(const std::string& mimeType);
    HttpResponse& cookie(const std::string& name, const std::string& value, int max_age = 3600);

    esp_err_t send(const std::string& text);
    esp_err_t sendJson(const HttpJson& obj);
    esp_err_t sendBinary(const uint8_t* data, size_t len);
    esp_err_t sendFile(const std::string& path, std::function<void(size_t, size_t)> progress = nullptr);
    esp_err_t redirect(const std::string& target);
    
    esp_err_t chunk(const void* data, size_t len);
    esp_err_t end();

    httpd_req_t* raw() { return _req; }
};

// --- Основной сервер ---

using HandlerFunc = std::function<esp_err_t(HttpRequest&, HttpResponse&)>;
using FilterFunc = std::function<bool(HttpRequest&, HttpResponse&)>;
using WsHandlerFunc = std::function<void(WsRequest&, WsResponse&)>;

class HttpServer {
    httpd_handle_t _server = nullptr;
    std::vector<FilterFunc> _filters;
    std::vector<httpd_uri_t> _pendingRoutes;

    void registerOrDefer(const httpd_uri_t& route);

    struct RouteData {
        HandlerFunc handler;
        HttpServer* server;
    };
    struct WsData {
        WsHandlerFunc handler;
    };

    static esp_err_t _global_handler(httpd_req_t* req);
    static esp_err_t _global_ws_handler(httpd_req_t* req);

public:
    HttpServer(int port = 80);
    void addFilter(FilterFunc f) { _filters.push_back(f); }
    void get(const char* uri, HandlerFunc h);
    void post(const char* uri, HandlerFunc h);
    void ws(const char* uri, WsHandlerFunc h);
    
    std::vector<int> wsClients();
};

// --- Встроенные фильтры ---

class BasicAuthFilter {
    std::string _auth_header;
public:
    BasicAuthFilter(const std::string& user, const std::string& pass);
    bool operator()(HttpRequest& req, HttpResponse& res);
};

}