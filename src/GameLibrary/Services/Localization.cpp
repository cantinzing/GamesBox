#include "pch.h"
#include "Localization.h"

#include <algorithm>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>

namespace Services
{
    namespace
    {
        // 中文为 source of truth / fallback
        std::unordered_map<std::wstring, std::wstring> const& Zh()
        {
            static std::unordered_map<std::wstring, std::wstring> const s{
                {L"common.back", L"返回"},
                {L"common.cancel", L"取消"},
                {L"common.save", L"保存"},
                {L"common.delete", L"删除"},
                {L"common.clear", L"清空"},
                {L"common.close", L"关闭"},
                {L"common.ok", L"确定"},
                {L"common.loading", L"设置中…"},

                {L"nav.games", L"游戏"},
                {L"nav.library", L"库"},

                {L"library.manage", L"批量"},
                {L"library.selectAll", L"全选"},
                {L"library.unselectAll", L"取消全选"},
                {L"library.removeSelected", L"移除所选 ({n})"},
                {L"library.removeSelectedTitle", L"批量移除游戏"},
                {L"library.removeSelectedBody", L"确定要从游戏库中移除选中的 {n} 款游戏吗？该操作不可撤销。"},
                {L"library.remove", L"移除"},
                {L"library.gameCount", L"{a} / {b} 款游戏"},
                {L"library.deleteConfirmBody", L"确定要从游戏库中删除这款游戏吗？该操作不可撤销。"},
                {L"library.sortManualHint", L"手动排序需先清除搜索 / 标签 / 收藏 / 来源筛选。"},

                {L"detail.delete", L"删除"},
                {L"detail.launch", L"启动"},
                {L"detail.edit", L"编辑"},
                {L"detail.assets", L"素材"},
                {L"detail.tags", L"标签"},
                {L"detail.manageTags", L"管理标签"},
                {L"detail.playRecords", L"游玩记录"},
                {L"detail.clearSessions", L"清空记录"},
                {L"detail.appIdPlaceholder", L"手动输入 AppID"},
                {L"detail.saveAppId", L"保存"},
                {L"detail.autoMatch", L"自动匹配"},
                {L"detail.clearAppId", L"清除"},
                {L"detail.platform", L"平台: {p}"},
                {L"detail.playTime", L"累计游玩 {n} 分钟"},
                {L"detail.noDescription", L"暂无简介。"},
                {L"detail.noSelection", L"未选择游戏"},
                {L"detail.noSelectionDesc", L"从游戏库选择一个游戏查看详情。"},
                {L"detail.noSessions", L"暂无游玩记录"},
                {L"detail.inLibrary", L"在库中"},
                {L"detail.playtime", L"游玩时长"},
                {L"detail.totalSessions", L"总会话数"},
                {L"detail.sessionLog", L"会话记录"},
                {L"detail.favorite", L"♡ 收藏"},
                {L"detail.favorited", L"★ 已收藏"},
                {L"library.title", L"库"},
                {L"library.import", L"导入"},
                {L"library.search", L"搜索"},
                {L"library.allTags", L"全部标签"},
                {L"library.sort.name", L"名称"},
                {L"library.sort.recent", L"最近游玩"},
                {L"library.sort.playtime", L"游玩时长"},
                {L"library.sort.added", L"加入时间"},
                {L"library.sort.manual", L"手动"},
                {L"library.orderDesc", L"↓ 降序"},
                {L"library.orderAsc", L"↑ 升序"},
                {L"library.favoriteToggle", L"★ 收藏"},
                {L"detail.deleteConfirmBody", L"确定要从游戏库中删除这款游戏吗？该操作不可撤销。"},
                {L"detail.deleteTitle", L"删除游戏"},
                {L"detail.noTags", L"暂无标签"},
                {L"detail.sessionMinutes", L"{n} 分钟"},
                {L"detail.name", L"名称"},
                {L"detail.namePlaceholder", L"游戏名称"},
                {L"detail.kbInput", L"⌨ 手柄输入"},
                {L"detail.desc", L"简介"},
                {L"detail.descPlaceholder", L"游戏简介"},
                {L"detail.editTitle", L"编辑游戏"},
                {L"detail.newTagPlaceholder", L"新标签名称"},
                {L"detail.createTag", L"创建并添加"},
                {L"detail.confirmDeleteTag", L"确认?"},
                {L"detail.clearSessionsTitle", L"清空游玩记录"},
                {L"detail.clearSessionsBody", L"确定要清除该游戏的全部游玩记录吗？"},
                {L"detail.assetsTitle", L"素材管理"},
                {L"detail.importCover", L"导入本地封面…"},
                {L"detail.importBackground", L"导入本地背景…"},
                {L"detail.fetchMetadata", L"在线抓取元数据…"},
                {L"detail.fetching", L"正在获取在线元数据…"},
                {L"detail.metadataTitle", L"在线元数据"},
                {L"detail.applyDesc", L"应用此简介"},
                {L"detail.artworkStrip", L"（{n} 张，点击选用）"},
                {L"detail.coverCandidates", L"封面候选"},
                {L"detail.bgCandidates", L"背景候选"},
                {L"detail.noMetadata", L"未找到可用的元数据。请在设置中配置 IGDB / SteamGridDB。"},
                {L"detail.searchSteamPlaceholder", L"搜索 Steam 商店"},
                {L"detail.searchSteam", L"搜索"},
                {L"detail.searchSteamHint", L"输入关键词搜索 Steam 商店，点选结果即可写入 AppID。"},
                {L"detail.autoMatchTitle", L"自动匹配 Steam AppID"},
                {L"detail.noMatch", L"未找到匹配结果。"},
                {L"detail.coverArt", L"封面"},
                {L"detail.steamAppId", L"Steam AppID"},

                {L"import.title", L"导入本地游戏"},
                {L"import.wizard", L"导入向导"},
                {L"import.step1of2", L"第 1 步，共 2 步"},
                {L"import.foundCandidates", L"找到候选"},
                {L"import.desc", L"选择包含游戏可执行文件的目录，扫描候选并加入游戏库。Steam 与 Epic 游戏可在设置中启用自动发现。"},
                {L"import.chooseSource", L"选择来源"},
                {L"import.local", L"本地文件"},
                {L"import.hintLocal", L"选择包含游戏可执行文件的目录，扫描候选并加入游戏库。"},
                {L"import.hintSteam", L"扫描已安装的 Steam 游戏库（自动检测 Steam 安装路径）。"},
                {L"import.hintEpic", L"扫描已安装的 Epic 游戏库（自动检测 Epic 安装清单）。"},
                {L"import.pathPlaceholder", L"例如 D:\\Games"},
                {L"import.browse", L"浏览"},
                {L"import.scan", L"扫描"},
                {L"import.importSelected", L"导入已选"},
                {L"import.candidates", L"· {n} 项"},
                {L"import.statusPickerUnavailable", L"文件夹选择器不可用，请直接输入路径后点击扫描。"},
                {L"import.statusDbNotReady", L"数据库未就绪"},
                {L"import.statusFillPath", L"请先填写目录路径。"},
                {L"import.statusAdapterUnavailable", L"来源适配器不可用"},
                {L"import.statusFound", L"{n} 个新候选。"},
                {L"import.statusNoCandidates", L"没有候选可导入，请先扫描。"},
                {L"import.statusImported", L"已导入 {n} 款游戏。"},
                {L"import.selectAll", L"全选"},
                {L"import.invertSelection", L"反选"},

                {L"home.emptyTitle", L"你的游戏库还是空的"},
                {L"home.emptySubtitle", L"前往游戏库导入你的 Steam / Epic / 本地游戏，统一管理与启动。"},
                {L"home.import", L"导入游戏"},
                {L"home.play", L"启动"},
                {L"home.continue", L"继续"},
                {L"home.detail", L"详情"},
                {L"home.played", L"已游玩"},
                {L"home.notPlayed", L"未游玩"},
                {L"home.noDescription", L"该游戏暂无简介。"},
                {L"home.justNow", L"刚刚"},
                {L"home.minAgo", L"{n} 分钟前"},
                {L"home.hourAgo", L"{n} 小时前"},
                {L"home.yesterday", L"昨天"},
                {L"home.dayAgo", L"{n} 天前"},
                {L"home.local", L"本地"},
                {L"home.autoplay", L"自动轮播"},
                {L"home.autoplay.pause", L"暂停自动轮播"},
                {L"home.autoplay.resume", L"继续自动轮播"},

                {L"launch.starting", L"正在启动游戏…"},
                {L"launch.checkingUpdate", L"正在检查更新…"},
                {L"launch.syncingCloud", L"正在同步云端数据…"},
                {L"launch.compilingShader", L"正在编译着色器…"},
                {L"launch.ready", L"准备就绪"},
                {L"launch.noGamepad", L"未检测到手柄"},

                {L"vk.pinyinPlaceholder", L"拼音…"},
                {L"vk.multilineHint", L"多行文本：用「手柄输入」拼写后按确定。"},
                {L"vk.space", L"空格"},
                {L"vk.zh", L"中文"},
                {L"vk.en", L"英文"},
                {L"vk.cancel", L"取消"},
                {L"vk.ok", L"确定"},
                {L"vk.noCandidates", L"无候选"},
                {L"vk.empty", L"（空）"},

                {L"platform.steam", L"Steam"},
                {L"platform.epic", L"Epic"},
                {L"platform.local", L"本地"},

                {L"search.placeholder", L"搜索游戏…"},
                {L"search.allSources", L"全部来源"},
                {L"search.local", L"本地"},
                {L"search.sortName", L"按名称"},
                {L"search.sortRecent", L"按最近游玩"},
                {L"search.sortTime", L"按游玩时长"},
                {L"search.favoritesOnly", L"★ 只看收藏"},
                {L"search.empty", L"游戏库为空"},
                {L"search.noMatch", L"没有匹配的游戏"},
                {L"search.loadMore", L"加载更多"},
                {L"search.notPlayed", L"未游玩"},
                {L"search.count", L"{n} 款游戏"},

                {L"help.title", L"帮助"},
                {L"help.shortcuts", L"快捷键"},
                {L"help.keys.search", L"Ctrl + F — 全局搜索"},
                {L"help.keys.esc", L"Esc — 关闭当前覆盖层"},
                {L"help.keys.help", L"F1 — 打开 / 关闭帮助"},
                {L"help.usage", L"使用说明"},
                {L"help.u1", L"· 在首页轮播中选择或启动游戏"},
                {L"help.u2", L"· 在库页导入 Steam / Epic / 本地游戏"},
                {L"help.u3", L"· 详情页可编辑、打标签、管理素材与记录"},
                {L"help.u4", L"· 前往设置配置 IGDB / SteamGridDB 抓取元数据"},
                {L"help.gotIt", L"我知道了"},

                {L"settings.title", L"设置"},
                {L"settings.sourceDetection", L"来源检测"},
                {L"settings.stateNotInstalled", L"未安装"},
                {L"settings.stateDisabled", L"已禁用"},
                {L"settings.stateReady", L"已就绪"},
                {L"settings.stateDetected", L"已检测到安装"},
                {L"settings.stateNotDetected", L"未在本机检测到"},
                {L"settings.sourceDetectionHint", L"检测本机已安装的游戏平台。"},
                {L"settings.library", L"游戏库"},
                {L"settings.scanRoot", L"本地扫描目录"},
                {L"settings.scanRootPh", L"例如 D:\Games"},
                {L"settings.scanDepth", L"扫描深度"},
                {L"settings.scanDepth.1", L"浅层（仅根目录）"},
                {L"settings.scanDepth.2", L"中等（2 层）"},
                {L"settings.scanDepth.3", L"深层（递归全部）"},
                {L"settings.metadata", L"在线元数据"},
                {L"settings.provider", L"元数据提供方"},
                {L"settings.provider.auto", L"自动（IGDB + SGDB）"},
                {L"settings.provider.igdb", L"仅 IGDB"},
                {L"settings.provider.sgdb", L"仅 SteamGridDB"},
                {L"settings.autoFetch", L"导入后自动抓取元数据"},
                {L"settings.appearance", L"外观"},
                {L"settings.heroInterval", L"首页轮播间隔（秒）"},
                {L"settings.autoCarousel", L"首页自动轮播"},
                {L"settings.heroIntervalHint", L"范围 3–30 秒，控制首页 Hero 卡片与动态背景自动切换节奏。"},
                {L"settings.language", L"语言"},
                {L"settings.languageHint", L"切换后立即全局生效。"},
                {L"settings.igdbTitle", L"IGDB"},
                {L"settings.igdbHint", L"用于获取游戏简介与封面。在 https://dev.twitch.tv/console/apps 注册应用。"},
                {L"settings.igdbClientId", L"客户端 ID"},
                {L"settings.igdbSecret", L"客户端密钥"},
                {L"settings.sgdbTitle", L"SteamGridDB"},
                {L"settings.sgdbHint", L"用于获取游戏网格图片与封面。在 https://www.steamgriddb.com/profile/preferences/api 申请密钥。"},
                {L"settings.sgdbKey", L"API 密钥"},
                {L"settings.save", L"保存设置"},
                {L"settings.saved", L"设置已保存"},
                {L"settings.languageSwitched", L"语言已切换。"},
                {L"settings.on", L"开启"},
                {L"settings.off", L"关闭"},

                {L"error.notReady", L"数据库未就绪"},

                {L"vk.im", L"手柄输入"},
            };
            return s;
        }

