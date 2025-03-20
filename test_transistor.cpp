#include <pigpio.h>
#include <iostream>
#include <thread>
#include <chrono>

#define GPIO_PIN 26
#define DELAY_SECONDS 1

int main() {
    if (gpioInitialise() < 0) {
        std::cerr << "Failed to initialize pigpio." << std::endl;
        return 1;
    }

    gpioSetMode(GPIO_PIN, PI_OUTPUT);

    while (true) {
        gpioWrite(GPIO_PIN, 1);  // Enable GPIO
        std::cout << "GPIO " << GPIO_PIN << " ON" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(DELAY_SECONDS));

        gpioWrite(GPIO_PIN, 0);  // Disable GPIO
        std::cout << "GPIO " << GPIO_PIN << " OFF" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(DELAY_SECONDS));
    }

    gpioTerminate();
    return 0;
}
