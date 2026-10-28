
#include "vfs.h"

namespace Astral::VFS {

struct Node {
    bool used;
    Type type;
    int parent;
    char name[MAX_NAME];
    char data[MAX_FILE_SIZE];
    uint32_t size;
};

static Node nodes[MAX_NODES]{};
static int cwd = 0;
static bool ready = false;

static uint32_t length(const char* s) {
    uint32_t n = 0;
    if (!s) return 0;
    while (s[n]) ++n;
    return n;
}

static bool same(const char* a, const char* b) {
    while (*a && *b) {
        if (*a++ != *b++) return false;
    }
    return *a == 0 && *b == 0;
}

static bool copy(
    char* dst,
    uint32_t capacity,
    const char* src
) {
    if (!dst || !src || !capacity)
        return false;

    uint32_t i = 0;
    while (src[i]) {
        if (i + 1 >= capacity)
            return false;
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
    return true;
}

static int child(int parent, const char* name) {
    for (uint32_t i = 1; i < MAX_NODES; ++i) {
        if (nodes[i].used &&
            nodes[i].parent == parent &&
            same(nodes[i].name, name))
            return static_cast<int>(i);
    }
    return -1;
}

static int allocate(
    int parent,
    const char* name,
    Type type
) {
    for (uint32_t i = 1; i < MAX_NODES; ++i) {
        if (nodes[i].used) continue;

        Node& n = nodes[i];
        n = {};
        n.used = true;
        n.type = type;
        n.parent = parent;
        copy(n.name, MAX_NAME, name);

        return static_cast<int>(i);
    }
    return -1;
}

// Resolve existing absolute or relative paths.
static Result resolve(const char* path, int& index) {
    if (!path || !*path)
        return Result::InvalidPath;

    int at = path[0] == '/' ? 0 : cwd;
    uint32_t pos = 0;

    while (path[pos]) {
        while (path[pos] == '/') ++pos;
        if (!path[pos]) break;

        char part[MAX_NAME]{};
        uint32_t count = 0;

        while (path[pos] && path[pos] != '/') {
            if (count + 1 >= MAX_NAME)
                return Result::InvalidPath;

            part[count++] = path[pos++];
        }
        part[count] = 0;

        if (same(part, ".")) continue;

        if (same(part, "..")) {
            if (at != 0)
                at = nodes[at].parent;
            continue;
        }

        if (nodes[at].type != Type::Directory)
            return Result::NotDirectory;

        int next = child(at, part);
        if (next < 0)
            return Result::NotFound;

        at = next;
    }

    index = at;
    return Result::OK;
}

// Find the parent directory and name for creation.
static Result parent_and_name(
    const char* path,
    int& parent,
    char* name
) {
    uint32_t n = length(path);
    if (n == 0 || n >= MAX_PATH)
        return Result::InvalidPath;

    // Ignore trailing slashes.
    while (n > 0 && path[n - 1] == '/')
        --n;

    if (n == 0)
        return Result::InvalidPath;

    uint32_t split = n;
    while (split > 0 && path[split - 1] != '/')
        --split;

    uint32_t nameSize = n - split;

    if (nameSize == 0 || nameSize >= MAX_NAME)
        return Result::InvalidPath;

    for (uint32_t i = 0; i < nameSize; ++i)
        name[i] = path[split + i];

    name[nameSize] = 0;

    if (same(name, ".") || same(name, ".."))
        return Result::InvalidPath;

    if (split == 0) {
        parent = cwd;
    } else {
        char prefix[MAX_PATH]{};

        for (uint32_t i = 0; i < split; ++i)
            prefix[i] = path[i];

        prefix[split] = 0;

        Result result = resolve(prefix, parent);
        if (result != Result::OK)
            return result;
    }

    if (nodes[parent].type != Type::Directory)
        return Result::NotDirectory;

    return Result::OK;
}

// Early prototype permission policy:
// /boot, /system and /root require HPT privileges.
static bool protected_node(int index) {
    while (index > 0) {
        if (nodes[index].parent == 0) {
            return same(nodes[index].name, "boot") ||
                   same(nodes[index].name, "system") ||
                   same(nodes[index].name, "root");
        }
        index = nodes[index].parent;
    }
    return false;
}

static bool allowed(int index, bool privileged) {
    return privileged || !protected_node(index);
}

static Result create(
    const char* path,
    Type type,
    bool privileged
) {
    int parent = -1;
    char name[MAX_NAME]{};

    Result result = parent_and_name(path, parent, name);
    if (result != Result::OK)
        return result;

    if (!allowed(parent, privileged))
        return Result::PermissionDenied;

    if (child(parent, name) >= 0)
        return Result::Exists;

    return allocate(parent, name, type) >= 0
        ? Result::OK : Result::NoSpace;
}

bool initialize() {
    ready = false;

    for (uint32_t i = 0; i < MAX_NODES; ++i)
        nodes[i] = {};

    nodes[0].used = true;
    nodes[0].type = Type::Directory;
    nodes[0].parent = 0;
    nodes[0].name[0] = '/';
    nodes[0].name[1] = 0;

    cwd = 0;

    // Initial filesystem layout.
    if (create("/boot", Type::Directory, true) != Result::OK ||
        create("/system", Type::Directory, true) != Result::OK ||
        create("/home", Type::Directory, true) != Result::OK ||
        create("/root", Type::Directory, true) != Result::OK ||
        create("/bin", Type::Directory, true) != Result::OK ||
        create("/tmp", Type::Directory, true) != Result::OK ||
        create("/var", Type::Directory, true) != Result::OK ||
        create("/var/logs", Type::Directory, true) != Result::OK ||
        create("/system/config", Type::Directory, true) != Result::OK ||
        create("/system/drivers", Type::Directory, true) != Result::OK ||
        create("/home/user", Type::Directory, true) != Result::OK) {
        return false;
    }

    ready = true;
    return true;
}

bool initialized() {
    return ready;
}

Result change_directory(
    const char* path,
    bool privileged
) {
    if (!ready) return Result::NoSpace;

    int index;
    Result result = resolve(path, index);

    if (result != Result::OK)
        return result;

    if (nodes[index].type != Type::Directory)
        return Result::NotDirectory;

    if (!allowed(index, privileged))
        return Result::PermissionDenied;

    cwd = index;
    return Result::OK;
}

Result make_directory(
    const char* path,
    bool privileged
) {
    if (!ready) return Result::NoSpace;
    return create(path, Type::Directory, privileged);
}

Result create_file(
    const char* path,
    bool privileged
) {
    if (!ready) return Result::NoSpace;

    int existing;
    Result result = resolve(path, existing);

    if (result == Result::OK) {
        return nodes[existing].type == Type::File
            ? Result::OK : Result::IsDirectory;
    }

    if (result != Result::NotFound)
        return result;

    return create(path, Type::File, privileged);
}

Result remove(
    const char* path,
    bool privileged
) {
    if (!ready) return Result::NoSpace;

    int index;
    Result result = resolve(path, index);

    if (result != Result::OK)
        return result;

    if (index == 0 || index == cwd)
        return Result::PermissionDenied;

    if (!allowed(index, privileged) ||
        !allowed(nodes[index].parent, privileged))
        return Result::PermissionDenied;

    if (nodes[index].type == Type::Directory) {
        for (uint32_t i = 1; i < MAX_NODES; ++i) {
            if (nodes[i].used &&
                nodes[i].parent == index)
                return Result::NotEmpty;
        }
    }

    // Also protect the current working directory
    // from deletion through one of its ancestors.
    int at = cwd;
    while (at > 0) {
        if (at == index)
            return Result::PermissionDenied;
        at = nodes[at].parent;
    }

    nodes[index] = {};
    return Result::OK;
}

Result write_file(
    const char* path,
    const char* content,
    bool privileged
) {
    if (!ready || !content)
        return Result::InvalidPath;

    uint32_t size = length(content);
    if (size >= MAX_FILE_SIZE)
        return Result::NoSpace;

    int index;
    Result result = resolve(path, index);

    if (result == Result::NotFound) {
        result = create_file(path, privileged);
        if (result != Result::OK)
            return result;
        result = resolve(path, index);
    }

    if (result != Result::OK)
        return result;

    if (nodes[index].type != Type::File)
        return Result::IsDirectory;

    if (!allowed(index, privileged))
        return Result::PermissionDenied;

    Node& file = nodes[index];

    for (uint32_t i = 0; i < size; ++i)
        file.data[i] = content[i];

    file.data[size] = 0;
    file.size = size;

    return Result::OK;
}

Result read_file(
    const char* path,
    char* output,
    uint32_t capacity,
    bool privileged
) {
    if (!ready || !output || capacity == 0)
        return Result::InvalidPath;

    int index;
    Result result = resolve(path, index);
    if (result != Result::OK)
        return result;

    if (nodes[index].type != Type::File)
        return Result::IsDirectory;

    if (!allowed(index, privileged))
        return Result::PermissionDenied;

    if (nodes[index].size + 1 > capacity)
        return Result::NoSpace;

    return copy(output, capacity, nodes[index].data)
        ? Result::OK : Result::NoSpace;
}

Result list(
    const char* path,
    Entry* output,
    uint32_t capacity,
    uint32_t& count,
    bool privileged
) {
    count = 0;

    if (!ready)
        return Result::NoSpace;

    int index;
    Result result = resolve(path, index);
    if (result != Result::OK)
        return result;

    if (!allowed(index, privileged))
        return Result::PermissionDenied;

    if (nodes[index].type == Type::File) {
        if (!output || capacity == 0)
            return Result::NoSpace;

        copy(output[0].name, MAX_NAME, nodes[index].name);
        output[0].type = Type::File;
        output[0].size = nodes[index].size;
        count = 1;
        return Result::OK;
    }

    for (uint32_t i = 1; i < MAX_NODES; ++i) {
        if (!nodes[i].used ||
            nodes[i].parent != index)
            continue;

        if (!output || count >= capacity)
            return Result::NoSpace;

        Entry& entry = output[count++];
        copy(entry.name, MAX_NAME, nodes[i].name);
        entry.type = nodes[i].type;
        entry.size = nodes[i].size;
    }

    return Result::OK;
}

void working_directory(
    char* output,
    uint32_t capacity
) {
    if (!output || capacity == 0)
        return;

    output[0] = 0;

    if (cwd == 0) {
        copy(output, capacity, "/");
        return;
    }

    int chain[MAX_NODES];
    uint32_t count = 0;

    int at = cwd;
    while (at > 0 && count < MAX_NODES) {
        chain[count++] = at;
        at = nodes[at].parent;
    }

    uint32_t pos = 0;

    while (count > 0) {
        const char* name = nodes[chain[--count]].name;

        if (pos + 1 >= capacity)
            break;

        output[pos++] = '/';

        while (*name && pos + 1 < capacity)
            output[pos++] = *name++;
    }

    output[pos] = 0;
}

const char* error_string(Result result) {
    switch (result) {
        case Result::OK: return "Success";
        case Result::NotFound: return "Not found";
        case Result::Exists: return "Already exists";
        case Result::NotDirectory: return "Not a directory";
        case Result::IsDirectory: return "Is a directory";
        case Result::InvalidPath: return "Invalid path";
        case Result::NoSpace: return "No space or buffer too small";
        case Result::NotEmpty: return "Directory not empty";
        case Result::PermissionDenied: return "Permission denied";
    }

    return "Unknown filesystem error";
}

}
