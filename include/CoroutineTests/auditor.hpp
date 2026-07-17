#pragma once

#include <string>

#include "event_context.hpp"

namespace CoroutineTests {

class Auditor {
    public:
    virtual ~Auditor() = default;
    virtual void start(const std::string& name, const EventContext& ctx) = 0;
    virtual void stop(const std::string& name, const EventContext& ctx) = 0;
};
}  // namespace CoroutineTests
