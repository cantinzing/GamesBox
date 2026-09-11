#pragma once

// 文本 / URL 处理小工具（header-only）。
//
// 这两个函数原本在 IgdbMetadataProvider、SteamGridDbProvider、MetadataService
// 三处各复制了一份（FindJsonString 三份、UrlEncode 两份，函数体逐字相同）。
// 复制品一多就会出现「只改了一处」的隐性分叉，统一收在这里。

#include <string>

namespace Core
{
    // 极简 JSON 取值：在 json 文本里找到 key，返回紧随其后的字符串值。
    //
    // 这是启发式扫描，不是 JSON 解析器：不理解嵌套层级，key 在文档中重复出现时取第一个。
    // 只适用于本项目这种「扁平结构、值必为字符串」的响应体。返回空串表示没取到。
    inline std::string FindJsonString(std::string const& json, std::string const& key)
    {
        auto marker = json.find(key);
        if (marker == std::string::npos)
        {
            return {};
        }
        marker += key.size();
        while (marker < json.size() && json[marker] != ':')
        {
            ++marker;
        }
        if (marker >= json.size())
        {
            return {};
        }
        ++marker;
        while (marker < json.size() && (json[marker] == ' ' || json[marker] == '\t'
            || json[marker] == '\r' || json[marker] == '\n'))
        {
            ++marker;
        }
        if (marker >= json.size() || json[marker] != '"')
        {
            return {};
        }
        ++marker;
        std::string result;
        while (marker < json.size() && json[marker] != '"')
        {
            if (json[marker] == '\\' && marker + 1 < json.size())
            {
                ++marker;
            }
            result.push_back(json[marker]);
            ++marker;
        }
        return result;
    }

    // 百分号编码，返回 UTF-8 字节串（URL 本质就是字节流）。
    // 不编码的字符集与 RFC 3986 的 unreserved 一致。
    //
    // 注意：这里按 wchar_t 逐个字符处理，仅对 ASCII 输入正确 —— 非 ASCII（中日韩等）
    // 会得到「单字符百分号」而不是 UTF-8 多字节序列。这是合并前就有的既有行为，
    // 本次重构原样保留以免引入行为变化；如需支持中文检索词需另行改用
    // WideCharToMultiByte(CP_UTF8) 后再按字节编码。
    inline std::string UrlEncode(std::wstring const& value)
    {
        std::string result;
        char const hex[] = "0123456789ABCDEF";
        for (wchar_t ch : value)
        {
            if ((ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z')
                || (ch >= L'a' && ch <= L'z') || ch == L'-' || ch == L'_' || ch == L'.' || ch == L'~')
            {
                result.push_back(static_cast<char>(ch));
            }
            else
            {
                unsigned int code = static_cast<unsigned int>(ch);
                result.push_back('%');
                result.push_back(hex[(code >> 4) & 0xF]);
                result.push_back(hex[code & 0xF]);
            }
        }
        return result;
    }

    // 在宽字符 URL 里拼接时用的包装：编码结果全是 ASCII，逐字节加宽即可。
    inline std::wstring UrlEncodeW(std::wstring const& value)
    {
        auto narrow = UrlEncode(value);
        return std::wstring(narrow.begin(), narrow.end());
    }
}
