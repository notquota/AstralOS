
#include "events.h"

namespace Astral::Desktop::Events {

namespace {

constexpr uint32_t CAPACITY = 256;

Event queue[CAPACITY]{};

uint32_t head = 0;
uint32_t tail = 0;
uint32_t size = 0;
uint32_t droppedCount = 0;

}

void initialize() {
    head = 0;
    tail = 0;
    size = 0;
    droppedCount = 0;
}

bool push(const Event& event) {
    if (size == CAPACITY) {
        ++droppedCount;
        return false;
    }

    queue[tail] = event;
    tail = (tail + 1) % CAPACITY;
    ++size;

    return true;
}

bool pop(Event& event) {
    if (size == 0)
        return false;

    event = queue[head];
    head = (head + 1) % CAPACITY;
    --size;

    return true;
}

uint32_t pending() {
    return size;
}

uint32_t dropped() {
    return droppedCount;
}

}
