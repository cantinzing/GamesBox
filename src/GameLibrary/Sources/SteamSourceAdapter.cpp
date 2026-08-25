#include "pch.h"
#include "SteamSourceAdapter.h"

#include "../Core/Normalization.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Sources
{
    namespace
    {
        std::wstring ReadRegistryString(HKEY root, std::wstring const& subKey, std::wstring const& valueName)
        {
            HKEY key = nullptr;
            if (RegOpenKeyExW(root, subKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
            {
                return {};
            }
            DWORD size = 0;
            RegQueryValueExW(key, valueName.c_str(), nullptr, nullptr, nullptr, &size);
            std::wstring result;
            if (size > 0)
            {
                result.resize(size / sizeof(wchar_t));
                RegQueryValueExW(key, valueName.c_str(), nullptr, nullptr,
                    reinterpret_cast<LPBYTE>(&result[0]), &size);
                auto terminator = result.find(L'\0');
                if (terminator != std::wstring::npos)
                {
                    result.resize(terminator);
                }
            }
            RegCloseKey(key);
            return result;
        }

        std::wstring Trim(std::wstring const& value)
        {
            size_t first = value.find_first_not_of(L" \t\r\n");
            if (first == std::wstring::npos)
            {
                return {};
            }
            size_t last = value.find_last_not_of(L" \t\r\n");
            return value.substr(first, last - first + 1);
        }

        std::wstring ReadUtf8File(std::wstring const& path)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                return {};
            }
            std::ostringstream buffer;
            buffer << stream.rdbuf();
            std::string bytes = buffer.str();
            if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF
                && static_cast<unsigned char>(bytes[1]) == 0xBB
                && static_cast<unsigned char>(bytes[2]) == 0xBF)
            {
                bytes.erase(0, 3);
            }
            int size = MultiByteToWideChar(CP_UTF8, 0, bytes.c_str(),
                static_cast<int>(bytes.size()), nullptr, 0);
            if (size <= 0)
            {
                return {};
            }
            std::wstring result(size, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, bytes.c_str(), static_cast<int>(bytes.size()),
                &result[0], size);
            return result;
        }

        std::wstring ExtractQuotedValue(std::wstring const& text, std::wstring const& quotedKey,
            size_t from = 0)
        {
            auto marker = text.find(quotedKey, from);
            if (marker == std::wstring::npos)
            {
                return {};
            }
            marker += quotedKey.size();
            while (marker < text.size()
                && (text[marker] == L' ' || text[marker] == L'\t'
                    || text[marker] == L'\r' || text[marker] == L'\n'))
            {
                ++marker;
            }
            if (marker < text.size() && text[marker] == L'"')
            {
                auto end = text.find(L'"', marker + 1);
                if (end != std::wstring::npos)
                {
                    return text.substr(marker + 1, end - marker - 1);
                }
            }
            return {};
        }
    }

    SteamSourceAdapter::SteamSourceAdapter()
    {
        m_steamPath = ReadRegistryString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
        if (m_steamPath.empty())
        {
            m_steamPath = ReadRegistryString(HKEY_LOCAL_MACHINE, L"Software\\Valve\\Steam", L"SteamPath");
        }
    }

    Core::GameSourceType SteamSourceAdapter::SourceType() const
    {
        return Core::GameSourceType::Steam;
    }

    std::wstring SteamSourceAdapter::DisplayName() const
    {
        return L"Steam";
    }

    bool SteamSourceAdapter::IsInstalled() const
    {
        return !m_steamPath.empty();
    }

    bool SteamSourceAdapter::IsEnabled() const
    {
        return m_enabled;
    }

    void SteamSourceAdapter::SetEnabled(bool enabled)
    {
        m_enabled = enabled;
    }

    std::vector<std::wstring> SteamSourceAdapter::FindLibraryFolders(std::wstring const& steamPath) const
    {
        std::vector<std::wstring> folders;
        auto manifestPath = steamPath + L"\\steamapps\\libraryfolders.vdf";
        auto text = ReadUtf8File(manifestPath);
        if (text.empty())
        {
            return folders;
        }
        size_t pos = 0;
        while ((pos = text.find(L"\"path\"", pos)) != std::wstring::npos)
        {
            auto path = ExtractQuotedValue(text, L"\"path\"", pos);
            if (!path.empty())
            {
                folders.push_back(path);
            }
            pos += 6;
        }
        return folders;
    }

    std::vector<Core::Installation> SteamSourceAdapter::ScanLibrary(std::wstring const& libraryPath) const
    {
        std::vector<Core::Installation> result;
        WIN32_FIND_DATAW findData{};
        auto pattern = libraryPath + L"\\steamapps\\appmanifest_*.acf";
        HANDLE find = FindFirstFileW(pattern.c_str(), &findData);
        if (find == INVALID_HANDLE_VALUE)
        {
            return result;
        }
        do
        {
            std::wstring fileName = findData.cFileName;
            if (fileName.size() < 16 || fileName.compare(0, 12, L"appmanifest_") != 0)
            {
                continue;
            }
            auto appId = fileName.substr(12, fileName.size() - 12 - 4);
            auto acfPath = libraryPath + L"\\steamapps\\" + fileName;
            auto text = ReadUtf8File(acfPath);
            auto name = ExtractQuotedValue(text, L"\"name\"");
            auto installDir = ExtractQuotedValue(text, L"\"installdir\"");
            if (name.empty() || appId.empty())
            {
                continue;
            }
            Core::Installation installation;
            installation.Source = Core::GameSourceType::Steam;
            installation.SourceKey = appId;
            installation.Name = name;
            installation.InstallPath = libraryPath + L"\\steamapps\\common\\" + installDir;
            installation.WorkingDirectory = installation.InstallPath;
            installation.LaunchTarget = L"steam://run/" + appId;
            result.push_back(std::move(installation));
        } while (FindNextFileW(find, &findData) != 0);
        FindClose(find);
        return result;
    }

    std::vector<Core::Installation> SteamSourceAdapter::DiscoverInstallations()
    {
        std::vector<Core::Installation> result;
        if (!m_enabled || m_steamPath.empty())
        {
            return result;
        }
        auto folders = FindLibraryFolders(m_steamPath);
        if (folders.empty())
        {
            folders.push_back(m_steamPath);
        }
        for (auto const& folder : folders)
        {
            auto scanned = ScanLibrary(folder);
            result.insert(result.end(), scanned.begin(), scanned.end());
        }
        return result;
    }

    bool SteamSourceAdapter::LaunchInstallation(Core::Installation const& installation)
    {
        auto url = L"steam://run/" + installation.SourceKey;
        HINSTANCE result = ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return reinterpret_cast<INT_PTR>(result) > 32;
    }
}
