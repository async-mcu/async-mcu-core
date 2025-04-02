#include <async/Duration.h>

using namespace async;

void setup() {
  Serial.begin(115200);

  // Create two durations
  auto duration1 = Duration(1500); // 1.5 seconds
  auto duration2 = Duration(3000); // 3 seconds

  // Addition
  auto sum = duration1.add(duration2);
  Serial.print("Sum: ");
  Serial.print(sum.toString().c_str()); // 4.5 seconds (4500 ms)
  Serial.println(" seconds");

  // Subtraction
  auto diff = duration2.subtract(duration1);
  Serial.print("Difference: ");
  Serial.print(diff.toString().c_str()); // 1.5 seconds (1500 ms)
  Serial.println(" seconds");

  // Check if 3 seconds have passed
  if (duration2.after(duration1)) {
    Serial.println("3 seconds have not yet passed.");
  } else {
    Serial.println("3 seconds have passed.");
  }
  
  // Check if 3 seconds have passed
  if (duration1.before(duration2)) {
    Serial.println("1.5 seconds have not yet passed.");
  } else {
    Serial.println("3 seconds have passed.");
  }
}

void loop() {
  auto from = Duration::now();
  delay(random(1000, 2000));

  Serial.print("Difference random: ");
  Serial.print(Duration::now().subtract(from).toString().c_str()); 
  Serial.println(" millis");
}