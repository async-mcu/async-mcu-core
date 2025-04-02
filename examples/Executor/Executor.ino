#include <Arduino.h>
#include <async/Log.h>
#include <async/Task.h>
#include <async/Duration.h>
#include <async/Executor.h>

using namespace async;

Executor core1;

void setup() {
  Serial.begin(115200); 
 
  Duration * repeatTask1Duration = new Duration(1000);
  auto repeatTask1 = new Task(Task::REPEAT, repeatTask1Duration, [repeatTask1Duration]() {
    info("Repeat task 1, every %s ms", repeatTask1Duration->toString().c_str());
  });

  auto repeatTask2 = new Task(Task::REPEAT, new Duration(2000), []() {
    info("Repeat task 2, every 2000 ms");
  });

  auto delayTask1 = new Task(Task::DELAY, new Duration(5000), [repeatTask1Duration]() {
    info("Delayed task 1, after 5000ms, change repeatTask duration to 500ms");
    repeatTask1Duration->set(500);
  });

  auto delayTask2 = new Task(Task::DELAY, new Duration(10000), [repeatTask2]() {
    info("Delayed task 2, after 10000ms, cancel repeatTask2");
    repeatTask2->cancel();
  });

  Task * demandTask = new Task(Task::DEMAND, []() {
    info("Demand task, trigger after delayTask3");
  });

  auto delayTask3 = new Task(Task::DELAY, new Duration(10000), [demandTask]() {
    info("Delayed task 3, after 15000ms, demand demandTask");
    demandTask->demand();
  });

  //core1.add(delayTask);
  core1.add(repeatTask1);
  core1.add(repeatTask2);
  core1.add(delayTask1);
  core1.add(delayTask2);
  core1.add(delayTask3);
  core1.add(demandTask);
}

void loop() {
  core1.tick();
}