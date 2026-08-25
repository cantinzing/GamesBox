#include "pch.h"
#include "EpicSourceAdapter.h"

#include <windows.h>
#include <shellapi.h>

#include <fstream>
#include <sstream>

namespace Sources
{
    namespace
    {
        std::wstring GetEnvironmentPath(std::wstring const& variable)
        {
            DWORD size = GetEnvironmentVariableW(variable.c_str(), nullptr, 0);
            if (size == 0)
            {
                return {};
            }
            std::wstring result(size, L'\0');
            GetEnvironmentVariableW(variable.c_str(), &result[0], size);
            result.pop_back();
            return result;
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

        std::wstring ExtractJsonValue(std::wstring const& text, std::wstring const& key)
        {
            auto marker = text.find(key);
            if (marker == std::wstring::npos)
            {
                return {};
            }
            marker += key.size();
            while (marker < text.size() && text[marker] != L':')
            {
                ++marker;
            }
            if (marker >= text.size())
            {
                return {};
            }
            ++marker;
            while (marker < text.size() && (text[marker] == L' ' || text[marker] == L'\t' || text[marker] == L'\r' || text[marker] == L'\n'))
            {
                ++marker;
            }
            if (marker >= text.size() || text[marker] != L'"')
            {
                return {};
            }
            ++marker;
            std::wstring result;
            while (marker < text.size() && text[marker] != L'"')
            {
                if (text[marker] == L'\\' && marker + 1 < text.size())
                {
                    ++marker;
                }
                result.push_back(text[marker]);
                ++marker;
            }
            return result;
        }
    }

    EpicSourceAdapter::EpicSourceAdapter()
    {
        auto programData = GetEnvironmentPath(L"PROGRAMDATA");
        if (!programData.empty())
        {
            m_manifestsPath = programData + L"\\Epic\\EpicGamesLauncher\\Data\\Manifests";
        }
    }

    Core::GameSourceType EpicSourceAdapter::SourceType() const
    {
        return Core::GameSourceType::Epic;
    }

    std::wstring EpicSourceAdapter::DisplayName() const
    {
        return L"Epic Games";
    }

    bool EpicSourceAdapter::IsInstalled() const
    {
        DWORD attributes = GetFileAttributesW(m_manifestsPath.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES
            && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    bool EpicSourceAdapter::IsEnabled() const
    {
        return m_enabled;
    }

    void EpicSourceAdapter::SetEnabled(bool enabled)
    {
        m_enabled = enabled;
    }

    std::vector<Core::Installation> EpicSourceAdapter::DiscoverInstallations()
    {
        std::vector<Core::Installation> result;
        if (!m_enabled || m_manifestsPath.empty())
        {
            return result;
        }
        WIN32_FIND_DATAW findData{};
        HANDLE find = FindFirstFileW((m_manifestsPath + L"\\*.item").c_str(), &findData);
        if (find == INVALID_HANDLE_VALUE)
        {
            return result;
        }
        do
        {
            auto filePath = m_manifestsPath + L"\\" + findData.cFileName;
            auto text = ReadUtf8File(filePath);
            auto displayName = ExtractJsonValue(text, L"DisplayName");
            auto appName = ExtractJsonValue(text, L"AppName");
            auto installLocation = ExtractJsonValue(text, L"InstallLocation");
            auto launchCommand = ExtractJsonValue(text, L"LaunchCommand");
            auto workingDir = ExtractJsonValue(text, L"WorkingDir");
            if (displayName.empty() || appName.empty())
            {
                continue;
            }
            Core::Installation installation;
            installation.Source = Core::GameSourceType::Epic;
            installation.SourceKey = appName;
            installation.Name = displayName;
            installation.InstallPath = installLocation;
            installation.LaunchTarget = launchCommand;
            installation.WorkingDirectory = workingDir;
            result.push_back(std::move(installation));
        } while (FindNextFileW(find, &findData) != 0);
        FindClose(find);
        return result;
    }

    bool EpicSourceAdapter::LaunchInstallation(Core::Installation const& installation)
    {
        auto url = installation.LaunchTarget.empty()
            ? L"com.epicgames.launcher://apps/" + installation.SourceKey + L"?action=launch&silent=true"
            : installation.LaunchTarget;
        HINSTANCE result = ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return reinterpret_cast<INT_PTR>(result) > 32;
    }
}
