#include <Async.h>
#include <Async/HttpServer.h>

using namespace async;
static const char *TAG_MAIN = "MAIN";

HttpServer http(80);

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);
  esp_log_level_set(TAG_INTERRUPT, ESP_LOG_VERBOSE);
  esp_log_level_set(TAG_PIN, ESP_LOG_VERBOSE);

  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());

  initAsync();

  http.get("/", [] (HttpRequest & req, HttpResponse & res) {
    return res.status(200).send("Hello world!");
  });

  startAsync();
}

void loop() {
  vTaskDelete(NULL);
}