        std::unordered_map<std::wstring, std::wstring> const& En()
        {
            static std::unordered_map<std::wstring, std::wstring> const s{
                {L"common.back", L"Back"},
                {L"common.cancel", L"Cancel"},
                {L"common.save", L"Save"},
                {L"common.delete", L"Delete"},
                {L"common.clear", L"Clear"},
                {L"common.close", L"Close"},
                {L"common.ok", L"OK"},
                {L"common.loading", L"Setting up…"},

                {L"nav.games", L"Games"},
                {L"nav.library", L"Library"},

                {L"library.manage", L"Manage"},
                {L"library.selectAll", L"Select all"},
                {L"library.unselectAll", L"Deselect all"},
                {L"library.removeSelected", L"Remove selected ({n})"},
                {L"library.removeSelectedTitle", L"Remove games"},
                {L"library.removeSelectedBody", L"Remove the {n} selected games from the library? This cannot be undone."},
                {L"library.remove", L"Remove"},
                {L"library.gameCount", L"{a} / {b} games"},
                {L"library.deleteConfirmBody", L"Delete this game from the library? This cannot be undone."},
                {L"library.sortManualHint", L"Manual sorting requires clearing search / tags / favorites / source filters first."},

                {L"detail.delete", L"Delete"},
                {L"detail.launch", L"Launch"},
                {L"detail.edit", L"Edit"},
                {L"detail.assets", L"Assets"},
                {L"detail.tags", L"Tags"},
                {L"detail.manageTags", L"Manage tags"},
                {L"detail.playRecords", L"Play sessions"},
                {L"detail.clearSessions", L"Clear"},
                {L"detail.appIdPlaceholder", L"Enter AppID manually"},
                {L"detail.saveAppId", L"Save"},
                {L"detail.autoMatch", L"Auto match"},
                {L"detail.clearAppId", L"Clear"},
                {L"detail.platform", L"Platform: {p}"},
                {L"detail.playTime", L"{n} min played"},
                {L"detail.noDescription", L"No description yet."},
                {L"detail.noSelection", L"No game selected"},
                {L"detail.noSelectionDesc", L"Select a game from the library to view its details."},
                {L"detail.noSessions", L"No play sessions yet"},
                {L"detail.inLibrary", L"IN LIBRARY"},
                {L"detail.playtime", L"PLAYTIME"},
                {L"detail.totalSessions", L"TOTAL SESSIONS"},
                {L"detail.sessionLog", L"SESSION LOG"},
                {L"detail.favorite", L"♡ Favorite"},
                {L"detail.favorited", L"★ Favorited"},
                {L"library.title", L"Library"},
                {L"library.import", L"Import"},
                {L"library.search", L"Search"},
                {L"library.allTags", L"All Tags"},
                {L"library.sort.name", L"Name"},
                {L"library.sort.recent", L"Recently Played"},
                {L"library.sort.playtime", L"Play Time"},
                {L"library.sort.added", L"Date Added"},
                {L"library.sort.manual", L"Manual"},
                {L"library.orderDesc", L"↓ Desc"},
                {L"library.orderAsc", L"↑ Asc"},
                {L"library.favoriteToggle", L"★ Favorite"},
                {L"detail.deleteConfirmBody", L"Delete this game from the library? This cannot be undone."},
                {L"detail.deleteTitle", L"Delete game"},
                {L"detail.noTags", L"No tags yet"},
                {L"detail.sessionMinutes", L"{n} min"},
                {L"detail.name", L"Name"},
                {L"detail.namePlaceholder", L"Game name"},
                {L"detail.kbInput", L"⌨ Gamepad input"},
                {L"detail.desc", L"Description"},
                {L"detail.descPlaceholder", L"Game description"},
                {L"detail.editTitle", L"Edit game"},
                {L"detail.newTagPlaceholder", L"New tag name"},
                {L"detail.createTag", L"Create & add"},
                {L"detail.confirmDeleteTag", L"Delete?"},
                {L"detail.clearSessionsTitle", L"Clear play records"},
                {L"detail.clearSessionsBody", L"Clear all play records for this game?"},
                {L"detail.assetsTitle", L"Assets"},
                {L"detail.importCover", L"Import local cover…"},
                {L"detail.importBackground", L"Import local background…"},
                {L"detail.fetchMetadata", L"Fetch metadata online…"},
                {L"detail.fetching", L"Fetching online metadata…"},
                {L"detail.metadataTitle", L"Online metadata"},
                {L"detail.applyDesc", L"Apply description"},
                {L"detail.artworkStrip", L"({n} images, click to use)"},
                {L"detail.coverCandidates", L"Cover candidates"},
                {L"detail.bgCandidates", L"Background candidates"},
                {L"detail.noMetadata", L"No metadata found. Configure IGDB / SteamGridDB in settings."},
                {L"detail.searchSteamPlaceholder", L"Search Steam store"},
                {L"detail.searchSteam", L"Search"},
                {L"detail.searchSteamHint", L"Enter keywords to search the Steam store; click a result to fill in the AppID."},
                {L"detail.autoMatchTitle", L"Auto-match Steam AppID"},
                {L"detail.noMatch", L"No matching results."},
                {L"detail.coverArt", L"COVER ART"},
                {L"detail.steamAppId", L"STEAM APP ID"},

                {L"import.title", L"Import local games"},
                {L"import.wizard", L"IMPORT WIZARD"},
                {L"import.step1of2", L"STEP 1 OF 2"},
                {L"import.foundCandidates", L"FOUND CANDIDATES"},
                {L"import.desc", L"Choose a folder containing game executables to scan for candidates and add them to the library. Steam and Epic games can be auto-discovered in Settings."},
                {L"import.chooseSource", L"Source"},
                {L"import.local", L"Local files"},
                {L"import.hintLocal", L"Choose a folder containing game executables to scan for candidates and add them to the library."},
                {L"import.hintSteam", L"Scan the installed Steam library (Steam install path is auto-detected)."},
                {L"import.hintEpic", L"Scan the installed Epic library (Epic manifest is auto-detected)."},
                {L"import.pathPlaceholder", L"e.g. D:\\Games"},
                {L"import.browse", L"Browse"},
                {L"import.scan", L"Scan"},
                {L"import.importSelected", L"Import selected"},
                {L"import.candidates", L"· {n} items"},
                {L"import.statusPickerUnavailable", L"Folder picker unavailable; type a path and press Scan."},
                {L"import.statusDbNotReady", L"Database not ready"},
                {L"import.statusFillPath", L"Enter a folder path first."},
                {L"import.statusAdapterUnavailable", L"Source adapter unavailable"},
                {L"import.statusFound", L"{n} new candidates."},
                {L"import.statusNoCandidates", L"No candidates to import; scan first."},
                {L"import.statusImported", L"Imported {n} games."},
                {L"import.selectAll", L"Select all"},
                {L"import.invertSelection", L"Invert"},

                {L"home.emptyTitle", L"Your library is empty"},
                {L"home.emptySubtitle", L"Import your Steam / Epic / local games from the library to manage and launch them all in one place."},
                {L"home.import", L"Import games"},
                {L"home.play", L"Play"},
                {L"home.continue", L"Continue"},
                {L"home.detail", L"Detail"},
                {L"home.played", L"PLAYED"},
                {L"home.notPlayed", L"未游玩"},
                {L"home.noDescription", L"No description yet."},
                {L"home.justNow", L"Just now"},
                {L"home.minAgo", L"{n} min ago"},
                {L"home.hourAgo", L"{n} hr ago"},
                {L"home.yesterday", L"Yesterday"},
                {L"home.dayAgo", L"{n} days ago"},
                {L"home.local", L"Local"},
                {L"home.autoplay", L"Auto carousel"},
                {L"home.autoplay.pause", L"Pause auto-rotation"},
                {L"home.autoplay.resume", L"Resume auto-rotation"},

                {L"launch.starting", L"Starting game…"},
                {L"launch.checkingUpdate", L"Checking for updates…"},
                {L"launch.syncingCloud", L"Syncing cloud data…"},
                {L"launch.compilingShader", L"Compiling shaders…"},
                {L"launch.ready", L"Ready"},
                {L"launch.noGamepad", L"No gamepad detected"},

                {L"vk.pinyinPlaceholder", L"Pinyin…"},
                {L"vk.multilineHint", L"Multiline text: spell with gamepad input, then press OK."},
                {L"vk.space", L"Space"},
                {L"vk.zh", L"中文"},
                {L"vk.en", L"EN"},
                {L"vk.cancel", L"Cancel"},
                {L"vk.ok", L"OK"},
                {L"vk.noCandidates", L"No candidates"},
                {L"vk.empty", L"(empty)"},

                {L"platform.steam", L"Steam"},
                {L"platform.epic", L"Epic"},
                {L"platform.local", L"Local"},

                {L"search.placeholder", L"Search games…"},
                {L"search.allSources", L"All sources"},
                {L"search.local", L"Local"},
                {L"search.sortName", L"By name"},
                {L"search.sortRecent", L"Recently played"},
                {L"search.sortTime", L"Play time"},
                {L"search.favoritesOnly", L"★ Favorites only"},
                {L"search.empty", L"Library is empty"},
                {L"search.noMatch", L"No matching games"},
                {L"search.loadMore", L"Load more"},
                {L"search.notPlayed", L"Not played"},
                {L"search.count", L"{n} games"},

                {L"help.title", L"Help"},
                {L"help.shortcuts", L"Shortcuts"},
                {L"help.keys.search", L"Ctrl + F — Global search"},
                {L"help.keys.esc", L"Esc — Close current overlay"},
                {L"help.keys.help", L"F1 — Toggle help"},
                {L"help.usage", L"How to use"},
                {L"help.u1", L"· Pick or launch a game on the home carousel"},
                {L"help.u2", L"· Import Steam / Epic / local games in the library"},
                {L"help.u3", L"· Detail page: edit, tag, manage assets & history"},
                {L"help.u4", L"· Configure IGDB / SteamGridDB in settings to fetch metadata"},
                {L"help.gotIt", L"Got it"},

                {L"settings.title", L"Settings"},
                {L"settings.sourceDetection", L"Source detection"},
                {L"settings.stateNotInstalled", L"Not installed"},
                {L"settings.stateDisabled", L"Disabled"},
                {L"settings.stateReady", L"Ready"},
                {L"settings.stateDetected", L"Install detected"},
                {L"settings.stateNotDetected", L"Not detected on this machine"},
                {L"settings.sourceDetectionHint", L"Detect game platforms installed on this machine."},
                {L"settings.library", L"Library"},
                {L"settings.scanRoot", L"Local scan folder"},
                {L"settings.scanRootPh", L"e.g. D:\Games"},
                {L"settings.scanDepth", L"Scan depth"},
                {L"settings.scanDepth.1", L"Shallow (root only)"},
                {L"settings.scanDepth.2", L"Medium (2 levels)"},
                {L"settings.scanDepth.3", L"Deep (recursive all)"},
                {L"settings.metadata", L"Online metadata"},
                {L"settings.provider", L"Metadata provider"},
                {L"settings.provider.auto", L"Auto (IGDB + SGDB)"},
                {L"settings.provider.igdb", L"IGDB only"},
                {L"settings.provider.sgdb", L"SteamGridDB only"},
                {L"settings.autoFetch", L"Auto-fetch metadata after import"},
                {L"settings.appearance", L"Appearance"},
                {L"settings.heroInterval", L"Carousel interval (seconds)"},
                {L"settings.autoCarousel", L"Home auto-carousel"},
                {L"settings.heroIntervalHint", L"Range 3–30s; controls auto-switching of the home Hero cards and background."},
                {L"settings.language", L"Language"},
                {L"settings.languageHint", L"Language changes apply immediately."},
                {L"settings.igdbTitle", L"IGDB"},
                {L"settings.igdbHint", L"Used to fetch game descriptions and covers. Register an app at https://dev.twitch.tv/console/apps"},
                {L"settings.igdbClientId", L"Client ID"},
                {L"settings.igdbSecret", L"Client Secret"},
                {L"settings.sgdbTitle", L"SteamGridDB"},
                {L"settings.sgdbHint", L"Used to fetch grid images and covers. Get a key at https://www.steamgriddb.com/profile/preferences/api"},
                {L"settings.sgdbKey", L"API Key"},
                {L"settings.save", L"Save settings"},
                {L"settings.saved", L"Settings saved"},
                {L"settings.languageSwitched", L"Language switched."},
                {L"settings.on", L"On"},
                {L"settings.off", L"Off"},

                {L"error.notReady", L"Database not ready"},

                {L"vk.im", L"Gamepad input"},
            };
            return s;
        }

