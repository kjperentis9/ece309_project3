// src/tools/workspace.cpp — YOU IMPLEMENT (spec §4.4).
//
// Workspace::resolve: a purely lexical check, steps 1–6 of the spec.
// Do not touch the filesystem here (no exists(), canonical(), ...).
// The constructor, root() and check_symlinks() are provided in
// workspace_provided.cpp.

#include "tools/workspace.h"

namespace fs = std::filesystem;

// Keep the supplied path inside the workspace.
bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    reason->clear();

    if (user_path.empty()) {
        *reason = "empty path";
        return false;
    }

    if (user_path.find('\0') != std::string_view::npos) {
        *reason = "Path contains a null character";
        return false;
    }

    try {
        fs::path path{std::string(user_path)};

        if (path.is_absolute() || path.has_root_path()) {
            *reason = "Path must be relative to the workspace";
            return false;
        }

        // Remove extra separators and simplify . and .. components.
        fs::path normal_path = path.lexically_normal();

        if (!normal_path.empty() && *normal_path.begin() == "..") {
            *reason = "Path goes outside the workspace";
            return false;
        }

        if (normal_path == ".") {
            *out = root();
        } else {
            *out = root() / normal_path;
        }

        return true;
    } catch (...) {
        *reason = "Could not process the path";
        return false;
    }
}
