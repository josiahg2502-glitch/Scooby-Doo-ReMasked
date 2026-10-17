#include "runtime_product.h"

namespace RuntimeProduct {

const Descriptor& Active() noexcept {
    static constexpr Descriptor descriptor{
        Kind::BaseGame,
        "Scooby-Doo: Re-Masked",
    };
    return descriptor;
}

} // namespace RuntimeProduct
