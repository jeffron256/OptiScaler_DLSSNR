#pragma once

// Which nvngx_dlssnr.dll the model is loaded from.
//
// NVIDIA ships the model as one DLL, and a newer build of that DLL is a different model. 310.9.16 --
// first seen in an NBA 2K27 update -- is reported as roughly twice as fast as the 310.8 builds, and it
// visibly does less. Both are worth keeping, so either can be chosen in the menu rather than by
// swapping files in the game folder between sessions.
//
// Looked for beside OptiScaler and beside the game:
//
//   nvngx_dlssnr.dll                           the default, found exactly as it was before any of this
//   nvngx_dlssnr*.dll                          a renamed copy, e.g. nvngx_dlssnr_310.9.16.dll
//   DlssNrModels\nvngx_dlssnr*.dll             the same, kept out of the game folder's root
//   DlssNrModels\<any name>\nvngx_dlssnr*.dll  a copy under its original name -- the safer layout, since
//                                              nothing about the model's own file changes
//
// [DlssNr] Model stores the choice as a path relative to the folder it was found in. Empty is the
// default, and resolves through the same lookup the pass always used, so an ini that never names a
// model behaves byte for byte as it did.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace DlssNr::Models
{
struct Candidate
{
    // What [DlssNr] Model stores to choose this one. Empty for the default.
    std::string id;
    std::filesystem::path path;
    // The file's own version, e.g. "310.9.16.0". Empty when the file carries none.
    std::string version;
};

// Every model this install can see, the default first.
//
// Cached, because the menu asks every frame and the default is found by a recursive search of the game
// folder. rescan looks again.
std::vector<Candidate> List(bool rescan = false);

// The file a stored choice names.
//
// An id that no longer exists resolves to the default rather than to nothing: a moved file should not
// cost the session. fellBack says when that happened, so the menu can say so instead of letting the
// default pass for the choice. Cached per id for the same reason as List; rescan drops the cache.
std::optional<std::filesystem::path> Resolve(const std::string& id, bool* fellBack = nullptr);

// The file's version as text, or empty.
std::string VersionOf(const std::filesystem::path& file);

// Which model a backend is running, for the menu. Each backend reports its own.
struct Status
{
    // The file the live feature was built from, and its version. Empty when nothing is running.
    std::filesystem::path running;
    std::string version;

    // The chosen model is not the one loaded, and this forwarder keeps its first model for the
    // session -- an older nvngx.dll_dlssnr.dll. The choice takes effect after a restart.
    bool needsRestart = false;
};
} // namespace DlssNr::Models
