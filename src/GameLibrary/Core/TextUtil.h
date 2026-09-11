#pragma once

// 文本 / URL 处理小工具（header-only）。
//
// 这两个函数原本在 IgdbMetadataProvider、SteamGridDbProvider、MetadataService
// 三处各复制了一份（FindJsonString 三份、UrlEncode 两份，函数体逐字相同）。
// 复制品一多就会出现「只改了一处」的隐性分叉，统一收在这里。

// UrlEncode 需要 WideCharToMultiByte。生产代码里每个 .cpp 都先 include pch.h
// （其中已有 windows.h），这里再写一次是为了让本头文件自给自足。
#include <Windows.h>

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

    // 百分号编码，返回 UTF-8 字节串。不编码的字符集与 RFC 3986 的 unreserved 一致。
    //
    // 【为什么必须先转 UTF-8】百分号编码的定义域是【字节】，不是字符。旧实现直接遍历
    // wchar_t 取低字节，非 ASCII 会被压成一个 %XX：
    //     「中」U+4E2D -> 低字节 0x2D -> "%2D"        （错）
    //     正确结果                  -> "%E4%B8%AD"    （UTF-8 三字节）
    // 服务端按 UTF-8 解码时必然对不上，表现为「用中文检索词一个都搜不到」。
    // 所以先 WideCharToMultiByte(CP_UTF8) 得到字节串，再逐字节编码。
    inline std::string UrlEncode(std::wstring const& value)
    {
        std::string result;
        if (value.empty())
        {
            return result;
        }

        // UTF-16 -> UTF-8。代理对（emoji 等）会变成 4 字节，交给系统转换即可。
        int const utf8Length = ::WideCharToMultiByte(CP_UTF8, 0, value.c_str(),
            static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (utf8Length <= 0)
        {
            return result;
        }
        std::string utf8(static_cast<size_t>(utf8Length), '\0');
        // 用 &utf8[0] 而不是 utf8.data()：非 const 的 data() 重载是 C++17 才有的，
        // 在 C++14 下它返回 const char*，这里需要一个可写缓冲。
        ::WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
            &utf8[0], utf8Length, nullptr, nullptr);

        char const hex[] = "0123456789ABCDEF";
        // 最坏情形是每个字节都编码成 3 个字符（"%XX"），一次备足避免反复扩容。
        result.reserve(utf8.size() * 3);
        // 必须是 unsigned char：带符号的 char 在 >= 0x80 时是负数，
        // (code >> 4) & 0xF 会拿到错误的高位。
        for (unsigned char byte : utf8)
        {
            if ((byte >= '0' && byte <= '9') || (byte >= 'A' && byte <= 'Z')
                || (byte >= 'a' && byte <= 'z')
                || byte == '-' || byte == '_' || byte == '.' || byte == '~')
            {
                result.push_back(static_cast<char>(byte));
            }
            else
            {
                result.push_back('%');
                result.push_back(hex[(byte >> 4) & 0xF]);
                result.push_back(hex[byte & 0xF]);
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
