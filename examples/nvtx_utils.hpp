#pragma once

#include <nvtx3/nvtx3.hpp>

namespace CoroutineTests::nvtx_utils {
namespace detail {
struct domain {
    static constexpr char const* name{"CoroutineTests"};
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

inline nvtx3::event_attributes get_attributes(std::string name, uint i) {
    return nvtx3::event_attributes{get_category(), get_color(i), name,
                                   nvtx3::payload{i}};
}
}  // namespace detail
inline detail::range_t make_range(std::string name, uint i) {
    return detail::range_t(name, detail::get_category(), detail::get_color(i),
                           nvtx3::payload{i});
}

inline detail::range_t make_range(std::string name) {
    return detail::range_t(name);
}

}  // namespace CoroutineTests::nvtx_utils
