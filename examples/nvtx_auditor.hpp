#pragma once

#include <nvtx3/nvToolsExt.h>

#include "CoroutineTests/auditor.hpp"
#include "CoroutineTests/event_context.hpp"
#include "nvtx_utils.hpp"

class NvtxAuditor : public CoroutineTests::Auditor {
    public:
    void start(const std::string& name,
               const CoroutineTests::EventContext& ctx) override {
        const auto attr = CoroutineTests::nvtx_utils::detail::get_attributes(
            name, ctx.event_id);
        nvtxDomainRangePushEx(
            nvtx3::domain::get<CoroutineTests::nvtx_utils::detail::domain>(),
            attr.get());
    }

    void stop(const std::string& /*name*/,
              const CoroutineTests::EventContext& /*ctx*/) override {
        nvtxDomainRangePop(
            nvtx3::domain::get<CoroutineTests::nvtx_utils::detail::domain>());
    }

    private:
};
