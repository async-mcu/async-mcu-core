#pragma once

// Генерируем выражение с || для всех элементов
#define BOOL_OR_1(a) (a[0])
#define BOOL_OR_2(a) (a[0] || a[1])
#define BOOL_OR_3(a) (a[0] || a[1] || a[2])
#define BOOL_OR_4(a) (a[0] || a[1] || a[2] || a[3])
#define BOOL_OR_N(n, a) BOOL_OR_##n(a)
#define BOOL_OR(a, n) BOOL_OR_N(n, a)

// Генерируем выражение с && для всех элементов
#define BOOL_AND_1(a) (a[0])
#define BOOL_AND_2(a) (a[0] && a[1])
#define BOOL_AND_3(a) (a[0] && a[1] && a[2])
#define BOOL_AND_4(a) (a[0] && a[1] && a[2] && a[3])
#define BOOL_AND_N(n, a) BOOL_AND_##n(a)
#define BOOL_AND(a, n) BOOL_AND_N(n, a)

// Минимум в массиве
#define MIN_IN_ARRAY_1(arr) (arr[0])
#define MIN_IN_ARRAY_2(arr) ((arr)[0] < (arr)[1] ? (arr)[0] : (arr)[1])
#define MIN_IN_ARRAY_3(arr) MIN_IN_ARRAY_2(arr) < (arr)[2] ? MIN_IN_ARRAY_2(arr) : (arr)[2]
#define MIN_IN_ARRAY_4(arr) MIN_IN_ARRAY_2(arr) < MIN_IN_ARRAY_2(&(arr)[2]) ? \
                            MIN_IN_ARRAY_2(arr) : MIN_IN_ARRAY_2(&(arr)[2])
#define MIN_IN_ARRAY_N(n, arr) MIN_IN_ARRAY_##n(arr)
#define MIN_IN_ARRAY(arr, n) MIN_IN_ARRAY_N(n, arr)

// Инициализация массива одинаковыми значениями
#define INIT_ARRAY_1(val) {val}
#define INIT_ARRAY_2(val) {val, val}
#define INIT_ARRAY_3(val) {val, val, val}
#define INIT_ARRAY_4(val) {val, val, val, val}
#define INIT_ARRAY_5(val) {val, val, val, val, val}
#define INIT_ARRAY_6(val) {val, val, val, val, val, val}
#define INIT_ARRAY_N(n, val) INIT_ARRAY_##n(val)
#define INIT_ARRAY(val, n) INIT_ARRAY_N(n, val)

// Определения для асинхронного выполнения задач
namespace async {
  enum Mode { 
    None,
    Deep, 
    Light, 
    Active
  };

  enum Type {
    REPEAT = 0,
    DELAY = 10,
    DEMAND = 20,
    TICK = 30,
    ONCE = 40,
    INTERR = 50
   };

  enum Core {
    CORE0 = 0,
    CORE1 = 1
  };

  char const* modeToStr(Mode mode) {
    switch(mode) {
      case None: return "None";
      case Deep: return "Deep";
      case Light: return "Light";
      case Active: return "Active";
      default: return "Unknown";
    }
  }

  char const* typeToStr(Type type) {
    switch(type) {
      case REPEAT: return "REPEAT";
      case DELAY: return "DELAY";
      case DEMAND: return "DEMAND";
      case TICK: return "TICK";
      case ONCE: return "ONCE";
      case INTERR: return "INTERR";
      default: return "Unknown";
    }
  }
}