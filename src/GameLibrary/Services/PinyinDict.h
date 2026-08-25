#pragma once

#include <string>
#include <vector>

namespace Services
{
    // 轻量拼音候选字典：词表前缀匹配优先，其次单字，最多 24 个。
    // 数据为内置静态表（对齐前端 src/lib/pinyin.ts 的覆盖场景）。
    class PinyinDict final
    {
    public:
        static std::vector<std::wstring> GetCandidates(std::wstring const& buffer);
    };
}