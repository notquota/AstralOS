
#pragma once

#include <stdint.h>

namespace Astral::VFS {

constexpr uint32_t MAX_NODES = 128;
constexpr uint32_t MAX_NAME = 32;
constexpr uint32_t MAX_FILE_SIZE = 512;
constexpr uint32_t MAX_PATH = 256;

enum class Type : uint8_t {
    File,
    Directory
};

enum class Result : uint8_t {
    OK,
    NotFound,
    Exists,
    NotDirectory,
    IsDirectory,
    InvalidPath,
    NoSpace,
    NotEmpty,
    PermissionDenied
};

struct Entry {
    char name[MAX_NAME];
    Type type;
    uint32_t size;
};

bool initialize();
bool initialized();

Result change_directory(
    const char* path,
    bool privileged
);

Result make_directory(
    const char* path,
    bool privileged
);

Result create_file(
    const char* path,
    bool privileged
);

Result remove(
    const char* path,
    bool privileged
);

Result write_file(
    const char* path,
    const char* content,
    bool privileged
);

Result read_file(
    const char* path,
    char* output,
    uint32_t capacity,
    bool privileged
);

Result list(
    const char* path,
    Entry* output,
    uint32_t capacity,
    uint32_t& count,
    bool privileged
);

void working_directory(
    char* output,
    uint32_t capacity
);

const char* error_string(Result result);

}
