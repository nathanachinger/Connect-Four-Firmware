#include "app_main.h"
#include "hw_test.hpp"

extern "C" void app_main(void) {
    hw_test::run_all();
}
