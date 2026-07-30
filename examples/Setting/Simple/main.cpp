#include <Async.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";

Setting<int> intValue(0, "intValue");
Setting<double> doubleValue(0, "doubleValue");
Setting<float> floatValue(0, "floatValue");
Setting<bool> booleanValue(false, "booleanValue");
Setting<String> stringValue("Hello world", "stringValue");

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);
  
  initAsync();

  // int
  intValue.onChange([](int prev, int current) {
    ESP_LOGI(TAG_MAIN, "onChange 1 int value: prev %d, current %d", prev, current);
  });

  intValue.onChange([](int prev, int current) {
    ESP_LOGI(TAG_MAIN, "onChange 2 int value: prev %d, current %d", prev, current);
  });

  ESP_LOGI(TAG_MAIN, "int value %d", intValue.set([](int prev) {
    return prev + 1;
  }));

  // double
  doubleValue.onChange([](double prev, double current) {
    ESP_LOGI(TAG_MAIN, "onChange 1 double value: prev %f, current %f", prev, current);
  });

  doubleValue.set(2);

  // float
  floatValue.onChange([](float prev, float current) {
    ESP_LOGI(TAG_MAIN, "onChange 1 float value: prev %f, current %f", prev, current);
  });

  floatValue.set(2);

  // boolean
  booleanValue.onChange([](bool prev, bool current) {
    ESP_LOGI(TAG_MAIN, "onChange 1 boolean value: prev %d, current %d", prev, current);
  });

  booleanValue.set(true);

  // String
  stringValue.onChange([](String prev, String current) {
    ESP_LOGI(TAG_MAIN, "onChange 1 String value: prev %s, current %s", prev.c_str(), current.c_str());
  });

  stringValue.set(stringValue + "!!!");

  startAsync();
};

void loop() {
  vTaskDelete(NULL);
}