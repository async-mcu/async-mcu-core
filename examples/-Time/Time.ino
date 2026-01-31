#include <async/Time.h>

using namespace async;

void setup() {
  Serial.begin(115200);
  Time::setSystem(2025, 1, 10, 11, 12, 13, 567);

  Time beforeOverflowTime = Time::from(1970, 2, 19, 17, 2, 40, 678);
  Time afterOverflowTime = Time::from(1970, 2, 19, 17, 2, 48, 678);
  Time before10DaysOverflowTime = Time::from(1971, 5, 13, 2, 27, 46, 678);
  Time after10DaysOverflowTime = Time::from(1971, 5, 13, 2, 27, 55, 678);

  while(true) {
    Serial.print("System time ");
    Serial.println(Time::getSystem().toString().c_str());

    Serial.print("Before overflow time ");
    Serial.println(beforeOverflowTime.toString().c_str());

    Serial.print("After overflow time ");
    Serial.println(afterOverflowTime.toString().c_str());

    Serial.print("Before 10 days overflow time ");
    Serial.println(before10DaysOverflowTime.toString().c_str());

    Serial.print("After 10 days overflow time ");
    Serial.println(after10DaysOverflowTime.toString().c_str());

    delay(2000);
  }
}

void loop() {}