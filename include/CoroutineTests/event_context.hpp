#pragma once
#include <cstddef>
namespace CoroutineTests {

struct EventContext {
    std::size_t event_id;  // id of the event
    std::size_t slot_id;   // id of the concurrent slot occupied by the event
};

}  // namespace CoroutineTests
