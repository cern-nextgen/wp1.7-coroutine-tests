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
    auto counter = 0;
    while (true) {
        ++counter;
        std::cout << "Ping " << counter << '\n';
        co_await CoroutineTests::Play{};
    }
}

int main() {
    std::cout << "Ping Pong example\n";
    auto ping_task = ping();
    auto pong_task = pong();
    ping_task.set_peer(pong_task);
    pong_task.set_peer(ping_task);
    ping_task.start();
    return 0;
}
