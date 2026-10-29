
#include "window.h"

namespace Astral::Desktop::Windows {

bool contains(
    const Rect& rect,
    int32_t x,
    int32_t y
) {
    if (rect.width <= 0 || rect.height <= 0)
        return false;

    return x >= rect.x &&
           y >= rect.y &&
           static_cast<int64_t>(x) <
               static_cast<int64_t>(rect.x) +
               rect.width &&
           static_cast<int64_t>(y) <
               static_cast<int64_t>(rect.y) +
               rect.height;
}

}
