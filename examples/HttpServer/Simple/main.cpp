#include <Async.h>
#include <Async/HttpServer.h>
#include <WiFi.h>

using namespace async;
static const char *TAG_MAIN = "MAIN";
const char *ssid = "ap";
const char *password = "12345678";

HttpServer http(80);

void setup() {
  setCpuFrequencyMhz(80);
  setManualSleepMode(SleepMode::Active);

  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);
  esp_log_level_set(TAG_HTTP_SERVER, ESP_LOG_VERBOSE);
  esp_log_level_set(TAG_EXECUTOR, ESP_LOG_VERBOSE);

  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());

  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  ESP_LOGI(TAG_MAIN, "AP IP address: %s", IP.toString().c_str());

  initAsync();

  http.get("/", [] (HttpRequest & req, HttpResponse & res) {
    auto result = res.status(200).send("Hello world!");
    return result;
  });

  startAsync();
}

void loop() {
  vTaskDelete(NULL);
}