#include <Arduino.h>
#include <async/Tick.h>
#include <async/Function.h>

using namespace async;




// Semaphore sem(2, 2);
// int i=0;

// void setup() {
//     Serial.begin(9600);
    
//     for (int i = 0; i < 8; i++) {
//         Chain<uint32_t>* chain = new Chain<uint32_t>(millis());
//         chain->semaphore(&sem)
//              ->then([](uint32_t val) {
//                 Serial.print("Start ");
//                 Serial.println(val);
//                 val++;
//                 Serial.print("Start2 ");
//                 Serial.println(val);
//                 return val;
//               })
//              ->delay(1000)
//              ->then([](uint32_t val) {
//                 Serial.print("End ");
//                 Serial.println(millis());
//                 Serial.print("End value ");
//                 Serial.println(val);
//                 sem.release();
//                 return val;
//              });
        
//         while (chain->tick()) {
//             delay(10);
//         }
//         delete chain;
//     }
// }

// void loop() {}