        std::wstring ReplaceVars(std::wstring text,
            std::unordered_map<std::wstring, std::wstring> const& vars)
        {
            for (auto const& [key, value] : vars)
            {
                std::wstring placeholder = L"{" + key + L"}";
                size_t pos = 0;
                while ((pos = text.find(placeholder, pos)) != std::wstring::npos)
                {
                    text.replace(pos, placeholder.size(), value);
                    pos += value.size();
                }
            }
            return text;
        }
    }

    Localization& Localization::Instance()
    {
        static Localization instance;
        return instance;
    }

    std::wstring const& Localization::Language() const
    {
        return m_language;
    }

    void Localization::SetLanguage(std::wstring const& language)
    {
        if (m_language == language)
        {
            return;
        }
        m_language = language;
        Notify();
    }

    std::uint64_t Localization::Subscribe(LanguageChanged const& callback)
    {
        auto token = m_nextToken++;
        m_listeners[token] = callback;
        return token;
    }

    void Localization::Unsubscribe(std::uint64_t token)
    {
        m_listeners.erase(token);
    }

    void Localization::Notify()
    {
        auto listeners = m_listeners;
        for (auto const& [token, callback] : listeners)
        {
            (void)token;
            if (callback)
            {
                callback();
            }
        }
    }

    std::wstring Localization::T(std::wstring const& key) const
    {
        auto const& table = (m_language == L"en") ? En() : Zh();
        auto it = table.find(key);
        if (it != table.end())
        {
            return it->second;
        }
        auto zh = Zh().find(key);
        return zh != Zh().end() ? zh->second : key;
    }

