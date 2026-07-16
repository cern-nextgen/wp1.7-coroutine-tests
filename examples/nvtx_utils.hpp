#pragma once

#include "nvtx3/nvtx3.hpp"

namespace CoroutineTests::nvtx_utils {
namespace detail {
struct domain {
    static constexpr char const* name{"WP1.7"};
};

using range_t = nvtx3::scoped_range_in<domain>;

inline nvtx3::named_category_in<domain>& get_category() {
    static auto category = nvtx3::named_category_in<domain>{42, "Algorithms"};
    return category;
}

inline nvtx3::rgb get_color(uint i) {
    using component_t = typename nvtx3::rgb::component_type;
    const auto CERNBlue = nvtx3::rgb{0, 51, 160};
    component_t r = ((i * 23) + CERNBlue.red) & 0xFF;
    component_t g = ((i * 47) + CERNBlue.green) & 0xFF;
    component_t b = ((i * 71) + CERNBlue.blue) & 0xFF;
    return nvtx3::rgb{r, g, b};
}
}  // namespace detail
inline detail::range_t make_range(std::string name, uint i=0) {
    return detail::range_t(name, detail::get_category(), detail::get_color(i), nvtx3::payload{i});
}

inline void make_c_range(std::string name, uint i=0) {
    auto attr = nvtxEventAttributes_t{};
    attr.version = NVTX_VERSION;
    attr.size = NVTX_EVENT_ATTRIB_STRUCT_SIZE;
    attr.colorType = NVTX_COLOR_ARGB;
    attr.color = 0xFF000000 | (detail::get_color(i).red << 16) |
                 (detail::get_color(i).green << 8) | detail::get_color(i).blue;
    attr.messageType = NVTX_MESSAGE_TYPE_ASCII;
    attr.message.ascii = name.c_str();
    nvtxRangePushEx(&attr);
}

}  // namespace CoroutineTests::nvtx_utils
