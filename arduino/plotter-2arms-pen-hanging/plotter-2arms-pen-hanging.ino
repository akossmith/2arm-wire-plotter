// #define TESTING

constexpr unsigned long baudRate = 115200;

#ifdef TESTING
#include "tests_main.h"
#else
#include "app_main.h"
#endif