
#pragma once

#include <stdint.h>
#include "../../windows/window.h"
#include "../../graphics/image.h"

namespace Astral::Desktop::Apps::AboutApp {

struct Information {
    const char* osName;
    const char* version;
    const char* architecture;
    const char* buildDate;
    const char* kernelName;
    const char* description;
};

void initialize();

void set_information(const Information& information);
void set_logo(const Image::Bitmap* logo);

void render(const Windows::Rect& bounds);

const Information& information();

}
