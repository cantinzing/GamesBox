#include "pch.h"
#include "LocalSourceAdapter.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <unordered_map>
#include <cstdio>
#include <vector>

#pragma comment(lib, "version.lib")

namespace Sources
{
    namespace
    {
        std::wstring Lower(std::wstring value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) {
                if (c >= L'A' && c <= L'Z')
                {
                    return static_cast<wchar_t>(c - L'A' + L'a');
                }
                return c;
            });
            return value;
        }

        std::wstring FileNameWithoutExtension(std::wstring const& fileName)
        {
            auto dot = fileName.find_last_of(L'.');
            if (dot != std::wstring::npos)
            {
                return fileName.substr(0, dot);
            }
            return fileName;
        }

        // 保守过滤：明显不属于"游戏本体"的可执行文件（卸载/安装/运行库/更新器/崩溃上报等），
        // 以及常见辅助子目录（Redist/CrashReport 等）内的文件。
        bool IsLikelyNotGame(std::wstring const& lowerName, std::wstring const& lowerDirectory)
        {
            auto lowerBase = FileNameWithoutExtension(lowerName);
            static const std::wstring kNamePrefixes[] = {
                L"unins", L"uninstall", L"setup", L"install", L"vcredist", L"vc_redist",
                L"dxsetup", L"dxwebsetup", L"redist", L"crashpad", L"crashhandler",
                L"crashreport", L"unitycrashhandler", L"vcruntime", L"updater",
                L"dotnet", L"autoupdate", L"bootstrap", L"patch", L"repair",
                L"webhelper", L"windowsstore", L"gamelaunch", L"gamelauncher"
            };
            for (auto const& prefix : kNamePrefixes)
            {
                if (lowerBase.compare(0, prefix.size(), prefix) == 0)
                {
                    return true;
                }
            }
            // 精确匹配：UE 引擎工具 / 图形调试器 / 其他已知辅助程序
            static const std::wstring kAuxiliaryNames[] = {
                L"unreallightmass", L"shadercompileworker", L"crashreportclient",
                L"unrealcefsubprocess", L"livecodingconsole", L"interchangeworker",
                L"epicwebhelper", L"unrealpak", L"unrealeditor", L"unrealeditor-cmd",
                L"ue4editor", L"ue4editor-cmd", L"ue5editor", L"ue5editor-cmd",
                L"renderdoc", L"renderdoccmd", L"apitrace", L"unrealbuildtool",
                L"unitycrashhandler64", L"unitycrashhandler32", L"cefsubprocess",
                L"slimecrashhandler", L"webviewhost"
            };
            for (auto const& aux : kAuxiliaryNames)
            {
                if (lowerBase == aux)
                {
                    return true;
                }
            }
            static const std::wstring kNameTokens[] = {
                L"subprocess", L"shadercompile", L"lightmass"
            };
            for (auto const& token : kNameTokens)
            {
                if (lowerBase.find(token) != std::wstring::npos)
                {
                    return true;
                }
            }
            static const std::wstring kDirTokens[] = {
                L"redist", L"crashreport", L"crashreporter", L"unitycrashhandler",
                L"redistributable", L"support", L"commonredist", L"_commonredist",
                L"engine", L"binaries", L"content", L"plugins"
            };
            for (auto const& token : kDirTokens)
            {
                if (lowerDirectory.find(token) != std::wstring::npos)
                {
                    return true;
                }
            }
            return false;
        }

        // 读取 PE 版本资源的指定字段（CompanyName / ProductName / FileDescription 等）
        std::wstring VersionInfoField(std::wstring const& path, wchar_t const* field)
        {
            DWORD handle = 0;
            DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
            if (size == 0)
            {
                return {};
            }
            std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1, L'\0');
            if (!GetFileVersionInfoW(path.c_str(), 0, size, buffer.data()))
            {
                return {};
            }
            struct LangAndCodePage
            {
                WORD Language;
                WORD CodePage;
            }* translation = nullptr;
            UINT translationBytes = 0;
            if (!VerQueryValueW(buffer.data(), L"\\VarFileInfo\\Translation",
                    reinterpret_cast<void**>(&translation), &translationBytes))
            {
                return {};
            }
            if (translationBytes < sizeof(LangAndCodePage))
            {
                return {};
            }
            wchar_t subBlock[128];
            swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\%s",
                translation[0].Language, translation[0].CodePage, field);
            wchar_t* value = nullptr;
            UINT valueBytes = 0;
            if (!VerQueryValueW(buffer.data(), subBlock, reinterpret_cast<void**>(&value), &valueBytes))
            {
                return {};
            }
            return value;
        }

        // 版本信息启发式：描述/产品/公司含安装器、更新器、崩溃上报、运行库等特征 → 辅助程序
        bool VersionInfoLooksAuxiliary(std::wstring const& path)
        {
            std::wstring combined = Lower(VersionInfoField(path, L"CompanyName"))
                + L" " + Lower(VersionInfoField(path, L"ProductName"))
                + L" " + Lower(VersionInfoField(path, L"FileDescription"));
            static const std::wstring kInfoTokens[] = {
                L"install", L"uninstall", L"updat", L"redistribut", L"crash", L"runtime",
                L"bootstrapper", L"repair", L"visual c++", L"directx", L".net framework"
            };
            for (auto const& token : kInfoTokens)
            {
                if (combined.find(token) != std::wstring::npos)
                {
                    return true;
                }
            }
            // Epic Games 出品但归属 UE 引擎的工具链（游戏本体描述不会自称 "Unreal Engine"）
            if (combined.find(L"epic games") != std::wstring::npos
                && combined.find(L"unreal") != std::wstring::npos)
            {
                return true;
            }
            // Unity Technologies 的崩溃上报等辅助进程
            if (combined.find(L"unity technologies") != std::wstring::npos
                && combined.find(L"crash") != std::wstring::npos)
            {
                return true;
            }
            return false;
        }

        // 目录名（最后一段，小写）
        std::wstring DirectoryName(std::wstring const& path)
        {
            auto slash = path.find_last_of(L'\\');
            if (slash == std::wstring::npos)
            {
                return Lower(path);
            }
            return Lower(path.substr(slash + 1));
        }

        // 是否为 Unity 游戏目录（存在 * _Data 子目录）
        bool HasUnityDataDir(std::wstring const& directory)
        {
            WIN32_FIND_DATAW findData{};
            HANDLE find = FindFirstFileW((directory + L"\\*_Data").c_str(), &findData);
            if (find == INVALID_HANDLE_VALUE)
            {
                return false;
            }
            FindClose(find);
            return true;
        }

        // 主程序评分：文件大小 + 目录深度 + 与目录同名 + 引擎关联 + exe 优先于 lnk
        int ScoreExecutable(Core::Installation const& candidate, std::wstring const& directory,
            std::wstring const& rootDirectory, bool unityDir)
        {
            int score = 0;
            WIN32_FILE_ATTRIBUTE_DATA info{};
            if (GetFileAttributesExW(candidate.InstallPath.c_str(), GetFileExInfoStandard, &info))
            {
                ULONGLONG size = (static_cast<ULONGLONG>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
                if (size >= 4 * 1024 * 1024) score += 60;
                else if (size >= 1024 * 1024) score += 40;
                else if (size >= 256 * 1024) score += 20;
                else score += 5;
            }
            if (DirectoryName(candidate.WorkingDirectory) == DirectoryName(rootDirectory))
            {
                score += 15;
            }
            else if (candidate.WorkingDirectory == rootDirectory)
            {
                score += 8;
            }
            if (DirectoryName(candidate.WorkingDirectory) == Lower(FileNameWithoutExtension(candidate.Name)))
            {
                score += 20;
            }
            if (unityDir && candidate.WorkingDirectory == rootDirectory)
            {
                score += 10;
            }
            std::wstring lowerTarget = Lower(candidate.LaunchTarget);
            if (lowerTarget.size() >= 4
                && lowerTarget.compare(lowerTarget.size() - 4, 4, L".exe") == 0)
            {
                score += 5;
            }
            return score;
        }

        // 每个目录只保留评分最高的主程序
        std::vector<Core::Installation> SelectPrimaryExecutables(
            std::vector<Core::Installation> const& candidates, std::wstring const& rootDirectory)
        {
            std::unordered_map<std::wstring, std::vector<size_t>> byDirectory;
            for (size_t i = 0; i < candidates.size(); ++i)
            {
                byDirectory[Lower(candidates[i].WorkingDirectory)].push_back(i);
            }
            std::vector<Core::Installation> result;
            result.reserve(byDirectory.size());
            for (auto const& entry : byDirectory)
            {
                if (entry.second.size() == 1)
                {
                    result.push_back(candidates[entry.second[0]]);
                    continue;
                }
                bool unityDir = HasUnityDataDir(entry.first);
                size_t best = entry.second[0];
                int bestScore = -1;
                for (auto index : entry.second)
                {
                    int score = ScoreExecutable(candidates[index], entry.first, rootDirectory, unityDir);
                    if (score > bestScore)
                    {
                        bestScore = score;
                        best = index;
                    }
                }
                result.push_back(candidates[best]);
            }
            return result;
        }
    }

    LocalSourceAdapter::LocalSourceAdapter(std::wstring rootDirectory)
        : m_rootDirectory(std::move(rootDirectory))
    {
        m_extensions.push_back(L".exe");
        m_extensions.push_back(L".lnk");
    }

    Core::GameSourceType LocalSourceAdapter::SourceType() const
    {
        return Core::GameSourceType::Local;
    }

    std::wstring LocalSourceAdapter::DisplayName() const
    {
        return L"本地文件";
    }

    bool LocalSourceAdapter::IsInstalled() const
    {
        DWORD attributes = GetFileAttributesW(m_rootDirectory.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES
            && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    bool LocalSourceAdapter::IsEnabled() const
    {
        return m_enabled;
    }

    void LocalSourceAdapter::SetEnabled(bool enabled)
    {
        m_enabled = enabled;
    }

    void LocalSourceAdapter::SetScanExtensions(std::vector<std::wstring> extensions)
    {
        m_extensions = std::move(extensions);
    }

    void LocalSourceAdapter::AddExcludedDirectory(std::wstring directory)
    {
        m_excludedDirectories.push_back(std::move(directory));
    }

    void LocalSourceAdapter::SetRootDirectory(std::wstring directory)
    {
        m_rootDirectory = std::move(directory);
    }

    void LocalSourceAdapter::ScanRecursive(std::wstring const& directory, std::vector<Core::Installation>& result, int currentDepth)
    {
        // 安全上限：避免超大 / 异常深的目录树把扫描拖死
        if (currentDepth > 24 || result.size() > 5000)
        {
            return;
        }
        // 跳过明显的系统 / 非游戏目录，既避免无意义的大量枚举，也防止卡死
        static const wchar_t* kSkipDirs[] = {
            L"windows", L"programdata", L"$recycle.bin", L"system volume information",
            L"appdata", L"boot", L"documents and settings", L"recovery", L"$sysreset",
            L"perflogs", L"msocache", L"node_modules", L"$windows.~bt", L"$windows.~ws"
        };
        std::wstring dirName = DirectoryName(directory);
        for (auto const& s : kSkipDirs)
        {
            if (dirName == s)
            {
                return;
            }
        }
        auto lowerDirectory = Lower(directory);
        for (auto const& excluded : m_excludedDirectories)
        {
            if (lowerDirectory == Lower(excluded))
            {
                return;
            }
        }
        WIN32_FIND_DATAW findData{};
        HANDLE find = FindFirstFileW((directory + L"\\*").c_str(), &findData);
        if (find == INVALID_HANDLE_VALUE)
        {
            return;
        }
        bool descend = (m_scanDepth >= 3) || (currentDepth < m_scanDepth);
        do
        {
            std::wstring name = findData.cFileName;
            if (name == L"." || name == L"..")
            {
                continue;
            }
            std::wstring fullPath = directory + L"\\" + name;
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                // 不进入符号链接 / junction（如 AppData\Application Data → AppData\Local），
                // 否则会形成环导致无限递归、扫描卡死
                if (descend && (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
                {
                    ScanRecursive(fullPath, result, currentDepth + 1);
                }
                continue;
            }
            auto lowerName = Lower(name);
            for (auto const& extension : m_extensions)
            {
                if (lowerName.size() > extension.size()
                    && lowerName.compare(lowerName.size() - extension.size(), extension.size(), extension) == 0)
                {
                    if (IsLikelyNotGame(lowerName, lowerDirectory)
                        || VersionInfoLooksAuxiliary(fullPath))
                    {
                        ++m_filtered;
                        break;
                    }
                    Core::Installation installation;
                    installation.Source = Core::GameSourceType::Local;
                    installation.SourceKey = Lower(fullPath);
                    installation.Name = FileNameWithoutExtension(name);
                    installation.InstallPath = fullPath;
                    installation.LaunchTarget = fullPath;
                    auto slash = fullPath.find_last_of(L'\\');
                    installation.WorkingDirectory = slash == std::wstring::npos ? fullPath : fullPath.substr(0, slash);
                    result.push_back(std::move(installation));
                    break;
                }
            }
        } while (FindNextFileW(find, &findData) != 0);
        FindClose(find);
    }

    std::vector<Core::Installation> LocalSourceAdapter::DiscoverInstallations()
    {
        std::vector<Core::Installation> result;
        if (!m_enabled || m_rootDirectory.empty())
        {
            return result;
        }
        m_filtered = 0;
        ScanRecursive(m_rootDirectory, result, 1);
        return SelectPrimaryExecutables(result, m_rootDirectory);
    }

    bool LocalSourceAdapter::LaunchInstallation(Core::Installation const& installation)
    {
        std::wstring workingDirectory = installation.WorkingDirectory;
        if (workingDirectory.empty() && !installation.LaunchTarget.empty())
        {
            auto slash = installation.LaunchTarget.find_last_of(L'\\');
            workingDirectory = slash == std::wstring::npos
                ? installation.LaunchTarget
                : installation.LaunchTarget.substr(0, slash);
        }
        auto command = L"\"" + installation.LaunchTarget + L"\" " + installation.Arguments;
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        std::vector<wchar_t> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back(L'\0');
        BOOL ok = CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
            CREATE_UNICODE_ENVIRONMENT, nullptr,
            workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
            &startup, &process);
        if (!ok)
        {
            return false;
        }
        CloseHandle(process.hThread);
        // 保留进程句柄用于启动页检测真实退出，旧句柄先释放
        if (m_hProcess != nullptr)
        {
            CloseHandle(reinterpret_cast<HANDLE>(m_hProcess));
        }
        m_hProcess = process.hProcess;
        return true;
    }

    bool LocalSourceAdapter::IsRunning() const
    {
        if (m_hProcess == nullptr)
        {
            return false;
        }
        DWORD code = 0;
        if (GetExitCodeProcess(reinterpret_cast<HANDLE>(m_hProcess), &code) && code == STILL_ACTIVE)
        {
            return true;
        }
        // 进程已退出，释放句柄并复位
        CloseHandle(reinterpret_cast<HANDLE>(m_hProcess));
        const_cast<LocalSourceAdapter*>(this)->m_hProcess = nullptr;
        return false;
    }
}
