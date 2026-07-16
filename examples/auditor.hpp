#pragma once
#include <string>

#include "event_context.hpp"

class Auditor {
    public:
    virtual ~Auditor() = default;
    virtual void start(const std::string& name, const EventContext& ctx) = 0;
    virtual void finish(const std::string& name, const EventContext& ctx) = 0;
};
