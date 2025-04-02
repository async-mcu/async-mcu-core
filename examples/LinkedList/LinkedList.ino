#include <Arduino.h>
#include <async/LinkedList.h>

void setup() {
    Serial.begin(9600);

    LinkedList<int> list;

    list.append(10);
    list.append(20);
    list.append(30);
    list.prepend(5);
    
    Serial.print("Size of the list: ");
    Serial.println(list.size());  // Output: 4

    Serial.print("Values: ");
    for(int i=0; i < list.size(); i++) {
        Serial.println(list.get(i));
    }

    Serial.print("Element at index 2: ");
    Serial.println(list.get(2));  // Output: 20

    Serial.print("Element at index 5: ");
    Serial.println(list.get(5));  // Output: 0 (default value for int)

    // Example with a custom type
    LinkedList<String> stringList;
    stringList.append("Hello");
    stringList.append("World");

    Serial.print("Element at index 1: ");
    Serial.println(stringList.get(1));  // Output: World

    Serial.print("Element at index 3: ");
    Serial.println(stringList.get(3));  // Output: (empty string, default value for String)
}

void loop() {
    // Empty loop
}