#include "pch.h"
#include "DlssNr_Models.h"

#include <Util.h>

#include <format>
#include <mutex>

namespace DlssNr::Models
{
namespace
{
// The menu thread lists and resolves while the render thread resolves, so everything below is behind
// this one lock.
std::mutex g_mutex;

bool g_scanned = false;
std::vector<Candidate> g_list;

// Resolve's answer for the id it was last asked about. The pass asks every frame, and for the default
// that is a recursive search of the game folder.
bool g_resolved = false;
std::string g_resolvedId;
std::optional<std::filesystem::path> g_resolvedPath;
bool g_resolvedFellBack = false;

// Beside OptiScaler first, then beside the game -- the order the pass has always looked in. Once,
// when they are the same folder, which is the usual install.
std::vector<std::filesystem::path> Roots()
{
    std::vector<std::filesystem::path> roots { Util::DllPath().remove_filename() };
    const std::filesystem::path exeDir = Util::ExePath().remove_filename();

    std::error_code ec;

    if (!std::filesystem::equivalent(roots[0], exeDir, ec))
        roots.push_back(exeDir);

    return roots;
}

// Exactly the lookup the pass made before a model could be chosen, so the default is what it was.
std::optional<std::filesystem::path> FindDefault()
{
    auto found = Util::FindFilePath(Util::DllPath().remove_filename(), "nvngx_dlssnr.dll");

    if (!found.has_value())
        found = Util::FindFilePath(Util::ExePath().remove_filename(), "nvngx_dlssnr.dll");

    return found;
}

// nvngx_dlssnr.dll, or a copy renamed around that stem. The forwarder is nvngx.dll_dlssnr.dll and
// does not match, which matters: it is the one DLL in the folder that must never be loaded as a model.
bool IsModelFile(const std::filesystem::directory_entry& entry)
{
    std::error_code ec;

    if (!entry.is_regular_file(ec))
        return false;

    const std::wstring name = Util::ToLower(entry.path().filename().wstring());
    return name.starts_with(L"nvngx_dlssnr") && name.ends_with(L".dll");
}

std::optional<std::filesystem::path> ResolveLocked(const std::string& id, bool* fellBack)
{
    if (fellBack != nullptr)
        *fellBack = false;

    if (!id.empty())
    {
        const std::filesystem::path named(string_to_wstring(id));
        std::error_code ec;

        if (named.is_absolute())
        {
            if (std::filesystem::is_regular_file(named, ec))
                return named;
        }
        else
        {
            for (const auto& root : Roots())
            {
                const auto candidate = root / named;

                if (std::filesystem::is_regular_file(candidate, ec))
                    return candidate;
            }
        }

        if (fellBack != nullptr)
            *fellBack = true;
    }

    return FindDefault();
}

bool AlreadyListed(const std::filesystem::path& path)
{
    std::error_code ec;

    for (const auto& c : g_list)
    {
        if (std::filesystem::equivalent(c.path, path, ec))
            return true;
    }

    return false;
}

// Adds one file found under root at the relative path rel.
//
// The id is relative so a game folder can be moved or shared without the choice breaking. It is made
// absolute only when the relative form would resolve to a different file -- a copy beside the game
// shadowed by one beside OptiScaler under the same name -- because an id that resolves elsewhere is a
// choice the menu could offer and never honour.
void Add(const std::filesystem::path& root, const std::filesystem::path& rel)
{
    const auto path = root / rel;

    if (AlreadyListed(path))
        return;

    Candidate c;
    c.id = wstring_to_string(rel.wstring());
    c.path = path;
    c.version = VersionOf(path);

    std::error_code ec;
    const auto resolves = ResolveLocked(c.id, nullptr);

    if (!resolves.has_value() || !std::filesystem::equivalent(*resolves, path, ec))
        c.id = wstring_to_string(path.wstring());

    g_list.push_back(std::move(c));
}

void ScanLocked()
{
    g_list.clear();
    g_scanned = true;
    g_resolved = false;

    if (const auto def = FindDefault(); def.has_value())
        g_list.push_back(Candidate { std::string(), *def, VersionOf(*def) });

    for (const auto& root : Roots())
    {
        std::error_code ec;

        // Beside it. Not recursive: a game folder can be large, and the menu rescans on request.
        for (const auto& entry :
             std::filesystem::directory_iterator(root, std::filesystem::directory_options::skip_permission_denied, ec))
        {
            if (IsModelFile(entry))
                Add(root, entry.path().filename());
        }

        const auto models = root / L"DlssNrModels";

        if (!std::filesystem::is_directory(models, ec))
            continue;

        for (const auto& entry : std::filesystem::directory_iterator(
                 models, std::filesystem::directory_options::skip_permission_denied, ec))
        {
            if (IsModelFile(entry))
            {
                Add(root, std::filesystem::path(L"DlssNrModels") / entry.path().filename());
                continue;
            }

            std::error_code dirEc;

            if (!entry.is_directory(dirEc))
                continue;

            // One level down, so each model can keep the name NVIDIA gave it.
            for (const auto& inner : std::filesystem::directory_iterator(
                     entry.path(), std::filesystem::directory_options::skip_permission_denied, dirEc))
            {
                if (IsModelFile(inner))
                    Add(root,
                        std::filesystem::path(L"DlssNrModels") / entry.path().filename() / inner.path().filename());
            }
        }
    }

    for (const auto& c : g_list)
        LOG_INFO("DLSS-NR model available: {} ({}){}", wstring_to_string(c.path.wstring()),
                 c.version.empty() ? "no version" : c.version, c.id.empty() ? " -- the default" : "");
}
} // namespace

std::vector<Candidate> List(bool rescan)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (rescan || !g_scanned)
        ScanLocked();

    return g_list;
}

std::optional<std::filesystem::path> Resolve(const std::string& id, bool* fellBack)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_resolved || g_resolvedId != id)
    {
        g_resolvedPath = ResolveLocked(id, &g_resolvedFellBack);
        g_resolvedId = id;
        g_resolved = true;

        if (g_resolvedFellBack)
            LOG_WARN("DLSS-NR model {} was not found; using the default", id);
    }

    if (fellBack != nullptr)
        *fellBack = g_resolvedFellBack;

    return g_resolvedPath;
}

std::string VersionOf(const std::filesystem::path& file)
{
    version_t v;

    if (!Util::GetFileVersion(file.wstring(), &v))
        return std::string();

    return std::format("{}.{}.{}.{}", v.major, v.minor, v.patch, v.reserved);
}
} // namespace DlssNr::Models