    std::wstring Localization::T(std::wstring const& key,
        std::unordered_map<std::wstring, std::wstring> const& vars) const
    {
        return ReplaceVars(T(key), vars);
    }

    void Localization::LocalizeVisualTree(winrt::Microsoft::UI::Xaml::DependencyObject const& root)
    {
        namespace xaml = winrt::Microsoft::UI::Xaml;
        namespace controls = winrt::Microsoft::UI::Xaml::Controls;
        namespace primitives = winrt::Microsoft::UI::Xaml::Controls::Primitives;
        namespace media = winrt::Microsoft::UI::Xaml::Media;

        auto readTag = [](xaml::FrameworkElement const& el) -> winrt::hstring {
            auto tag = el.Tag();
            if (!tag)
            {
                return {};
            }
            return winrt::unbox_value_or<winrt::hstring>(tag, winrt::hstring{});
        };
        auto apply = [this](winrt::hstring const& tag) -> std::wstring {
            std::wstring ts(tag.data(), tag.size());
            if (ts.size() <= 5 || ts.rfind(L"i18n:", 0) != 0)
            {
                return {};
            }
            return T(ts.substr(5));
        };

        int count = media::VisualTreeHelper::GetChildrenCount(root);
        for (int i = 0; i < count; ++i)
        {
            auto child = media::VisualTreeHelper::GetChild(root, i);
            if (!child)
            {
                continue;
            }
            if (auto tb = child.try_as<controls::TextBlock>())
            {
                auto tag = readTag(tb);
                auto value = apply(tag);
                if (!value.empty())
                {
                    tb.Text(winrt::hstring(value));
                }
            }
            else if (auto box = child.try_as<controls::TextBox>())
            {
                auto tag = readTag(box);
                std::wstring ts(tag.data(), tag.size());
                if (ts.size() > 7 && ts.rfind(L"i18nph:", 0) == 0)
                {
                    box.PlaceholderText(winrt::hstring(T(ts.substr(7))));
                }
            }
            else if (auto pbox = child.try_as<controls::PasswordBox>())
            {
                auto tag = readTag(pbox);
                std::wstring ts(tag.data(), tag.size());
                if (ts.size() > 7 && ts.rfind(L"i18nph:", 0) == 0)
                {
                    pbox.PlaceholderText(winrt::hstring(T(ts.substr(7))));
                }
            }
            else if (auto btn = child.try_as<controls::Button>())
            {
                auto tag = readTag(btn);
                auto value = apply(tag);
                if (!value.empty() && btn.Content())
                {
                    btn.Content(winrt::box_value(winrt::hstring(value)));
                }
            }
            else if (auto tg = child.try_as<primitives::ToggleButton>())
            {
                auto tag = readTag(tg);
                auto value = apply(tag);
                if (!value.empty() && tg.Content())
                {
                    tg.Content(winrt::box_value(winrt::hstring(value)));
                }
            }
            else if (auto item = child.try_as<controls::ComboBoxItem>())
            {
                auto tag = readTag(item);
                auto value = apply(tag);
                if (!value.empty() && item.Content())
                {
                    item.Content(winrt::box_value(winrt::hstring(value)));
                }
            }
            LocalizeVisualTree(child);
        }
    }
}
