#include <async/Log.h>
#include <async/Chain.h>
#include <async/Boot.h>

using namespace async;

//Executor executor;

// void setup() {
//   Serial.begin(115200);
//   executor.start();

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
//       });
//       //->loop();

//       executor.add(ch);
// }

// void loop() {
//   executor.tick();
//   delay(1000);
// }

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