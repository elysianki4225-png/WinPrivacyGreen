#include "items.h"

#include <algorithm>

namespace wpg {

namespace {

const wchar_t* kCdm = L"Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager";
const wchar_t* kExplorerAdv = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
const wchar_t* kPrivacy = L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy";

ItemOp dword(Hive hive, const wchar_t* key, const wchar_t* name, DWORD block, DWORD open) {
    ItemOp op;
    op.addr = {hive, key, name};
    op.isString = false;
    op.blockDword = block;
    op.openDword = open;
    return op;
}

ItemOp str(Hive hive, const wchar_t* key, const wchar_t* name, const wchar_t* block, const wchar_t* open) {
    ItemOp op;
    op.addr = {hive, key, name};
    op.isString = true;
    op.blockString = block;
    op.openString = open;
    return op;
}

std::vector<ItemDef> buildCatalog() {
    std::vector<ItemDef> list;

    auto add = [&](const char* id, ItemGroup group, IconKind icon, const wchar_t* zhT, const wchar_t* enT,
                   const wchar_t* zhD, const wchar_t* enD, bool admin, int minBuild,
                   std::vector<ItemOp> ops) {
        ItemDef def;
        def.id = id;
        def.group = group;
        def.icon = icon;
        def.zhTitle = zhT;
        def.enTitle = enT;
        def.zhDesc = zhD;
        def.enDesc = enD;
        def.needsAdmin = admin;
        def.minBuild = minBuild;
        def.ops = std::move(ops);
        list.push_back(std::move(def));
    };

    // ---- Ads & recommendations -------------------------------------------
    add("ad_id", ItemGroup::Ads, IconKind::IdCard, L"广告 ID", L"Advertising ID",
        L"阻止应用使用广告 ID 向你推送个性化广告", L"Stops apps from using your advertising ID", false, 0,
        {dword(Hive::CurrentUser, L"Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo", L"Enabled", 0, 1)});

    add("start_suggestions", ItemGroup::Ads, IconKind::Sparkle, L"开始菜单推荐", L"Start menu suggestions",
        L"隐藏开始菜单里的应用推广条目", L"Removes promoted apps from Start", false, 0,
        {dword(Hive::CurrentUser, kCdm, L"SubscribedContent-338388Enabled", 0, 1)});

    add("silent_install", ItemGroup::Ads, IconKind::Cloud, L"自动安装推广应用", L"Silent app installs",
        L"阻止系统在后台自动装好推荐的应用", L"Stops Windows installing promoted apps silently", false, 0,
        {dword(Hive::CurrentUser, kCdm, L"SilentInstalledAppsEnabled", 0, 1)});

    add("settings_suggestions", ItemGroup::Ads, IconKind::Grid, L"设置页建议", L"Settings suggestions",
        L"关闭设置应用中出现的推荐内容", L"Turns off promoted cards in Settings", false, 0,
        {dword(Hive::CurrentUser, kCdm, L"SubscribedContent-338393Enabled", 0, 1)});

    add("lockscreen_ads", ItemGroup::Ads, IconKind::Lock, L"锁屏广告与提示", L"Lock screen ads & tips",
        L"关闭锁屏上的 Windows 聚焦推广信息", L"Disables Spotlight promotions on the lock screen", false, 0,
        {dword(Hive::CurrentUser, kCdm, L"RotatingLockScreenOverlayEnabled", 0, 1),
         dword(Hive::CurrentUser, kCdm, L"SubscribedContent-338387Enabled", 0, 1)});

    add("soft_landing", ItemGroup::Ads, IconKind::Sparkle, L"Windows 使用提示", L"Windows tips",
        L"不再弹出功能介绍与使用建议", L"Stops feature tips and suggestion popups", false, 0,
        {dword(Hive::CurrentUser, kCdm, L"SoftLandingEnabled", 0, 1)});

    add("finish_setup", ItemGroup::Ads, IconKind::Shield, L"完成设备设置提醒", L"Finish setup prompts",
        L"关闭“继续设置你的设备”之类的提示", L"Hides the 'finish setting up your device' prompt", false, 0,
        {dword(Hive::CurrentUser, L"Software\\Microsoft\\Windows\\CurrentVersion\\UserProfileEngagement",
               L"ScoobeSystemSettingEnabled", 0, 1)});

    // ---- Search & shell ---------------------------------------------------
    add("web_search", ItemGroup::Search, IconKind::Search, L"搜索框网络结果", L"Web results in search",
        L"搜索框只显示本机内容，不请求网页建议", L"Search shows local results only", false, 0,
        {dword(Hive::CurrentUser, L"Software\\Policies\\Microsoft\\Windows\\Explorer", L"DisableSearchBoxSuggestions",
               1, 0)});

    add("search_highlights", ItemGroup::Search, IconKind::Search, L"搜索亮点", L"Search highlights",
        L"去掉搜索面板里的节日与资讯插画", L"Removes seasonal cards in the search flyout", false, 0,
        {dword(Hive::CurrentUser, L"Software\\Microsoft\\Windows\\CurrentVersion\\SearchSettings",
               L"IsDynamicSearchBoxEnabled", 0, 1)});

    add("taskbar_widgets", ItemGroup::Search, IconKind::Grid, L"任务栏资讯小组件", L"Taskbar widgets",
        L"关闭任务栏的天气与资讯入口（Windows 11）", L"Removes the widgets button (Windows 11)", false, 22000,
        {dword(Hive::CurrentUser, kExplorerAdv, L"TaskbarDa", 0, 1)});

    add("start_recommendations", ItemGroup::Search, IconKind::Sparkle, L"开始菜单推荐文件", L"Recommended files",
        L"隐藏开始菜单“推荐”区域的文件建议（Windows 11）", L"Hides Recommended files in Start (Windows 11)", false,
        22621, {dword(Hive::CurrentUser, kExplorerAdv, L"Start_IrisRecommendations", 0, 1)});

    // ---- Activity & privacy ----------------------------------------------
    add("activity_history", ItemGroup::Privacy, IconKind::History, L"活动历史记录", L"Activity history",
        L"不记录、也不上传你在设备上的活动", L"Stops collecting and uploading your activity", false, 0,
        {dword(Hive::CurrentUser, kPrivacy, L"PublishUserActivities", 0, 1),
         dword(Hive::CurrentUser, kPrivacy, L"UploadUserActivities", 0, 1)});

    add("recent_docs", ItemGroup::Privacy, IconKind::Doc, L"最近使用的文件", L"Recent files",
        L"不在开始菜单记录最近打开的文件", L"Keeps recent files out of Start", false, 0,
        {dword(Hive::CurrentUser, kExplorerAdv, L"Start_TrackDocs", 0, 1)});

    add("typing_insights", ItemGroup::Privacy, IconKind::Keyboard, L"键入见解", L"Typing insights",
        L"停止收集你输入的统计信息", L"Stops collecting typing statistics", false, 0,
        {dword(Hive::CurrentUser, L"Software\\Microsoft\\Input\\TIPC", L"Enabled", 0, 1)});

    add("location", ItemGroup::Privacy, IconKind::Location, L"定位服务", L"Location service",
        L"拒绝应用获取你的位置（关闭后地图等功能受影响）", L"Denies location access to all apps", false, 0,
        {str(Hive::CurrentUser, L"Software\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\location",
             L"Value", L"Deny", L"Allow")});

    add("tailored_experiences", ItemGroup::Privacy, IconKind::Shield, L"个性化诊断体验", L"Tailored experiences",
        L"不让微软用诊断数据定制提示与广告", L"Stops using diagnostic data for tailored tips and ads", false, 0,
        {dword(Hive::CurrentUser, kPrivacy, L"TailoredExperiencesWithDiagnosticDataEnabled", 0, 1)});

    add("feedback", ItemGroup::Privacy, IconKind::Speech, L"反馈请求频率", L"Feedback requests",
        L"不再定期弹出 Windows 反馈邀请", L"Stops periodic feedback invitations", false, 0,
        {dword(Hive::CurrentUser, L"Software\\Microsoft\\Siuf\\Rules", L"NumberOfSIUFInPeriod", 0, 1)});

    // ---- Diagnostics ------------------------------------------------------
    add("telemetry", ItemGroup::Diagnostics, IconKind::Chart, L"诊断数据（可选）", L"Optional diagnostics",
        L"把诊断数据限制在最低级别，需要管理员权限", L"Limits diagnostic data to the minimum (admin required)", true,
        0, {dword(Hive::LocalMachine, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", L"AllowTelemetry", 0, 1)});

    add("error_reporting", ItemGroup::Diagnostics, IconKind::Alert, L"错误报告", L"Error reporting",
        L"崩溃后不向微软发送报告", L"Stops sending crash reports to Microsoft", false, 0,
        {dword(Hive::CurrentUser, L"Software\\Microsoft\\Windows\\Windows Error Reporting", L"Disabled", 1, 0)});

    add("ceip", ItemGroup::Diagnostics, IconKind::Chart, L"客户体验改善计划", L"Customer experience program",
        L"退出微软的 CEIP 数据收集", L"Opts out of the CEIP program", false, 0,
        {dword(Hive::CurrentUser, L"Software\\Microsoft\\SQMClient\\Windows", L"CEIPEnable", 0, 1)});

    add("handwriting", ItemGroup::Diagnostics, IconKind::Keyboard, L"手写与输入数据", L"Inking & typing data",
        L"不上传手写与输入样本", L"Stops uploading inking and typing samples", false, 0,
        {dword(Hive::CurrentUser, L"Software\\Microsoft\\InputPersonalization", L"RestrictImplicitTextCollection", 1,
               0),
         dword(Hive::CurrentUser, L"Software\\Microsoft\\InputPersonalization", L"RestrictImplicitInkCollection", 1,
               0)});

    add("speech", ItemGroup::Diagnostics, IconKind::Speech, L"在线语音识别", L"Online speech recognition",
        L"不把语音样本发送到微软", L"Stops sending voice samples to Microsoft", false, 0,
        {dword(Hive::CurrentUser, L"Software\\Microsoft\\Speech_OneCore\\Settings\\OnlineSpeechPrivacy", L"HasAccepted",
               0, 1)});

    return list;
}

}  // namespace

const std::vector<ItemDef>& itemCatalog() {
    static const std::vector<ItemDef> catalog = buildCatalog();
    return catalog;
}

const wchar_t* groupTitle(ItemGroup g, bool chinese) {
    static const wchar_t* zh[] = {L"广告与推荐", L"搜索与开始菜单", L"活动与隐私", L"诊断与反馈"};
    static const wchar_t* en[] = {L"Ads & suggestions", L"Search & Start", L"Activity & privacy", L"Diagnostics"};
    int idx = (int)g;
    if (idx < 0 || idx >= (int)ItemGroup::Count) idx = 0;
    return chinese ? zh[idx] : en[idx];
}

ItemState readItemState(const ItemDef& def) {
    ItemState state;
    if (def.minBuild > 0 && windowsBuild() != 0 && windowsBuild() < def.minBuild) {
        state.available = false;
        return state;
    }
    bool allBlocked = true;
    for (const auto& op : def.ops) {
        bool blocked = false;
        if (op.isString) {
            std::wstring value;
            blocked = readRegString(op.addr, value) && _wcsicmp(value.c_str(), op.blockString) == 0;
        } else {
            DWORD value = 0;
            blocked = readRegDword(op.addr, value) && value == op.blockDword;
        }
        if (!blocked) allBlocked = false;
    }
    state.blocked = allBlocked;
    return state;
}

bool applyItem(const ItemDef& def, bool block, bool* accessDenied) {
    bool allOk = true;
    bool denied = false;
    for (const auto& op : def.ops) {
        bool ok;
        if (op.isString) {
            ok = writeRegString(op.addr, block ? op.blockString : op.openString);
        } else {
            ok = writeRegDword(op.addr, block ? op.blockDword : op.openDword);
        }
        if (!ok) {
            allOk = false;
            DWORD err = GetLastError();
            if (err == ERROR_ACCESS_DENIED || op.addr.hive == Hive::LocalMachine) denied = true;
        }
    }
    if (accessDenied) *accessDenied = denied;
    return allOk;
}

std::vector<RegAddress> itemAddresses(const ItemDef& def) {
    std::vector<RegAddress> out;
    for (const auto& op : def.ops) out.push_back(op.addr);
    return out;
}

}  // namespace wpg
