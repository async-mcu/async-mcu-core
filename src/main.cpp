#include <async/Log.h>
#include <async/Chain.h>
#include <async/Pin.h>
#include <async/Setting.h>

using namespace async;

Executor core1;
Pin pin(23, INPUT_PULLUP);

Setting<int> val("name", 0x0000, 10);

void setup() {
  Serial.begin(115200);
  Serial.println("setup");

  core1.start();

  auto delay = new DelayTask(1000, []() {
    info("Delay task executed");
  });

}

void loop() {
  core1.loop();
}

// Boot zBoot([](Executor * executor) {
//   Serial.begin(115200);

//   info("onRepeat");

//   executor->onRepeat(1000, []() {
//     info("onRepeat");
//   });

//   auto ch = chain(0)
//       ->delay(2000)
//       ->then([](int val) {
//         info("Start value %d", val);
//         return val+1;
//       })
//       ->then([](int val) {
//         info("Intermediate value %d", val);
//         return val+1;
//       })
//       ->delay(2000)
//       // ->animate(0, 100, Duration(2000), [](int current, int target) {
//       //   info("Animate value %d, value %d", current, target);
//       //   return target + current;
//       // })
//       ->then([](int val) {
//         info("Final value %d", val);
//         return val;
//       })
//       ->loop();

//   executor->onDelay(10000, [&ch]() {
//     ch->cancel();
//   });

//   executor->add(ch);
// });