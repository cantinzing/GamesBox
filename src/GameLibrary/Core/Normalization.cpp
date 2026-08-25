#include "pch.h"
#include "Normalization.h"

#include <algorithm>
#include <cctype>
#include <cwctype>

namespace Core
{
    namespace
    {
        std::wstring const kSpecialSuffixes[] = {
            L": Definitive Edition", L" Definitive Edition", L": Game of the Year Edition",
            L" Game of the Year Edition", L": Enhanced Edition", L" Enhanced Edition",
            L": Anniversary Edition", L" Anniversary Edition", L" Remastered", L" Remaster",
            L": Deluxe Edition", L" Deluxe Edition", L": Gold Edition", L" Gold Edition",
            L": Complete Edition", L" Complete Edition", L": Collector's Edition",
            L" Collector's Edition", L" - Game of the Year", L" (Digital)"
        };
    }

    std::wstring NormalizeTitle(std::wstring const& title)
    {
        std::wstring result = title;
        for (auto& ch : result)
        {
            if (ch >= L'A' && ch <= L'Z')
            {
                ch = static_cast<wchar_t>(ch - L'A' + L'a');
            }
        }
        for (auto const& suffix : kSpecialSuffixes)
        {
            auto needle = suffix;
            std::transform(needle.begin(), needle.end(), needle.begin(), [](wchar_t c) {
                if (c >= L'A' && c <= L'Z')
                {
                    return static_cast<wchar_t>(c - L'A' + L'a');
                }
                return c;
            });
            if (result.size() > needle.size() && result.compare(result.size() - needle.size(), needle.size(), needle) == 0)
            {
                result.erase(result.size() - needle.size());
                break;
            }
        }
        while (!result.empty() && std::iswspace(result.back()))
        {
            result.pop_back();
        }
        return result;
    }
}
