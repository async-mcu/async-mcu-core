#include <async/HttpServer.h>
#include <mbedtls/base64.h>
#include <async/Executor.h>

namespace async {

// --- Сессии ---
class SimpleSession : public IHttpSession {
    std::map<std::string, std::string> _data;
public:
    void set(const std::string& k, const std::string& v) override { _data[k] = v; }
    std::string get(const std::string& k) override { return _data[k]; }
    bool has(const std::string& k) override { return _data.count(k); }
};

IHttpSession& HttpRequest::getSession() {
    if (!_session) _session = std::make_shared<SimpleSession>();
    return *_session;
}

// --- HttpResponse Реализация ---
const char* _map_status(int code) {
    switch(code) {
        case 200: return "200 OK";
        case 201: return "201 Created";
        case 204: return "204 No Content";
        case 302: return "302 Found";
        case 400: return "400 Bad Request";
        case 401: return "401 Unauthorized";
        case 403: return "403 Forbidden";
        case 404: return "404 Not Found";
        default: return "500 Internal Server Error";
    }
}

HttpResponse& HttpResponse::status(int code) {
    httpd_resp_set_status(_req, _map_status(code));
    return *this;
}

HttpResponse& HttpResponse::header(const std::string& name, const std::string& value) {
    httpd_resp_set_hdr(_req, name.c_str(), value.c_str());
    return *this;
}

HttpResponse& HttpResponse::type(const std::string& mimeType) {
    httpd_resp_set_type(_req, mimeType.c_str());
    return *this;
}

HttpResponse& HttpResponse::cookie(const std::string& name, const std::string& value, int max_age) {
    std::string c = name + "=" + value + "; Max-Age=" + std::to_string(max_age) + "; Path=/; HttpOnly";
    return header("Set-Cookie", c);
}

esp_err_t HttpResponse::send(const std::string& text) {
    return httpd_resp_sendstr(_req, text.c_str());
}

esp_err_t HttpResponse::sendJson(const HttpJson& obj) {
    type("application/json");
    return send(obj.toJson());
}

esp_err_t HttpResponse::sendBinary(const uint8_t* data, size_t len) {
    return httpd_resp_send(_req, (const char*)data, len);
}

esp_err_t HttpResponse::sendFile(const std::string& path, std::function<void(size_t, size_t)> progress) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return status(404).send("File Not Found");
    fseek(f, 0, SEEK_END);
    size_t total = ftell(f);
    fseek(f, 0, SEEK_SET);
    char buf[1024]; size_t read, sent = 0;
    while ((read = fread(buf, 1, sizeof(buf), f)) > 0) {
        chunk(buf, read); sent += read;
        if (progress) progress(sent, total);
    }
    fclose(f);
    return end();
}

esp_err_t HttpResponse::redirect(const std::string& target) {
    return status(302).header("Location", target).send("");
}

esp_err_t HttpResponse::chunk(const void* data, size_t len) {
    return httpd_resp_send_chunk(_req, (const char*)data, len);
}

esp_err_t HttpResponse::end() {
    return httpd_resp_send_chunk(_req, NULL, 0);
}

// --- HttpRequest Реализация ---

std::string HttpRequest::header(const std::string& name) const {
    size_t len = httpd_req_get_hdr_value_len(_req, name.c_str());
    if (len == 0) return "";
    std::vector<char> buf(len + 1);
    httpd_req_get_hdr_value_str(_req, name.c_str(), buf.data(), len + 1);
    return std::string(buf.data());
}

std::string HttpRequest::method() const {
    switch(_req->method) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        default: return "UNKNOWN";
    }
}

std::string HttpRequest::body() {
    if (!_cached_body.empty()) return _cached_body;
    if (_req->content_len == 0) return "";

    std::string body;
    body.reserve(_req->content_len);

    size_t remaining = _req->content_len;
    std::vector<char> chunk(256);
    while (remaining > 0) {
        size_t want = remaining < chunk.size() ? remaining : chunk.size();
        int ret = httpd_req_recv(_req, chunk.data(), want);
        if (ret <= 0) break; // error or timeout
        body.append(chunk.data(), ret);
        remaining -= ret;
    }

    _cached_body = std::move(body);
    return _cached_body;
}

// --- Server Реализация ---

