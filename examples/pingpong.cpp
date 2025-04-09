#include "CoroutineTests/pingpong.hpp"
#include <iostream>

CoroutineTests::Player pong() {
    auto counter = 0;
    while (true) {
        ++counter;
        std::cout << "Pong " << counter << '\n';
        co_await CoroutineTests::Play{};
    }
}

CoroutineTests::Player ping() {
    auto peer = pong();
    auto counter = 1;
    std::cout << "Ping " << counter << '\n';
    co_await peer;
    while (true) {
        ++counter;
        std::cout << "Ping " << counter << '\n';
        co_await CoroutineTests::Play{};
    }
}

int main() {
    std::cout << "Ping Pong example\n";
    auto ping_task = ping();
    ping_task.start();
    return 0;
}
