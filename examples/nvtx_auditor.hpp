#pragma once

#include "auditor.hpp"
#include "event_context.hpp"
#include "nvtx_utils.hpp"
#include "tbb/conc"

class NvtxAuditor : public Auditor {
    public:
    void start(const std::string& name, const EventContext& ctx) override {
        m_range = CoroutineTests::nvtx_utils::make_range(name, ctx.event_id);
    }

    void finish(const std::string& name, const EventContext& ctx) override {
        m_range.reset();
    }

    private:
    CoroutineTests::nvtx_utils::detail::range_t m_range;
};