esp_err_t HttpServer::_global_handler(httpd_req_t* req) {
    auto* rd = (RouteData*)req->user_ctx;
    HttpRequest request(req);
    HttpResponse response(req);

    for (auto& filter : rd->server->_filters) {
        if (!filter(request, response)) return ESP_OK;
    }

    esp_err_t res = rd->handler(request, response);
    if (!request.isValid()) {
        return response.status(400).send("Bad Request: Data Invalid");
    }
    return res;
}

HttpServer::HttpServer(int port) {
    onInit(CURRENT_CORE, [port, this] (Task &) {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.server_port = port;
        config.uri_match_fn = httpd_uri_match_wildcard;
        esp_err_t err = httpd_start(&_server, &config);
        
        if (err != ESP_OK) {
            ESP_LOGE(TAG_HTTP_SERVER, "Failed to start HTTP server: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG_HTTP_SERVER, "HTTP server started on port %d", port);
            for (const auto& route : _pendingRoutes) {
                httpd_register_uri_handler(_server, &route);
            }
            _pendingRoutes.clear();
        }
        
    });
}

void HttpServer::registerOrDefer(const httpd_uri_t& route) {
    if (_server) {
        httpd_register_uri_handler(_server, &route);
    } else {
        _pendingRoutes.push_back(route);
    }
}

void HttpServer::get(const char* uri, HandlerFunc h) {
    httpd_uri_t r = {uri, HTTP_GET, _global_handler, new RouteData{h, this}};
    registerOrDefer(r);
}

void HttpServer::post(const char* uri, HandlerFunc h) {
    httpd_uri_t r = {uri, HTTP_POST, _global_handler, new RouteData{h, this}};
    registerOrDefer(r);
}

std::vector<int> HttpServer::wsClients() {
    if (!_server) return {};
    const size_t CAP = 16;
    int fds[CAP];
    size_t count = CAP;
    if (httpd_get_client_list(_server, &count, fds) == ESP_OK) {
        size_t n = count < CAP ? count : CAP;
        return std::vector<int>(fds, fds + n);
    }
    return {};
}

// --- WS Реализация ---
esp_err_t WsResponse::send(const std::string& text) {
    httpd_ws_frame_t f = {.final=true, .type=HTTPD_WS_TYPE_TEXT, .payload=(uint8_t*)text.c_str(), .len=text.length()};
    return httpd_ws_send_frame(_req, &f);
}

esp_err_t HttpServer::_global_ws_handler(httpd_req_t* req) {
    auto* wd = (WsData*)req->user_ctx;
    WsResponse res(req); WsRequest r{.raw_req = req};
    if (req->method == HTTP_GET) {
        r.type = WsEventType::CONNECT; wd->handler(r, res);
        return ESP_OK;
    }
    httpd_ws_frame_t frame = {};
    // первый вызов определяет размер payload
    if (httpd_ws_recv_frame(req, &frame, 0) == ESP_OK) {
        std::vector<uint8_t> buf(frame.len ? frame.len : 1);
        frame.payload = buf.data();
        if (httpd_ws_recv_frame(req, &frame, buf.size()) == ESP_OK) {
            r.type = (frame.type == HTTPD_WS_TYPE_TEXT) ? WsEventType::TEXT : WsEventType::BINARY;
            r.data.assign(buf.data(), buf.data() + frame.len);
            wd->handler(r, res);
        }
    }
    return ESP_OK;
}

void HttpServer::ws(const char* uri, WsHandlerFunc h) {
    httpd_uri_t r = {uri, HTTP_GET, _global_ws_handler, new WsData{h}, true};
    registerOrDefer(r);
}

// --- Фильтр Auth ---
BasicAuthFilter::BasicAuthFilter(const std::string& u, const std::string& p) {
    std::string r = u + ":" + p;
    size_t outLen = 4 * ((r.length() + 2) / 3) + 1; // размер base64-буфера
    std::vector<unsigned char> b(outLen);
    size_t o = 0;
    if (mbedtls_base64_encode(b.data(), b.size(), &o, (const unsigned char*)r.c_str(), r.length()) == 0) {
        _auth_header = "Basic " + std::string((char*)b.data(), o);
    } else {
        _auth_header = "Basic ";
    }
}

bool BasicAuthFilter::operator()(HttpRequest& req, HttpResponse& res) {
    if (req.header("Authorization") != _auth_header) {
        res.status(401).header("WWW-Authenticate", "Basic realm=\"ESP32\"").send("Auth required");
        return false;
    }
    return true;
}


}