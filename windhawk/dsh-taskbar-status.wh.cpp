// ==WindhawkMod==
// @id              dsh-taskbar-status
// @name            DSH Taskbar Status
// @name:zh-CN      DSH 任务栏状态
// @description     Shows DSH and OpenCode session, task progress, activity, token and context status next to the Windows taskbar tray
// @description:zh-CN 在 Windows 任务栏通知区域左侧显示 DSH 与 OpenCode 的会话、任务进度、活动、Token 和上下文状态
// @version         0.1.0
// @author          1812z
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -DWIN32_LEAN_AND_MEAN -lws2_32 -lruntimeobject -lwindowsapp -luuid -luser32
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# DSH Taskbar Status

Displays status received from the `dsh-windhawk-status` DSH plugin in the empty
area immediately to the left of the taskbar notification area.

Derived from OpenCode Taskbar Status. The XAML taskbar injection, layouts,
formats, placeholders and the details flyout are unchanged; only the transport
defaults, the "open conversation" action and the idle timeout are new.

Features:

- Windows 10 and Windows 11 taskbars
- Primary or all monitor taskbars
- Single-line and two-line layouts
- Multiple concurrent sessions
- User-defined formats with placeholders
- Left click opens the details flyout, right click jumps to the conversation
- Automatic stale-instance removal
- Idle timeout: a session that stops working is removed from the taskbar
- Automatically collapses when no instance is connected

The DSH plugin sends newline-delimited JSON to `127.0.0.1:19290` by default.
Keep the port in both plugins identical. This mod also accepts OpenCode payloads,
so pointing the OpenCode plugin's `OPENCODE_STATUS_PORT` at the same port shows
both products in one row of widgets.

## Placeholders

`{app}`, `{name}`, `{status}`, `{status_text}`, `{activity}`, `{progress}`,
`{current}`, `{completed}`, `{total}`, `{task}`, `{tokens}`, `{input_tokens}`,
`{output_tokens}`, `{reasoning_tokens}`, `{cache_read}`, `{cache_write}`,
`{context_used}`, `{context_limit}`, `{context_percent}`, `{runtime}`, `{provider}`,
`{model}`, `{approval}`, `{indicator}`, `{pid}`, `{instance_id}`,
`{instance_count}`.

Placeholders whose values are empty can be wrapped in an optional block. The
whole block is omitted when one of its placeholders is empty:

`[{progress} ]{task}`

Examples:

- `{app} [{progress} ]{task}  {context_used}/{context_limit}`
- First line: `{name}  {status_text}`
- Second line: `[{progress} ]{task}  ctx {context_percent}`
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- port: 19290
  $name: TCP port
  $name:zh-CN: TCP 端口
  $description: Must match DSH_STATUS_PORT used by the DSH plugin. Set the OpenCode plugin's OPENCODE_STATUS_PORT to the same value to show both in one row.
  $description:zh-CN: 必须与 DSH 插件的 DSH_STATUS_PORT 一致。把 OpenCode 插件的 OPENCODE_STATUS_PORT 也设成同一个值，两者就显示在同一排里。
- controlPort: 19289
  $name: Control port
  $name:zh-CN: 控制端口
  $description: Where the plugin listens for focus requests. Must match DSH_STATUS_CONTROL_PORT. Used by "Open conversation".
  $description:zh-CN: 插件监听跳转请求的端口，必须与 DSH_STATUS_CONTROL_PORT 一致。「打开会话」用它。
- idleTimeoutMinutes: 10
  $name: Idle timeout in minutes
  $name:zh-CN: 无动作超时（分钟）
  $description: Removes a session from the taskbar once it has stopped working and stayed quiet this long. A running session is never removed by time. Set to 0 to disable.
  $description:zh-CN: 会话停止工作并静默这么久后，从任务栏移除。正在运行的会话不会因超时移除。填 0 关闭。
- layout: twoLines
  $name: Layout
  $name:zh-CN: 布局
  $options:
    - singleLine: Single line
    - twoLines: Two lines
  $options:zh-CN:
    - singleLine: 单行
    - twoLines: 双行
- displayLanguage: auto
  $name: Display language
  $name:zh-CN: 显示语言
  $description: Auto follows the Windows user interface language.
  $description:zh-CN: 自动模式跟随 Windows 用户界面语言。
  $options:
    - auto: Automatic
    - zh-CN: Simplified Chinese
    - en: English
  $options:zh-CN:
    - auto: 自动
    - zh-CN: 简体中文
    - en: English
- singleLineFormat: "[{progress} ] {status_text}"
  $name: Single-line format
  $name:zh-CN: 单行格式
- firstLineFormat: "[{task}] {name}"
  $name: First-line format
  $name:zh-CN: 第一行格式
- secondLineFormat: "[进度: {progress}  ] {status_text} [{runtime}]"
  $name: Second-line format
  $name:zh-CN: 第二行格式
- width: 230
  $name: Maximum width
  $name:zh-CN: 最大宽度
  $description: Width in logical pixels. Text is clipped with an ellipsis when needed.
  $description:zh-CN: 单位为逻辑像素，文字过长时使用省略号截断。
- lineSpacing: -6
  $name: Two-line spacing
  $name:zh-CN: 双行上下间距
  $description: Adjusts the distance between the first and second lines in logical pixels. Positive values move them apart; negative values move them closer.
  $description:zh-CN: 调节第一行与第二行之间的距离，单位为逻辑像素。正值增大间距，负值减小间距。
- fontSize: 11
  $name: Font size
  $name:zh-CN: 字体大小
- fontFamily: Microsoft YaHei UI
  $name: Font family
  $name:zh-CN: 字体
- textColor: auto
  $name: Text color
  $name:zh-CN: 文字颜色
  $description: Use auto or a hex RGB value such as FFFFFF.
  $description:zh-CN: 使用 auto 自动取色，或输入 FFFFFF 这样的十六进制 RGB 颜色。
- openConversationOnClick: true
  $name: Open conversation on click
  $name:zh-CN: 点击打开对应会话
  $description: Right-clicking a widget jumps straight to its conversation, and the details panel offers one button that does the same.
  $description:zh-CN: 右键小窗直接跳到对应会话，详情面板里也有一个作用相同的按钮。
- updateIntervalMilliseconds: 250
  $name: Position/update interval
  $name:zh-CN: 位置和显示刷新间隔
- debugLogging: false
  $name: Diagnostic logging
  $name:zh-CN: 诊断日志
  $description: Logs socket, parsing and rendering summaries to the Windhawk log.
  $description:zh-CN: 将 Socket、JSON 解析和渲染摘要输出到 Windhawk 日志。
*/
// ==/WindhawkModSettings==

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <unknwn.h>

#undef GetCurrentTime
#include <windhawk_utils.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cwctype>
#include <cwchar>
#include <iterator>
#include <functional>
#include <memory>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Controls::Primitives;
using namespace winrt::Windows::UI::Xaml::Input;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::UI::Xaml::Media::Animation;
using namespace winrt::Windows::UI::Xaml::Shapes;

namespace {

constexpr UINT kReloadSettingsMessage = WM_APP + 0x371;
constexpr UINT kWakeMessage = WM_APP + 0x372;
// Distinct from the original mod's widget name on purpose: both inject into the
// same taskbar XAML tree, and two widgets sharing a name can find each other.
constexpr wchar_t kWidgetName[] = L"DshStatusWidget";

enum class Layout { SingleLine, TwoLines };
enum class DisplayLanguage { SimplifiedChinese, English };
enum class IndicatorKind { None, Working, Approval, Error };

struct Settings {
    int port = 19290;
    int controlPort = 19289;
    int idleTimeoutMinutes = 10;
    Layout layout = Layout::TwoLines;
    DisplayLanguage displayLanguage = DisplayLanguage::SimplifiedChinese;
    std::wstring singleLineFormat;
    std::wstring firstLineFormat;
    std::wstring secondLineFormat;
    int width = 430;
    int lineSpacing = 0;
    int fontSize = 11;
    std::wstring fontFamily;
    std::wstring textColor;
    bool openConversationOnClick = true;
    int updateIntervalMilliseconds = 250;
    bool debugLogging = true;
};

struct InstanceStatus {
    std::wstring instanceId;
    std::wstring app;
    std::wstring source;
    bool openable = false;
    ULONGLONG lastActivityAt = 0;
    std::wstring name;
    std::wstring status;
    std::wstring statusText;
    std::wstring activity;
    std::wstring indicator;
    std::wstring approval;
    std::wstring progress;
    std::wstring task;
    std::wstring provider;
    std::wstring model;
    std::wstring pid;
    std::wstring directory;
    std::wstring parentPid;
    std::wstring terminalSession;
    HWND terminalWindow = nullptr;
    std::wstring current;
    std::wstring completed;
    std::wstring total;
    std::wstring tokens;
    std::wstring inputTokens;
    std::wstring outputTokens;
    std::wstring reasoningTokens;
    std::wstring cacheRead;
    std::wstring cacheWrite;
    std::wstring contextUsed;
    std::wstring contextLimit;
    std::wstring contextPercent;
    bool waitingForOutput = false;
    ULONGLONG runStartedAt = 0;
};

std::mutex g_settingsMutex;
Settings g_settings;
std::atomic<bool> g_unloading{false};
HANDLE g_stopEvent = nullptr;
HANDLE g_workerThread = nullptr;
DWORD g_workerThreadId = 0;
HWND g_taskbarWindow = nullptr;
std::mutex g_instancesMutex;
std::map<std::wstring, InstanceStatus> g_uiInstances;
std::atomic<bool> g_uiRefreshPending{false};
std::atomic<bool> g_runtimeRefreshPending{false};
std::atomic<bool> g_widgetHealthCheckPending{false};
std::atomic<bool> g_widgetInjected{false};
[[clang::no_destroy]] StackPanel g_widgetRoot = nullptr;
[[clang::no_destroy]] Grid g_injectionParent = nullptr;
[[clang::no_destroy]] ColumnDefinition g_injectedColumn = nullptr;
int64_t g_injectedColumnWidthCallback = 0;
[[clang::no_destroy]] Style g_widgetButtonStyle = nullptr;
[[clang::no_destroy]] std::map<std::wstring, TextBlock> g_primaryTextBlocks;
[[clang::no_destroy]] std::map<std::wstring, TextBlock> g_secondaryTextBlocks;
std::mutex g_hiddenInstancesMutex;
std::set<std::wstring> g_hiddenInstances;
// Instance ids whose idle timeout already fired, mapped to the lastActivityAt
// seen at that moment. The plugin keeps heartbeating, so without this memory the
// next message would put the widget straight back; a heartbeat repeats the same
// lastActivityAt and stays suppressed, while a newer value means the session
// went back to work and the widget is revived.
std::mutex g_idleExpiredMutex;
std::map<std::wstring, ULONGLONG> g_idleExpired;
[[clang::no_destroy]] Flyout g_detailsFlyout = nullptr;
std::wstring g_detailsInstanceId;
bool g_detailsFlyoutOpen = false;

void ScheduleWidgetRefresh();
void ScheduleWidgetHealthCheck();
bool IsLightTaskbarTheme();
winrt::Windows::UI::Color GetTextColor(const Settings& settings);
winrt::Windows::UI::Color IndicatorColor(IndicatorKind indicator);
std::wstring FormatRuntime(ULONGLONG runStartedAt);
SolidColorBrush MakeBrush(winrt::Windows::UI::Color color);

AcrylicBrush CreateDetailsAcrylicBrush(bool isLight) {
    AcrylicBrush brush;
    brush.BackgroundSource(AcrylicBackgroundSource::HostBackdrop);
    auto tint = isLight
                    ? winrt::Windows::UI::Color{0xFF, 0xF2, 0xF2, 0xF2}
                    : winrt::Windows::UI::Color{0xFF, 0x24, 0x24, 0x24};
    brush.TintColor(tint);
    brush.TintOpacity(isLight ? 0.0 : 0.5);
    brush.TintLuminosityOpacity(isLight ? 0.9 : 0.96);
    brush.FallbackColor(tint);
    return brush;
}

bool IsInstanceHidden(const std::wstring& instanceId) {
    std::lock_guard lock(g_hiddenInstancesMutex);
    return g_hiddenInstances.contains(instanceId);
}

using CTaskBand_GetTaskbarHost_t = void*(WINAPI*)(void*, void*);
using TaskbarHost_FrameHeight_t = int(WINAPI*)(void*);
using Std_Ref_Decref_t = void(WINAPI*)(void*);
using TrayUI_StartTaskbar_t = void(WINAPI*)(void*);
CTaskBand_GetTaskbarHost_t CTaskBand_GetTaskbarHost_Original = nullptr;
CTaskBand_GetTaskbarHost_t CSecondaryTaskBand_GetTaskbarHost_Original = nullptr;
TaskbarHost_FrameHeight_t TaskbarHost_FrameHeight_Original = nullptr;
Std_Ref_Decref_t Std_Ref_Decref_Original = nullptr;
TrayUI_StartTaskbar_t TrayUI_StartTaskbar_Original = nullptr;
void* CTaskBand_ITaskListWndSite_vftable = nullptr;
void* CSecondaryTaskBand_ITaskListWndSite_vftable = nullptr;

std::wstring GetStringSetting(PCWSTR name, PCWSTR fallback) {
    PCWSTR value = Wh_GetStringSetting(name);
    std::wstring result = value && *value ? value : fallback;
    Wh_FreeStringSetting(value);
    return result;
}

int ClampInt(int value, int minimum, int maximum) {
    return std::max(minimum, std::min(maximum, value));
}

void LoadSettings() {
    Settings settings;
    settings.port = ClampInt(Wh_GetIntSetting(L"port"), 1, 65535);
    settings.controlPort = ClampInt(Wh_GetIntSetting(L"controlPort"), 1, 65535);
    settings.idleTimeoutMinutes = ClampInt(Wh_GetIntSetting(L"idleTimeoutMinutes"), 0, 1440);

    auto layout = GetStringSetting(L"layout", L"twoLines");
    settings.layout = layout == L"singleLine" ? Layout::SingleLine : Layout::TwoLines;
    auto displayLanguage = GetStringSetting(L"displayLanguage", L"auto");
    if (displayLanguage == L"zh-CN") {
        settings.displayLanguage = DisplayLanguage::SimplifiedChinese;
    } else if (displayLanguage == L"en") {
        settings.displayLanguage = DisplayLanguage::English;
    } else {
        LANGID language = GetUserDefaultUILanguage();
        settings.displayLanguage = PRIMARYLANGID(language) == LANG_CHINESE
                                       ? DisplayLanguage::SimplifiedChinese
                                       : DisplayLanguage::English;
    }
    settings.singleLineFormat =
        GetStringSetting(L"singleLineFormat", L"[{progress} ] {status_text}");
    settings.firstLineFormat = GetStringSetting(L"firstLineFormat", L"[{task}] {name}");
    settings.secondLineFormat = GetStringSetting(
        L"secondLineFormat", L"[进度: {progress}  ] {status_text} [{runtime}]");
    settings.width = ClampInt(Wh_GetIntSetting(L"width"), 80, 1600);
    settings.lineSpacing = ClampInt(Wh_GetIntSetting(L"lineSpacing"), -20, 20);
    settings.fontSize = ClampInt(Wh_GetIntSetting(L"fontSize"), 7, 40);
    settings.fontFamily = GetStringSetting(L"fontFamily", L"Microsoft YaHei UI");
    settings.textColor = GetStringSetting(L"textColor", L"auto");
    settings.openConversationOnClick = Wh_GetIntSetting(L"openConversationOnClick") != 0;
    settings.updateIntervalMilliseconds =
        ClampInt(Wh_GetIntSetting(L"updateIntervalMilliseconds"), 50, 5000);
    settings.debugLogging = Wh_GetIntSetting(L"debugLogging") != 0;

    std::lock_guard lock(g_settingsMutex);
    g_settings = std::move(settings);
}

Settings CopySettings() {
    std::lock_guard lock(g_settingsMutex);
    return g_settings;
}

std::wstring Utf8ToWide(std::string_view input) {
    if (input.empty()) return {};
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                                     static_cast<int>(input.size()), nullptr, 0);
    if (length <= 0) return {};
    std::wstring output(length, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                        static_cast<int>(input.size()), output.data(), length);
    return output;
}

size_t SkipWhitespace(std::string_view json, size_t position) {
    while (position < json.size() &&
           (json[position] == ' ' || json[position] == '\t' || json[position] == '\r' ||
            json[position] == '\n')) {
        position++;
    }
    return position;
}

bool ParseJsonString(std::string_view json, size_t position, std::string* value,
                     size_t* endPosition) {
    position = SkipWhitespace(json, position);
    if (position >= json.size() || json[position] != '"') return false;
    position++;
    std::string output;
    while (position < json.size()) {
        char c = json[position++];
        if (c == '"') {
            *value = std::move(output);
            *endPosition = position;
            return true;
        }
        if (c != '\\') {
            output.push_back(c);
            continue;
        }
        if (position >= json.size()) return false;
        char escaped = json[position++];
        switch (escaped) {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u': {
                if (position + 4 > json.size()) return false;
                unsigned code = 0;
                for (int i = 0; i < 4; i++) {
                    char digit = json[position++];
                    code <<= 4;
                    if (digit >= '0' && digit <= '9') code += digit - '0';
                    else if (digit >= 'a' && digit <= 'f') code += digit - 'a' + 10;
                    else if (digit >= 'A' && digit <= 'F') code += digit - 'A' + 10;
                    else return false;
                }
                if (code <= 0x7F) {
                    output.push_back(static_cast<char>(code));
                } else if (code <= 0x7FF) {
                    output.push_back(static_cast<char>(0xC0 | (code >> 6)));
                    output.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                } else {
                    output.push_back(static_cast<char>(0xE0 | (code >> 12)));
                    output.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    output.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                break;
            }
            default: return false;
        }
    }
    return false;
}

bool FindJsonValue(std::string_view json, std::string_view key, size_t* valueStart) {
    std::string needle = "\"" + std::string(key) + "\"";
    size_t position = 0;
    while ((position = json.find(needle, position)) != std::string_view::npos) {
        size_t colon = SkipWhitespace(json, position + needle.size());
        if (colon < json.size() && json[colon] == ':') {
            *valueStart = SkipWhitespace(json, colon + 1);
            return true;
        }
        position += needle.size();
    }
    return false;
}

std::wstring JsonString(std::string_view json, std::string_view key) {
    size_t position;
    if (!FindJsonValue(json, key, &position)) return {};
    std::string value;
    size_t end;
    return ParseJsonString(json, position, &value, &end) ? Utf8ToWide(value) : std::wstring{};
}

double JsonNumber(std::string_view json, std::string_view key, double fallback = 0) {
    size_t position;
    if (!FindJsonValue(json, key, &position)) return fallback;
    size_t end = position;
    while (end < json.size() &&
           ((json[end] >= '0' && json[end] <= '9') || json[end] == '-' || json[end] == '+' ||
            json[end] == '.' || json[end] == 'e' || json[end] == 'E')) {
        end++;
    }
    if (end == position) return fallback;
    try {
        return std::stod(std::string(json.substr(position, end - position)));
    } catch (...) {
        return fallback;
    }
}

std::string_view JsonObject(std::string_view json, std::string_view key) {
    size_t position;
    if (!FindJsonValue(json, key, &position) || position >= json.size() || json[position] != '{') {
        return {};
    }
    size_t start = position;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (; position < json.size(); position++) {
        char c = json[position];
        if (inString) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') inString = true;
        else if (c == '{') depth++;
        else if (c == '}' && --depth == 0) return json.substr(start, position - start + 1);
    }
    return {};
}

std::wstring IntegerString(double value) {
    if (!std::isfinite(value)) return L"0";
    return std::to_wstring(static_cast<unsigned long long>(std::max(0.0, value)));
}

std::wstring CompactNumber(double value) {
    if (!std::isfinite(value) || value <= 0) return L"0";
    const wchar_t* suffix = L"";
    double shown = value;
    if (value >= 1000000) {
        shown = value / 1000000.0;
        suffix = L"M";
    } else if (value >= 1000) {
        shown = value / 1000.0;
        suffix = L"K";
    } else {
        return IntegerString(value);
    }
    wchar_t buffer[32];
    swprintf_s(buffer, shown >= 100 ? L"%.0f%s" : shown >= 10 ? L"%.1f%s" : L"%.2f%s",
               shown, suffix);
    std::wstring result = buffer;
    size_t suffixPosition = result.find(suffix);
    if (suffixPosition != std::wstring::npos) {
        while (suffixPosition > 0 && result[suffixPosition - 1] == L'0' &&
               result.find(L'.') != std::wstring::npos) {
            result.erase(suffixPosition - 1, 1);
            suffixPosition--;
        }
        if (suffixPosition > 0 && result[suffixPosition - 1] == L'.') result.erase(suffixPosition - 1, 1);
    }
    return result;
}

bool JsonBool(std::string_view json, std::string_view key, bool fallback = false) {
    size_t position;
    if (!FindJsonValue(json, key, &position)) return fallback;
    if (json.substr(position, 4) == "true") return true;
    if (json.substr(position, 5) == "false") return false;
    return fallback;
}

/**
 * The executables whose sessions this mod serves.
 *
 * A reported pid is only trusted while its image is one of these, so a pid
 * recycled by an unrelated process cannot keep a widget alive. The reference mod
 * hardcoded opencode.exe; against a DSH session that check is always false, so
 * the widget was created and erased again inside a single tick and never painted.
 */
bool IsTrackedAppImage(std::wstring_view fileName) {
    std::wstring name(fileName);
    return _wcsicmp(name.c_str(), L"opencode.exe") == 0 ||
           _wcsicmp(name.c_str(), L"DSH Desktop.exe") == 0;
}

bool IsProcessAlive(std::wstring_view pidText) {
    if (pidText.empty()) return true;
    std::wstring value(pidText);
    wchar_t* end = nullptr;
    unsigned long pid = wcstoul(value.c_str(), &end, 10);
    if (!pid || !end || *end != L'\0') return true;
    HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        // Access and transient query failures don't prove that the process
        // exited. ERROR_INVALID_PARAMETER is the reliable "PID not found"
        // result for a non-zero process ID.
        return GetLastError() != ERROR_INVALID_PARAMETER;
    }
    DWORD waitResult = WaitForSingleObject(process, 0);
    wchar_t imagePath[MAX_PATH];
    DWORD imagePathLength = ARRAYSIZE(imagePath);
    bool isTrackedApp = false;
    if (waitResult != WAIT_OBJECT_0 &&
        QueryFullProcessImageNameW(process, 0, imagePath, &imagePathLength)) {
        std::wstring_view path(imagePath, imagePathLength);
        size_t separator = path.find_last_of(L"\\/");
        std::wstring_view fileName = separator == std::wstring_view::npos
                                         ? path
                                         : path.substr(separator + 1);
        isTrackedApp = IsTrackedAppImage(fileName);
    }
    CloseHandle(process);
    return waitResult != WAIT_OBJECT_0 && isTrackedApp;
}

bool IsTerminalWindow(HWND window) {
    if (!window || !IsWindow(window)) return false;
    wchar_t className[128];
    if (!GetClassNameW(window, className, ARRAYSIZE(className))) return false;
    return _wcsicmp(className, L"CASCADIA_HOSTING_WINDOW_CLASS") == 0 ||
           _wcsicmp(className, L"ConsoleWindowClass") == 0;
}

HWND FindOnlyTerminalWindow() {
    struct Context {
        HWND window = nullptr;
        int count = 0;
    } context;
    EnumWindows([](HWND window, LPARAM parameter) -> BOOL {
        auto* context = reinterpret_cast<Context*>(parameter);
        if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr) return TRUE;
        if (IsTerminalWindow(window)) {
            context->window = window;
            context->count++;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context));
    return context.count == 1 ? context.window : nullptr;
}

void ActivateWindow(HWND window) {
    if (!window) return;
    if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
    DWORD foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD currentThread = GetCurrentThreadId();
    bool attached = foregroundThread && foregroundThread != currentThread &&
                    AttachThreadInput(currentThread, foregroundThread, TRUE);
    SetForegroundWindow(window);
    BringWindowToTop(window);
    if (attached) AttachThreadInput(currentThread, foregroundThread, FALSE);
}

std::string WideToUtf8(std::wstring_view input) {
    if (input.empty()) return {};
    int length = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                                     nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), result.data(),
                        length, nullptr, nullptr);
    return result;
}

/** Raises the DSH Desktop window so the navigation the plugin performs is visible. */
bool ActivateDshDesktop() {
    struct Search {
        HWND best = nullptr;
        long bestArea = 0;
    } search;
    EnumWindows(
        [](HWND window, LPARAM parameter) -> BOOL {
            auto* state = reinterpret_cast<Search*>(parameter);
            if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER)) return TRUE;
            if (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
            DWORD pid = 0;
            GetWindowThreadProcessId(window, &pid);
            if (!pid) return TRUE;
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!process) return TRUE;
            wchar_t path[MAX_PATH]{};
            DWORD size = ARRAYSIZE(path);
            BOOL resolved = QueryFullProcessImageNameW(process, 0, path, &size);
            CloseHandle(process);
            if (!resolved) return TRUE;
            if (!wcsstr(path, L"DSH Desktop.exe")) return TRUE;
            RECT rect{};
            if (!GetWindowRect(window, &rect)) return TRUE;
            long area = (rect.right - rect.left) * (rect.bottom - rect.top);
            if (area > state->bestArea) {
                state->bestArea = area;
                state->best = window;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    if (!search.best) return false;
    ActivateWindow(search.best);
    return true;
}

/** Sends one newline-terminated NDJSON line to the plugin's control port. */
bool SendControlMessage(const Settings& settings, const std::string& json) {
    SOCKET connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection == INVALID_SOCKET) return false;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<u_short>(settings.controlPort));
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    bool sent = connect(connection, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
    if (sent) {
        std::string line = json + "\n";
        sent = send(connection, line.c_str(), static_cast<int>(line.size()), 0) != SOCKET_ERROR;
    }
    closesocket(connection);
    return sent;
}

/**
 * Jumps to the conversation behind one widget.
 *
 * The mod cannot select a conversation by itself: it lives in explorer.exe and
 * has no access to the DSH page. So it raises the Desktop window and hands the
 * session id to the plugin, which answers by navigating the UI. Returns false
 * when the instance is not an openable DSH session, leaving the caller free to
 * fall back to the terminal window.
 */
bool OpenSessionForInstance(const InstanceStatus& status, const Settings& settings) {
    if (!status.openable || status.instanceId.empty()) return false;
    ActivateDshDesktop();
    std::string json =
        "{\"action\":\"focus\",\"sessionId\":\"" + WideToUtf8(status.instanceId) + "\"}";
    bool sent = SendControlMessage(settings, json);
    if (settings.debugLogging) {
        Wh_Log(L"Open conversation: session=%s controlPort=%d sent=%d",
               status.instanceId.c_str(), settings.controlPort, sent ? 1 : 0);
    }
    return true;
}

void HideInstanceFromWidget(const std::wstring& instanceId) {
    {
        std::lock_guard lock(g_hiddenInstancesMutex);
        g_hiddenInstances.insert(instanceId);
    }
    if (g_detailsFlyout) g_detailsFlyout.Hide();
    ScheduleWidgetRefresh();
}

Button MakeDetailActionButton(std::wstring_view glyph, std::wstring_view tip,
                              std::function<void()> action) {
    Button button;
    button.Width(32);
    button.Height(28);
    button.Padding({0, 0, 0, 0});
    button.Content(winrt::box_value(std::wstring(glyph)));
    button.FontFamily(FontFamily(L"Segoe Fluent Icons"));
    button.FontSize(13);
    ToolTipService::SetToolTip(button, winrt::box_value(std::wstring(tip)));
    button.Click([action](auto const&, auto const&) { action(); });
    return button;
}

void AddDetailRow(StackPanel const& panel, std::wstring_view label,
                  std::wstring_view value) {
    if (value.empty()) return;
    StackPanel row;
    row.Orientation(Orientation::Horizontal);
    row.Margin({0, 3, 0, 3});
    TextBlock labelBlock;
    labelBlock.Text(std::wstring(label) + L": ");
    labelBlock.Opacity(0.62);
    labelBlock.FontSize(12);
    labelBlock.Foreground(MakeBrush(GetTextColor(CopySettings())));
    TextBlock valueBlock;
    valueBlock.Text(std::wstring(value));
    valueBlock.FontSize(12);
    valueBlock.Foreground(MakeBrush(GetTextColor(CopySettings())));
    valueBlock.TextTrimming(TextTrimming::CharacterEllipsis);
    valueBlock.MaxWidth(285);
    row.Children().Append(labelBlock);
    row.Children().Append(valueBlock);
    panel.Children().Append(row);
}

void ShowInstanceDetails(Button const& source, const InstanceStatus& status,
                         const Settings& settings) {
    if (g_detailsFlyoutOpen && g_detailsFlyout) {
        g_detailsFlyout.Hide();
        return;
    }
    StackPanel content;
    content.Width(350);
    TextBlock title;
    title.Text(status.name.empty() ? (status.app.empty() ? L"DSH" : status.app) : status.name);
    title.FontSize(16);
    title.Foreground(MakeBrush(GetTextColor(settings)));
    content.Children().Append(title);
    AddDetailRow(content, L"状态", status.statusText);
    AddDetailRow(content, L"模型", status.provider.empty() ? status.model :
                 status.provider + L"/" + status.model);
    AddDetailRow(content, L"目录", status.directory);
    AddDetailRow(content, L"工具", status.activity);
    AddDetailRow(content, L"任务", status.task);
    AddDetailRow(content, L"进度", status.progress);
    AddDetailRow(content, L"Token", status.tokens);
    AddDetailRow(content, L"上下文", status.contextUsed.empty() ? L"" :
                 status.contextUsed + L"/" + status.contextLimit);
    AddDetailRow(content, L"运行时间", FormatRuntime(status.runStartedAt));
    AddDetailRow(content, L"PID", status.pid);
    StackPanel actions;
    actions.Orientation(Orientation::Horizontal);
    actions.HorizontalAlignment(HorizontalAlignment::Right);
    actions.Margin({0, 8, 0, 0});
    if (status.openable && settings.openConversationOnClick) {
        auto sessionButton = MakeDetailActionButton(L"\uE8AF", L"打开会话",
            [status, settings] { OpenSessionForInstance(status, settings); });
        sessionButton.Margin({0, 0, 6, 0});
        actions.Children().Append(sessionButton);
    }
    actions.Children().Append(MakeDetailActionButton(L"\uE74D", L"隐藏此实例",
        [id = status.instanceId] { HideInstanceFromWidget(id); }));
    content.Children().Append(actions);
    Border background;
    background.CornerRadius({10, 10, 10, 10});
    background.Padding({16, 14, 12, 10});
    background.Background(CreateDetailsAcrylicBrush(IsLightTaskbarTheme()));
    background.Child(content);
    Flyout flyout;
    flyout.Content(background);
    flyout.ShouldConstrainToRootBounds(false);
    static constexpr wchar_t presenterXaml[] = LR"(
<Style TargetType="FlyoutPresenter"
 xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation">
 <Setter Property="Background" Value="Transparent"/>
 <Setter Property="Margin" Value="0"/>
 <Setter Property="Padding" Value="0"/>
</Style>)";
    try {
        flyout.FlyoutPresenterStyle(
            winrt::Windows::UI::Xaml::Markup::XamlReader::Load(presenterXaml).as<Style>());
    } catch (...) {
    }
    flyout.Placement(FlyoutPlacementMode::Top);
    flyout.Opened([](auto const&, auto const&) { g_detailsFlyoutOpen = true; });
    flyout.Closed([](auto const&, auto const&) {
        g_detailsFlyoutOpen = false;
        g_detailsFlyout = nullptr;
        g_detailsInstanceId.clear();
    });
    g_detailsFlyout = flyout;
    g_detailsInstanceId = status.instanceId;
    flyout.ShowAt(source);
}

ULONGLONG UnixTimeMilliseconds() {
    return static_cast<ULONGLONG>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

std::wstring FormatRuntime(ULONGLONG runStartedAt) {
    if (!runStartedAt) return {};
    ULONGLONG now = UnixTimeMilliseconds();
    if (now <= runStartedAt) return {};
    ULONGLONG elapsedSeconds = (now - runStartedAt) / 1000;
    if (!elapsedSeconds) return {};
    ULONGLONG hours = elapsedSeconds / 3600;
    ULONGLONG minutes = (elapsedSeconds % 3600) / 60;
    ULONGLONG seconds = elapsedSeconds % 60;
    std::wstring result;
    if (hours) result += std::to_wstring(hours) + L"h";
    if (minutes) result += std::to_wstring(minutes) + L"m";
    if (seconds) result += std::to_wstring(seconds) + L"s";
    return result;
}

std::wstring LocalizeApproval(std::wstring_view approval, DisplayLanguage language) {
    if (approval.empty()) return {};
    if (language == DisplayLanguage::SimplifiedChinese) {
        if (approval == L"approval") return L"等待审批";
        if (approval == L"question") return L"等待回答";
    } else {
        if (approval == L"approval") return L"Waiting for approval";
        if (approval == L"question") return L"Waiting for answer";
    }
    return std::wstring(approval);
}

bool ParseStatusMessage(std::string_view json, InstanceStatus* result, bool* remove) {
    if (JsonString(json, "protocol") != L"opencode-status") return false;
    result->instanceId = JsonString(json, "instanceId");
    if (result->instanceId.empty()) return false;
    *remove = JsonString(json, "type") == L"remove";
    if (*remove) return true;

    // pid is a top-level field and must be parsed even when session is null.
    // Previously a never-used OpenCode instance returned before this point,
    // leaving pid empty and making its taskbar record impossible to clean up.
    result->app = JsonString(json, "app");
    result->source = JsonString(json, "source");
    result->openable = JsonBool(json, "openable");
    result->pid = IntegerString(JsonNumber(json, "pid"));
    result->parentPid = IntegerString(JsonNumber(json, "parentPid"));
    result->terminalSession = JsonString(json, "terminalSession");

    auto session = JsonObject(json, "session");
    if (session.empty()) {
        *remove = true;
        return true;
    }
    auto progress = JsonObject(session, "progress");
    auto model = JsonObject(session, "model");
    auto tokens = JsonObject(session, "tokens");

    result->name = JsonString(session, "name");
    result->status = JsonString(session, "status");
    result->statusText = JsonString(session, "statusText");
    result->activity = JsonString(session, "activity");
    result->indicator = JsonString(session, "indicator");
    result->approval = JsonString(session, "approval");
    result->waitingForOutput = JsonBool(session, "waitingForOutput");
    result->progress = JsonString(progress, "label");
    result->task = JsonString(progress, "task");
    result->provider = JsonString(model, "providerId");
    result->model = JsonString(model, "modelId");
    result->current = IntegerString(JsonNumber(progress, "current"));
    result->completed = IntegerString(JsonNumber(progress, "completed"));
    result->total = IntegerString(JsonNumber(progress, "total"));
    result->tokens = CompactNumber(JsonNumber(tokens, "total"));
    result->inputTokens = CompactNumber(JsonNumber(tokens, "input"));
    result->outputTokens = CompactNumber(JsonNumber(tokens, "output"));
    result->reasoningTokens = CompactNumber(JsonNumber(tokens, "reasoning"));
    result->cacheRead = CompactNumber(JsonNumber(tokens, "cacheRead"));
    result->cacheWrite = CompactNumber(JsonNumber(tokens, "cacheWrite"));
    result->contextUsed = CompactNumber(JsonNumber(tokens, "contextUsed"));
    double contextLimit = JsonNumber(tokens, "contextLimit");
    result->contextLimit = contextLimit > 0 ? CompactNumber(contextLimit) : L"?";
    double percent = JsonNumber(tokens, "contextPercent");
    wchar_t percentBuffer[32];
    swprintf_s(percentBuffer, L"%.1f%%", std::max(0.0, percent));
    result->contextPercent = percentBuffer;
    result->runStartedAt = static_cast<ULONGLONG>(
        std::max(0.0, JsonNumber(session, "runStartedAt")));

    // The DSH plugin reports lastActivityAt as wall-clock epoch milliseconds.
    // Rebase it onto the tick clock so the idle test can compare it against
    // GetTickCount64(). A payload without the field (OpenCode) counts its own
    // arrival as the last action, which keeps its existing removal behaviour.
    double lastActivity = JsonNumber(session, "lastActivityAt");
    ULONGLONG ticks = GetTickCount64();
    if (lastActivity > 0) {
        ULONGLONG epoch = static_cast<ULONGLONG>(lastActivity);
        ULONGLONG wall = UnixTimeMilliseconds();
        ULONGLONG age = wall > epoch ? wall - epoch : 0;
        result->lastActivityAt = ticks > age ? ticks - age : 0;
    } else {
        result->lastActivityAt = ticks;
    }
    return true;
}

std::wstring PlaceholderValue(std::wstring_view name, const InstanceStatus& status,
                              size_t instanceCount, DisplayLanguage language) {
    if (name == L"app") return status.app.empty() ? L"OpenCode" : status.app;
    if (name == L"name") return status.name;
    if (name == L"status") return status.status;
    // These three arrive display-ready: the plugin attaches the wording DSH
    // itself is showing, in DSH's own language, so the mod holds no tool table.
    if (name == L"status_text") return status.statusText;
    if (name == L"activity") return status.activity;
    if (name == L"approval") return LocalizeApproval(status.approval, language);
    if (name == L"indicator") return status.indicator;
    if (name == L"progress") return status.progress;
    if (name == L"current") return status.current;
    if (name == L"completed") return status.completed;
    if (name == L"total") return status.total;
    if (name == L"task") return status.task;
    if (name == L"tokens") return status.tokens;
    if (name == L"input_tokens") return status.inputTokens;
    if (name == L"output_tokens") return status.outputTokens;
    if (name == L"reasoning_tokens") return status.reasoningTokens;
    if (name == L"cache_read") return status.cacheRead;
    if (name == L"cache_write") return status.cacheWrite;
    if (name == L"context_used") return status.contextUsed;
    if (name == L"context_limit") return status.contextLimit;
    if (name == L"context_percent") return status.contextPercent;
    if (name == L"runtime") return FormatRuntime(status.runStartedAt);
    if (name == L"provider") return status.provider;
    if (name == L"model") return status.model;
    if (name == L"pid") return status.pid;
    if (name == L"instance_id") return status.instanceId;
    if (name == L"instance_count") return std::to_wstring(instanceCount);
    return L"{" + std::wstring(name) + L"}";
}

std::wstring ExpandPlaceholders(std::wstring_view format, const InstanceStatus& status,
                                size_t instanceCount, DisplayLanguage language,
                                bool* missingValue = nullptr) {
    std::wstring output;
    bool missing = false;
    for (size_t i = 0; i < format.size();) {
        if (format[i] != L'{') {
            output.push_back(format[i++]);
            continue;
        }
        size_t end = format.find(L'}', i + 1);
        if (end == std::wstring_view::npos) {
            output.append(format.substr(i));
            break;
        }
        auto value = PlaceholderValue(format.substr(i + 1, end - i - 1), status,
                                      instanceCount, language);
        if (value.empty()) missing = true;
        output += value;
        i = end + 1;
    }
    if (missingValue) *missingValue = missing;
    return output;
}

std::wstring FormatInstance(std::wstring_view format, const InstanceStatus& status,
                            size_t instanceCount, DisplayLanguage language) {
    std::wstring output;
    for (size_t i = 0; i < format.size();) {
        if (format[i] != L'[') {
            size_t next = format.find(L'[', i);
            auto plain = format.substr(i, next == std::wstring_view::npos ? format.size() - i : next - i);
            output += ExpandPlaceholders(plain, status, instanceCount, language);
            if (next == std::wstring_view::npos) break;
            i = next;
            continue;
        }
        size_t end = format.find(L']', i + 1);
        if (end == std::wstring_view::npos) {
            output += ExpandPlaceholders(format.substr(i), status, instanceCount, language);
            break;
        }
        bool missing = false;
        auto optional = ExpandPlaceholders(format.substr(i + 1, end - i - 1), status,
                                           instanceCount, language, &missing);
        if (!missing) output += optional;
        i = end + 1;
    }
    while (!output.empty() && iswspace(output.back())) output.pop_back();
    return output;
}

std::wstring BuildUiSignature(const std::map<std::wstring, InstanceStatus>& instances,
                              const Settings& settings) {
    std::wstring signature = std::to_wstring(static_cast<int>(settings.layout)) + L"|" +
                             settings.singleLineFormat + L"|" + settings.firstLineFormat +
                             L"|" + settings.secondLineFormat + L"|" +
                             std::to_wstring(settings.fontSize) + L"|" +
                             settings.fontFamily + L"|" + std::to_wstring(settings.lineSpacing) +
                             L"|" + std::to_wstring(settings.openConversationOnClick);
    for (const auto& [id, status] : instances) {
        signature += L"\n" + id + L"|" + status.app + L"|" + status.name + L"|" + status.status + L"|" +
                      status.statusText + L"|" + status.activity + L"|" + status.indicator +
                      L"|" + status.approval + L"|" + status.progress + L"|" + status.task +
                     L"|" + status.provider + L"|" + status.model + L"|" + status.pid + L"|" +
                      status.current + L"|" + status.completed + L"|" + status.total + L"|" +
                      status.parentPid + L"|" + status.terminalSession + L"|" +
                     status.tokens + L"|" + status.inputTokens + L"|" + status.outputTokens +
                     L"|" + status.reasoningTokens + L"|" + status.cacheRead + L"|" +
                      status.cacheWrite + L"|" + status.contextUsed + L"|" +
                       status.contextLimit + L"|" + status.contextPercent + L"|" +
                       std::to_wstring(status.waitingForOutput) + L"|" +
                       std::to_wstring(status.runStartedAt);
    }
    return signature;
}

IndicatorKind GetIndicatorKind(const std::map<std::wstring, InstanceStatus>& instances) {
    IndicatorKind result = IndicatorKind::None;
    for (const auto& [id, status] : instances) {
        if (status.indicator == L"error") return IndicatorKind::Error;
        if (status.indicator == L"approval") result = IndicatorKind::Approval;
        else if ((status.waitingForOutput || status.indicator == L"working") &&
                 result == IndicatorKind::None) {
            result = IndicatorKind::Working;
        }
    }
    return result;
}

HWND FindTrayWindow(HWND taskbar) {
    return FindWindowExW(taskbar, nullptr, L"TrayNotifyWnd", nullptr);
}

using WindowThreadProc = void (*)(void*);

bool RunFromWindowThread(HWND window, WindowThreadProc proc, void* parameter) {
    static const UINT message =
        RegisterWindowMessageW(L"Windhawk_RunFromWindowThread_" WH_MOD_ID);
    struct Payload {
        WindowThreadProc proc;
        void* parameter;
    };
    DWORD threadId = GetWindowThreadProcessId(window, nullptr);
    if (!threadId) return false;
    if (threadId == GetCurrentThreadId()) {
        proc(parameter);
        return true;
    }
    HHOOK hook = SetWindowsHookExW(
        WH_CALLWNDPROC,
        [](int code, WPARAM wParam, LPARAM lParam) CALLBACK -> LRESULT {
            if (code == HC_ACTION) {
                const auto* call = reinterpret_cast<const CWPSTRUCT*>(lParam);
                static const UINT callbackMessage =
                    RegisterWindowMessageW(L"Windhawk_RunFromWindowThread_" WH_MOD_ID);
                if (call->message == callbackMessage) {
                    auto* payload = reinterpret_cast<Payload*>(call->lParam);
                    payload->proc(payload->parameter);
                }
            }
            return CallNextHookEx(nullptr, code, wParam, lParam);
        },
        nullptr, threadId);
    if (!hook) return false;
    Payload payload{proc, parameter};
    SendMessageW(window, message, 0, reinterpret_cast<LPARAM>(&payload));
    UnhookWindowsHookEx(hook);
    return true;
}

HWND FindPrimaryTaskbarWindow() {
    HWND result = nullptr;
    EnumWindows(
        [](HWND window, LPARAM parameter) CALLBACK -> BOOL {
            DWORD processId = 0;
            wchar_t className[32]{};
            if (GetWindowThreadProcessId(window, &processId) &&
                processId == GetCurrentProcessId() &&
                GetClassNameW(window, className, ARRAYSIZE(className)) &&
                _wcsicmp(className, L"Shell_TrayWnd") == 0) {
                *reinterpret_cast<HWND*>(parameter) = window;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&result));
    return result;
}

bool IsReadableMemoryRange(const void* address, size_t size) {
    if (!address || !size) return false;
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(address, &memory, sizeof(memory)) || memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
        return false;
    }
    uintptr_t start = reinterpret_cast<uintptr_t>(address);
    uintptr_t regionStart = reinterpret_cast<uintptr_t>(memory.BaseAddress);
    uintptr_t regionEnd = regionStart + memory.RegionSize;
    return start >= regionStart && start <= regionEnd && size <= regionEnd - start;
}

XamlRoot GetTaskbarXamlRoot(HWND taskbarWindow) {
    wchar_t className[64]{};
    GetClassNameW(taskbarWindow, className, ARRAYSIZE(className));
    bool secondary = _wcsicmp(className, L"Shell_SecondaryTrayWnd") == 0;
    HWND taskBandWindow = secondary
                              ? FindWindowExW(taskbarWindow, nullptr, L"WorkerW", nullptr)
                              : reinterpret_cast<HWND>(GetPropW(taskbarWindow, L"TaskbandHWND"));
    if (!taskBandWindow) return nullptr;
    void* taskBand = reinterpret_cast<void*>(GetWindowLongPtrW(taskBandWindow, 0));
    void* expectedVtable = secondary ? CSecondaryTaskBand_ITaskListWndSite_vftable
                                     : CTaskBand_ITaskListWndSite_vftable;
    auto getTaskbarHost = secondary ? CSecondaryTaskBand_GetTaskbarHost_Original
                                    : CTaskBand_GetTaskbarHost_Original;
    if (!taskBand || !expectedVtable || !getTaskbarHost || !TaskbarHost_FrameHeight_Original) {
        return nullptr;
    }
    void* site = taskBand;
    for (int i = 0; i <= 20; i++) {
        if (!IsReadableMemoryRange(site, sizeof(void*))) return nullptr;
        if (*reinterpret_cast<void**>(site) == expectedVtable) break;
        if (i == 20) return nullptr;
        site = reinterpret_cast<void**>(site) + 1;
    }
    void* taskbarHost[2]{};
    getTaskbarHost(site, taskbarHost);
    if (!taskbarHost[0]) {
        if (taskbarHost[1] && Std_Ref_Decref_Original) Std_Ref_Decref_Original(taskbarHost[1]);
        return nullptr;
    }
    size_t elementOffset = 0;
#if defined(_M_X64) || defined(__x86_64__)
    const BYTE* code = reinterpret_cast<const BYTE*>(TaskbarHost_FrameHeight_Original);
    if (IsReadableMemoryRange(code, 8) && code[0] == 0x48 && code[1] == 0x83 &&
        code[2] == 0xEC && code[4] == 0x48 && code[5] == 0x83 && code[6] == 0xC1 &&
        code[7] <= 0x7F) {
        elementOffset = code[7];
    } else {
        if (taskbarHost[1] && Std_Ref_Decref_Original) Std_Ref_Decref_Original(taskbarHost[1]);
        return nullptr;
    }
#else
    elementOffset = 0x10;
#endif
    auto elementAddress = static_cast<BYTE*>(taskbarHost[0]) + elementOffset;
    if (!IsReadableMemoryRange(elementAddress, sizeof(IUnknown*))) {
        if (taskbarHost[1] && Std_Ref_Decref_Original) Std_Ref_Decref_Original(taskbarHost[1]);
        return nullptr;
    }
    IUnknown* elementUnknown = *reinterpret_cast<IUnknown**>(elementAddress);
    FrameworkElement element{nullptr};
    HRESULT result = elementUnknown
                         ? elementUnknown->QueryInterface(winrt::guid_of<FrameworkElement>(),
                                                          winrt::put_abi(element))
                         : E_NOINTERFACE;
    auto xamlRoot = SUCCEEDED(result) && element ? element.XamlRoot() : nullptr;
    if (taskbarHost[1] && Std_Ref_Decref_Original) Std_Ref_Decref_Original(taskbarHost[1]);
    return xamlRoot;
}

FrameworkElement FindChildByName(FrameworkElement const& parent, std::wstring_view name,
                                 int depth = 32) {
    if (!parent || depth <= 0) return nullptr;
    int count = VisualTreeHelper::GetChildrenCount(parent);
    for (int i = 0; i < count; i++) {
        auto child = VisualTreeHelper::GetChild(parent, i).try_as<FrameworkElement>();
        if (!child) continue;
        if (child.Name() == name) return child;
        if (auto found = FindChildByName(child, name, depth - 1)) return found;
    }
    return nullptr;
}

SolidColorBrush MakeBrush(winrt::Windows::UI::Color color) {
    SolidColorBrush brush;
    brush.Color(color);
    return brush;
}

Style GetWidgetButtonStyle() {
    if (!g_widgetButtonStyle) {
        static constexpr wchar_t xaml[] = LR"(<Style TargetType="Button"
 xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
 xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
 <Setter Property="Background" Value="Transparent"/><Setter Property="BorderThickness" Value="0"/>
 <Setter Property="UseSystemFocusVisuals" Value="False"/><Setter Property="Template"><Setter.Value>
 <ControlTemplate TargetType="Button"><Border x:Name="Root" Background="{TemplateBinding Background}"
 CornerRadius="{TemplateBinding CornerRadius}" Padding="{TemplateBinding Padding}">
 <Border.BackgroundTransition><BrushTransition Duration="0:0:0.083"/></Border.BackgroundTransition>
 <ContentPresenter Content="{TemplateBinding Content}" HorizontalContentAlignment="Center"
 VerticalContentAlignment="Center"/></Border></ControlTemplate></Setter.Value></Setter></Style>)";
        try {
            g_widgetButtonStyle = winrt::Windows::UI::Xaml::Markup::XamlReader::Load(xaml).as<Style>();
        } catch (...) {
        }
    }
    return g_widgetButtonStyle;
}

winrt::Windows::UI::Color IndicatorColor(IndicatorKind indicator) {
    if (indicator == IndicatorKind::Approval) return {0xFF, 0xF5, 0xBE, 0x40};
    if (indicator == IndicatorKind::Error) return {0xFF, 0xEF, 0x53, 0x50};
    return {0xFF, 0x4A, 0xDE, 0x80};
}

winrt::Windows::UI::Color ParseTextColor(std::wstring_view value,
                                         winrt::Windows::UI::Color fallback) {
    if (value == L"auto" || value.size() != 6) return fallback;
    unsigned int rgb = 0;
    for (wchar_t character : value) {
        rgb <<= 4;
        if (character >= L'0' && character <= L'9') rgb |= character - L'0';
        else if (character >= L'A' && character <= L'F') rgb |= character - L'A' + 10;
        else if (character >= L'a' && character <= L'f') rgb |= character - L'a' + 10;
        else return fallback;
    }
    return {0xFF, static_cast<BYTE>((rgb >> 16) & 0xFF),
            static_cast<BYTE>((rgb >> 8) & 0xFF), static_cast<BYTE>(rgb & 0xFF)};
}

bool IsLightTaskbarTheme() {
    return g_widgetRoot && g_widgetRoot.ActualTheme() ==
                               winrt::Windows::UI::Xaml::ElementTheme::Light;
}

winrt::Windows::UI::Color GetTextColor(const Settings& settings) {
    if (settings.textColor != L"auto") {
        return ParseTextColor(settings.textColor, {0xFF, 0xFF, 0xFF, 0xFF});
    }
    if (IsLightTaskbarTheme()) {
        return {0xFF, 0x20, 0x20, 0x20};
    }
    return {0xFF, 0xFF, 0xFF, 0xFF};
}

Button BuildInstanceButton(const InstanceStatus& status, const Settings& settings,
                           size_t instanceCount) {
    Button button;
    button.Height(40);
    button.MaxWidth(static_cast<double>(settings.width));
    button.Padding({8, 0, 8, 0});
    button.CornerRadius({4, 4, 4, 4});
    button.VerticalAlignment(VerticalAlignment::Center);
    if (auto style = GetWidgetButtonStyle()) button.Style(style);

    Grid content;
    ColumnDefinition iconColumn;
    iconColumn.Width({28.0, GridUnitType::Pixel});
    ColumnDefinition textColumn;
    textColumn.Width({1.0, GridUnitType::Auto});
    content.ColumnDefinitions().Append(iconColumn);
    content.ColumnDefinitions().Append(textColumn);

    Grid iconArea;
    iconArea.Width(28);
    iconArea.Height(40);
    Grid::SetColumn(iconArea, 0);
    TextBlock glyph;
    glyph.Text(L"\uE756");
    glyph.FontFamily(FontFamily(L"Segoe Fluent Icons"));
    glyph.FontSize(16);
    glyph.HorizontalAlignment(HorizontalAlignment::Center);
    glyph.VerticalAlignment(VerticalAlignment::Center);
    auto textColor = GetTextColor(settings);
    glyph.Foreground(MakeBrush(textColor));
    iconArea.Children().Append(glyph);

    IndicatorKind indicator = IndicatorKind::None;
    if (status.indicator == L"error") indicator = IndicatorKind::Error;
    else if (status.indicator == L"approval") indicator = IndicatorKind::Approval;
    else if (status.waitingForOutput) {
        indicator = IndicatorKind::Working;
    }
    else if (status.indicator == L"working") indicator = IndicatorKind::Working;
    const auto primaryTextBrush = MakeBrush(
        indicator == IndicatorKind::Error
            ? winrt::Windows::UI::Color{0xFF, 0xE5, 0x73, 0x73}
            : textColor);
    if (indicator != IndicatorKind::None) {
        winrt::Windows::UI::Xaml::Shapes::Ellipse dot;
        dot.Width(7);
        dot.Height(7);
        dot.Fill(MakeBrush((status.waitingForOutput || status.statusText == L"等待中" ||
                            status.statusText == L"Waiting")
                               ? winrt::Windows::UI::Color{0xFF, 0xFF, 0x98, 0x00}
                               : IndicatorColor(indicator)));
        dot.HorizontalAlignment(HorizontalAlignment::Right);
        dot.VerticalAlignment(VerticalAlignment::Bottom);
        dot.Margin({0, 0, 1, 6});
        iconArea.Children().Append(dot);
        if (indicator == IndicatorKind::Working) {
            DoubleAnimation breathing;
            breathing.From(0.28);
            breathing.To(1.0);
            breathing.Duration(DurationHelper::FromTimeSpan(
                std::chrono::milliseconds(1600)));
            breathing.AutoReverse(true);
            breathing.RepeatBehavior(RepeatBehaviorHelper::Forever());
            Storyboard::SetTarget(breathing, dot);
            Storyboard::SetTargetProperty(breathing, L"Opacity");
            Storyboard storyboard;
            storyboard.Children().Append(breathing);
            storyboard.Begin();
        }
    }
    content.Children().Append(iconArea);

    FrameworkElement textContent{nullptr};
    if (settings.layout == Layout::SingleLine) {
        TextBlock line;
        line.Text(FormatInstance(settings.singleLineFormat, status, instanceCount,
                                 settings.displayLanguage));
        line.FontFamily(FontFamily(settings.fontFamily));
        line.FontSize(settings.fontSize);
        line.TextTrimming(TextTrimming::CharacterEllipsis);
        line.VerticalAlignment(VerticalAlignment::Center);
        line.Foreground(primaryTextBrush);
        g_primaryTextBlocks[status.instanceId] = line;
        textContent = line;
    } else {
        Grid lines;
        lines.Height(40);
        RowDefinition firstRow;
        firstRow.Height({1.0, GridUnitType::Star});
        RowDefinition secondRow;
        secondRow.Height({1.0, GridUnitType::Star});
        lines.RowDefinitions().Append(firstRow);
        lines.RowDefinitions().Append(secondRow);
        TextBlock firstLine;
        firstLine.Text(FormatInstance(settings.firstLineFormat, status, instanceCount,
                                      settings.displayLanguage));
        firstLine.FontFamily(FontFamily(settings.fontFamily));
        firstLine.FontSize(settings.fontSize);
        firstLine.TextTrimming(TextTrimming::CharacterEllipsis);
        firstLine.VerticalAlignment(VerticalAlignment::Center);
        firstLine.Foreground(primaryTextBrush);
        TextBlock secondLine;
        secondLine.Text(FormatInstance(settings.secondLineFormat, status, instanceCount,
                                       settings.displayLanguage));
        secondLine.FontFamily(FontFamily(settings.fontFamily));
        secondLine.FontSize(settings.fontSize);
        secondLine.TextTrimming(TextTrimming::CharacterEllipsis);
        secondLine.VerticalAlignment(VerticalAlignment::Center);
        secondLine.Foreground(MakeBrush(textColor));
        double estimatedLineHeight = settings.fontSize * 1.25;
        double minimumSpacing = -(20.0 - estimatedLineHeight);
        double spacing = std::max(static_cast<double>(settings.lineSpacing), minimumSpacing);
        TranslateTransform firstOffset;
        firstOffset.Y(-spacing / 2.0);
        firstLine.RenderTransform(firstOffset);
        TranslateTransform secondOffset;
        secondOffset.Y(spacing / 2.0);
        secondLine.RenderTransform(secondOffset);
        Grid::SetRow(firstLine, 0);
        Grid::SetRow(secondLine, 1);
        lines.Children().Append(firstLine);
        lines.Children().Append(secondLine);
        g_primaryTextBlocks[status.instanceId] = firstLine;
        g_secondaryTextBlocks[status.instanceId] = secondLine;
        textContent = lines;
    }
    Grid::SetColumn(textContent, 1);
    content.Children().Append(textContent);
    button.Content(content);

    std::wstring tooltip = status.name;
    if (!status.statusText.empty()) tooltip += L"\n" + status.statusText;
    if (!status.task.empty()) tooltip += L"\n" + status.task;
    if (!status.progress.empty()) tooltip += L"  " + status.progress;
    if (!status.contextUsed.empty() || !status.contextLimit.empty()) {
        tooltip += L"\nContext: " + status.contextUsed + L"/" + status.contextLimit;
    }
    ToolTipService::SetToolTip(button, winrt::box_value(tooltip));

    bool lightTheme = IsLightTaskbarTheme();
    auto normal = MakeBrush({0x00, 0xFF, 0xFF, 0xFF});
    auto hover = MakeBrush(lightTheme
                               ? winrt::Windows::UI::Color{0x99, 0xFF, 0xFF, 0xFF}
                               : winrt::Windows::UI::Color{0x0F, 0xFF, 0xFF, 0xFF});
    auto pressed = MakeBrush(lightTheme
                                 ? winrt::Windows::UI::Color{0x4D, 0xFF, 0xFF, 0xFF}
                                 : winrt::Windows::UI::Color{0x0A, 0xFF, 0xFF, 0xFF});
    button.Background(normal);
    button.PointerEntered([button, hover](auto const&, auto const&) { button.Background(hover); });
    button.PointerExited([button, normal](auto const&, auto const&) { button.Background(normal); });
    button.Click([button, status, settings](auto const&, auto const&) {
        ShowInstanceDetails(button, status, settings);
    });
    button.RightTapped([status, settings](auto const&, RightTappedRoutedEventArgs const& args) {
        args.Handled(true);
        // Right-click jumps straight to the conversation, skipping the flyout.
        if (settings.openConversationOnClick) OpenSessionForInstance(status, settings);
    });
    button.AddHandler(
        UIElement::PointerPressedEvent(),
        winrt::box_value(PointerEventHandler(
            [button, pressed](auto const&, PointerRoutedEventArgs const&) { button.Background(pressed); })),
        true);
    button.AddHandler(
        UIElement::PointerReleasedEvent(),
        winrt::box_value(PointerEventHandler(
            [button, hover](auto const&, PointerRoutedEventArgs const&) { button.Background(hover); })),
        true);
    return button;
}

void RefreshWidgetContents() {
    if (!g_widgetRoot) return;
    std::map<std::wstring, InstanceStatus> instances;
    {
        std::lock_guard lock(g_instancesMutex);
        instances = g_uiInstances;
    }
    Settings settings = CopySettings();
    g_primaryTextBlocks.clear();
    g_secondaryTextBlocks.clear();
    auto children = g_widgetRoot.Children();
    uint32_t index = 0;
    for (const auto& [id, status] : instances) {
        auto button = BuildInstanceButton(status, settings, instances.size());
        if (index < children.Size()) {
            children.SetAt(index, button);
        } else {
            children.Append(button);
        }
        ++index;
    }
    while (children.Size() > index) {
        children.RemoveAtEnd();
    }
    g_widgetRoot.Visibility(instances.empty() ? Visibility::Collapsed : Visibility::Visible);
    g_uiRefreshPending = false;
}

void RefreshRuntimeText() {
    std::map<std::wstring, InstanceStatus> instances;
    {
        std::lock_guard lock(g_instancesMutex);
        instances = g_uiInstances;
    }
    Settings settings = CopySettings();
    for (const auto& [id, status] : instances) {
        auto primary = g_primaryTextBlocks.find(id);
        if (primary == g_primaryTextBlocks.end()) continue;
        if (settings.layout == Layout::SingleLine) {
            primary->second.Text(FormatInstance(settings.singleLineFormat, status,
                                                instances.size(), settings.displayLanguage));
        } else {
            primary->second.Text(FormatInstance(settings.firstLineFormat, status,
                                                instances.size(), settings.displayLanguage));
            auto secondary = g_secondaryTextBlocks.find(id);
            if (secondary != g_secondaryTextBlocks.end()) {
                secondary->second.Text(FormatInstance(settings.secondLineFormat, status,
                                                      instances.size(), settings.displayLanguage));
            }
        }
    }
    g_runtimeRefreshPending = false;
}

int FindInjectedColumnIndex(Grid const& parent) {
    if (!parent || !g_injectedColumn) return -1;
    auto columns = parent.ColumnDefinitions();
    for (uint32_t i = 0; i < columns.Size(); i++) {
        if (columns.GetAt(i) == g_injectedColumn) return static_cast<int>(i);
    }
    return -1;
}

void RestoreInjectedColumnWidth() {
    if (!g_injectedColumn) return;
    auto width = g_injectedColumn.Width();
    if (width.GridUnitType != GridUnitType::Auto || width.Value != 1.0) {
        g_injectedColumn.Width({1.0, GridUnitType::Auto});
    }
}

void RemoveWidget() {
    try {
        if (g_injectedColumn && g_injectedColumnWidthCallback) {
            g_injectedColumn.UnregisterPropertyChangedCallback(
                ColumnDefinition::WidthProperty(), g_injectedColumnWidthCallback);
        }
        if (g_injectionParent) {
            int column = FindInjectedColumnIndex(g_injectionParent);
            auto children = g_injectionParent.Children();
            if (g_widgetRoot) {
                for (uint32_t i = 0; i < children.Size(); i++) {
                    if (children.GetAt(i) == g_widgetRoot) {
                        children.RemoveAt(i);
                        break;
                    }
                }
            }
            // Remove only the exact ColumnDefinition object created by this mod.
            // A cached numeric index can point at another mod's column after either
            // mod changes the shared taskbar grid.
            if (column >= 0) {
                for (uint32_t i = 0; i < children.Size(); i++) {
                    auto child = children.GetAt(i).try_as<FrameworkElement>();
                    if (!child) continue;
                    int childColumn = Grid::GetColumn(child);
                    int childSpan = Grid::GetColumnSpan(child);
                    if (childColumn > column) {
                        Grid::SetColumn(child, childColumn - 1);
                    } else if (childColumn < column && childColumn + childSpan > column) {
                        Grid::SetColumnSpan(child, childSpan - 1);
                    }
                }
                g_injectionParent.ColumnDefinitions().RemoveAt(column);
            }
        }
    } catch (...) {
    }
    g_injectedColumnWidthCallback = 0;
    g_injectedColumn = nullptr;
    g_widgetRoot = nullptr;
    g_injectionParent = nullptr;
    g_primaryTextBlocks.clear();
    g_secondaryTextBlocks.clear();
    g_widgetInjected = false;
}

bool InjectWidget() {
    HWND taskbar = g_taskbarWindow ? g_taskbarWindow : FindPrimaryTaskbarWindow();
    if (!taskbar) return false;
    g_taskbarWindow = taskbar;
    try {
        auto xamlRoot = GetTaskbarXamlRoot(taskbar);
        auto root = xamlRoot ? xamlRoot.Content().try_as<FrameworkElement>() : nullptr;
        auto trayElement = root ? FindChildByName(root, L"SystemTrayFrameGrid") : nullptr;
        auto trayGrid = trayElement ? trayElement.try_as<Grid>() : nullptr;
        if (!trayGrid) return false;
        RemoveWidget();

        constexpr int insertColumn = 0;
        ColumnDefinition column;
        column.Width({1.0, GridUnitType::Auto});
        trayGrid.ColumnDefinitions().InsertAt(insertColumn, column);
        g_injectionParent = trayGrid;
        g_injectedColumn = column;
        for (uint32_t i = 0; i < trayGrid.Children().Size(); i++) {
            auto child = trayGrid.Children().GetAt(i).try_as<FrameworkElement>();
            if (child && Grid::GetColumn(child) >= insertColumn) {
                Grid::SetColumn(child, Grid::GetColumn(child) + 1);
            }
        }

        StackPanel widget;
        widget.Name(kWidgetName);
        widget.Orientation(Orientation::Horizontal);
        widget.HorizontalAlignment(HorizontalAlignment::Left);
        widget.VerticalAlignment(VerticalAlignment::Stretch);
        widget.Margin({4, 0, 4, 0});
        Grid::SetColumn(widget, insertColumn);
        trayGrid.Children().Append(widget);
        g_widgetRoot = widget;
        g_injectedColumnWidthCallback = column.RegisterPropertyChangedCallback(
            ColumnDefinition::WidthProperty(),
            [](DependencyObject const& sender, DependencyProperty const&) {
                if (g_unloading || !g_injectedColumn || sender != g_injectedColumn) return;
                try {
                    RestoreInjectedColumnWidth();
                } catch (...) {
                }
            });
        g_widgetInjected = true;
        widget.ActualThemeChanged([](auto const&, auto const&) { RefreshWidgetContents(); });
        RefreshWidgetContents();
        return true;
    } catch (...) {
        RemoveWidget();
        return false;
    }
}

bool IsWidgetHealthy() {
    if (!g_widgetInjected || !g_widgetRoot || !g_injectionParent || !g_injectedColumn) {
        return false;
    }

    HWND taskbar = g_taskbarWindow ? g_taskbarWindow : FindPrimaryTaskbarWindow();
    if (!taskbar) return false;
    auto xamlRoot = GetTaskbarXamlRoot(taskbar);
    auto root = xamlRoot ? xamlRoot.Content().try_as<FrameworkElement>() : nullptr;
    auto trayElement = root ? FindChildByName(root, L"SystemTrayFrameGrid") : nullptr;
    auto liveTrayGrid = trayElement ? trayElement.try_as<Grid>() : nullptr;
    if (!liveTrayGrid || liveTrayGrid != g_injectionParent) return false;

    int column = FindInjectedColumnIndex(liveTrayGrid);
    if (column < 0 || Grid::GetColumn(g_widgetRoot) != column) return false;

    bool foundWidget = false;
    for (auto const& child : liveTrayGrid.Children()) {
        if (child == g_widgetRoot) {
            foundWidget = true;
            break;
        }
    }
    if (!foundWidget) return false;

    // Some taskbar mods retain a stale numeric column index and can write a
    // zero width into our column. Keep the property owned by this mod intact.
    RestoreInjectedColumnWidth();
    return true;
}

void ScheduleWidgetRefresh() {
    if (g_uiRefreshPending.exchange(true)) return;
    HWND taskbar = g_taskbarWindow ? g_taskbarWindow : FindPrimaryTaskbarWindow();
    if (!taskbar || !RunFromWindowThread(taskbar, [](void*) {
            bool healthy = false;
            try {
                healthy = IsWidgetHealthy();
            } catch (...) {
            }
            if (!healthy) InjectWidget();
            RefreshWidgetContents();
        }, nullptr)) {
        g_uiRefreshPending = false;
    }
}

void ScheduleWidgetHealthCheck() {
    if (g_widgetHealthCheckPending.exchange(true)) return;
    HWND taskbar = g_taskbarWindow ? g_taskbarWindow : FindPrimaryTaskbarWindow();
    if (!taskbar || !RunFromWindowThread(taskbar, [](void*) {
            try {
                if (!IsWidgetHealthy()) {
                    if (CopySettings().debugLogging) {
                        Wh_Log(L"Taskbar widget was detached or its column was removed; reinjecting");
                    }
                    InjectWidget();
                }
            } catch (...) {
                InjectWidget();
            }
            g_widgetHealthCheckPending = false;
        }, nullptr)) {
        g_widgetHealthCheckPending = false;
    }
}

void ScheduleRuntimeRefresh() {
    if (g_runtimeRefreshPending.exchange(true)) return;
    HWND taskbar = g_taskbarWindow ? g_taskbarWindow : FindPrimaryTaskbarWindow();
    if (!taskbar || !RunFromWindowThread(taskbar, [](void*) {
            if (g_widgetInjected) RefreshRuntimeText();
            else g_runtimeRefreshPending = false;
        }, nullptr)) {
        g_runtimeRefreshPending = false;
    }
}

SOCKET CreateListener(int port, bool debugLogging) {
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        Wh_Log(L"Socket creation failed: %d", WSAGetLastError());
        return INVALID_SOCKET;
    }
    BOOL exclusive = TRUE;
    setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
               reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
    u_long nonBlocking = 1;
    ioctlsocket(listener, FIONBIO, &nonBlocking);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<u_short>(port));
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        Wh_Log(L"Failed to listen on 127.0.0.1:%d: %d", port, WSAGetLastError());
        closesocket(listener);
        return INVALID_SOCKET;
    }
    if (debugLogging) {
        Wh_Log(L"Listening on 127.0.0.1:%d, explorer pid=%lu", port,
               GetCurrentProcessId());
    }
    return listener;
}

void ReceiveConnections(SOCKET listener, std::map<std::wstring, InstanceStatus>* instances,
                         const Settings& settings) {
    for (;;) {
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;
        u_long blocking = 0;
        ioctlsocket(client, FIONBIO, &blocking);
        DWORD timeout = 500;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
                   sizeof(timeout));
        std::string data;
        char buffer[4096];
        for (;;) {
            int received = recv(client, buffer, sizeof(buffer), 0);
            if (received <= 0) break;
            data.append(buffer, received);
            if (data.size() > 1024 * 1024) break;
        }
        closesocket(client);
        if (settings.debugLogging) {
            Wh_Log(L"Socket message received: %zu bytes", data.size());
        }

        size_t start = 0;
        while (start < data.size()) {
            size_t end = data.find('\n', start);
            if (end == std::string::npos) end = data.size();
            auto line = std::string_view(data).substr(start, end - start);
            if (!line.empty()) {
                InstanceStatus status;
                bool remove = false;
                if (ParseStatusMessage(line, &status, &remove)) {
                    if (settings.debugLogging) {
                        Wh_Log(
                            L"Parsed message: instance=%s remove=%d status=%s text=%s "
                            L"activity=%s indicator=%s approval=%s",
                            status.instanceId.c_str(), remove, status.status.c_str(),
                            status.statusText.c_str(), status.activity.c_str(),
                            status.indicator.c_str(), status.approval.c_str());
                    }
                    if (remove) {
                        instances->erase(status.instanceId);
                        {
                            std::lock_guard lock(g_hiddenInstancesMutex);
                            g_hiddenInstances.erase(status.instanceId);
                        }
                    } else {
                        // Remove records created by older plugin versions which
                        // monitored the terminal parent instead of opencode.exe.
                        if (status.instanceId.find(L":process:") != std::wstring::npos) {
                            for (auto old = instances->begin(); old != instances->end();) {
                                if (old->first.find(L":terminal:") != std::wstring::npos) {
                                    old = instances->erase(old);
                                } else {
                                    ++old;
                                }
                            }
                        }
                        auto existing = instances->find(status.instanceId);
                        if (IsInstanceHidden(status.instanceId)) {
                            continue;
                        }
                        if (existing != instances->end() &&
                            IsTerminalWindow(existing->second.terminalWindow)) {
                            status.terminalWindow = existing->second.terminalWindow;
                        } else {
                            HWND foreground = GetForegroundWindow();
                            if (IsTerminalWindow(foreground)) {
                                status.terminalWindow = foreground;
                            } else if (!status.terminalSession.empty()) {
                                status.terminalWindow = FindOnlyTerminalWindow();
                            }
                        }
                        (*instances)[status.instanceId] = std::move(status);
                    }
                } else if (settings.debugLogging) {
                    std::wstring preview = Utf8ToWide(line.substr(0, std::min<size_t>(line.size(), 180)));
                    Wh_Log(L"Rejected socket message: %s", preview.c_str());
                }
            }
            start = end + 1;
        }
    }
}

DWORD WINAPI WorkerThreadProc(void*) {
    g_workerThreadId = GetCurrentThreadId();

    std::map<std::wstring, InstanceStatus> instances;
    Settings settings = CopySettings();
    bool ownsTaskbar = FindPrimaryTaskbarWindow() != nullptr;
    SOCKET listener = ownsTaskbar
                          ? CreateListener(settings.port, settings.debugLogging)
                          : INVALID_SOCKET;
    ULONGLONG lastListenerAttempt = GetTickCount64();
    ULONGLONG lastRender = 0;
    ULONGLONG lastRuntimeRefresh = 0;
    ULONGLONG lastWidgetHealthCheck = 0;
    std::wstring lastRenderSummary;

    if (settings.debugLogging) {
        Wh_Log(L"Worker started: thread=%lu layout=%s ownsTaskbar=%d",
               GetCurrentThreadId(),
               settings.layout == Layout::SingleLine ? L"single" : L"two-lines",
               ownsTaskbar);
    }

    while (!g_unloading && WaitForSingleObject(g_stopEvent, 0) != WAIT_OBJECT_0) {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                g_unloading = true;
                break;
            }
            if (message.message == kReloadSettingsMessage) {
                Settings newSettings = CopySettings();
                if (newSettings.port != settings.port) {
                    if (listener != INVALID_SOCKET) closesocket(listener);
                    listener = ownsTaskbar
                                   ? CreateListener(newSettings.port, newSettings.debugLogging)
                                   : INVALID_SOCKET;
                    lastListenerAttempt = GetTickCount64();
                }
                settings = std::move(newSettings);
                if (settings.debugLogging) {
                    Wh_Log(L"Worker settings reloaded: port=%d", settings.port);
                }
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        ULONGLONG now = GetTickCount64();
        bool currentlyOwnsTaskbar = FindPrimaryTaskbarWindow() != nullptr;
        if (currentlyOwnsTaskbar != ownsTaskbar) {
            ownsTaskbar = currentlyOwnsTaskbar;
            if (!ownsTaskbar && listener != INVALID_SOCKET) {
                closesocket(listener);
                listener = INVALID_SOCKET;
            }
            if (settings.debugLogging) {
                Wh_Log(L"Taskbar ownership changed: ownsTaskbar=%d", ownsTaskbar);
            }
        }
        if (ownsTaskbar && listener == INVALID_SOCKET && now - lastListenerAttempt >= 1000) {
            listener = CreateListener(settings.port, settings.debugLogging);
            lastListenerAttempt = now;
        }
        if (listener != INVALID_SOCKET) {
            ReceiveConnections(listener, &instances, settings);
        }

        // ReceiveConnections may wait for a client for up to the socket timeout.
        // Do not compare the new messages against a timestamp captured before it.
        now = GetTickCount64();
        for (auto iterator = instances.begin(); iterator != instances.end();) {
            auto& status = iterator->second;
            bool processAlive = IsProcessAlive(status.pid);
            if (!processAlive) {
                // Closing a terminal window can terminate Node before the plugin
                // has a chance to send its asynchronous remove message. A dead
                // process is treated as a normal instance removal;
                // actual session/API failures arrive explicitly as error events.
                iterator = instances.erase(iterator);
                continue;
            }
            ++iterator;
        }

        // Idle timeout: a session that stopped working and stayed quiet for
        // settings.idleTimeoutMinutes leaves the taskbar, exactly as if its
        // process had exited. A session that is still working is never removed
        // by time alone, however long it runs. Turning the setting off clears
        // the suppression so every session comes back on the next message.
        if (settings.idleTimeoutMinutes > 0) {
            ULONGLONG idleMilliseconds =
                static_cast<ULONGLONG>(settings.idleTimeoutMinutes) * 60'000ULL;
            std::lock_guard lock(g_idleExpiredMutex);
            for (auto iterator = instances.begin(); iterator != instances.end();) {
                auto expired = g_idleExpired.find(iterator->first);
                if (expired != g_idleExpired.end()) {
                    if (iterator->second.lastActivityAt > expired->second) {
                        g_idleExpired.erase(expired);
                    } else {
                        iterator = instances.erase(iterator);
                        continue;
                    }
                }
                const auto& status = iterator->second;
                bool active = status.status == L"working" || status.status == L"waiting" ||
                              status.status == L"retrying" || status.status == L"compacting";
                bool quiet = !active && status.lastActivityAt != 0 &&
                             now - status.lastActivityAt >= idleMilliseconds;
                if (quiet) {
                    if (settings.debugLogging) {
                        Wh_Log(L"Idle timeout: session=%s quiet=%llu ms",
                               status.instanceId.c_str(), now - status.lastActivityAt);
                    }
                    g_idleExpired[iterator->first] = status.lastActivityAt;
                    iterator = instances.erase(iterator);
                    continue;
                }
                ++iterator;
            }
        } else {
            std::lock_guard lock(g_idleExpiredMutex);
            g_idleExpired.clear();
        }

        if (now - lastRender >= static_cast<ULONGLONG>(settings.updateIntervalMilliseconds)) {
            {
                std::lock_guard hiddenLock(g_hiddenInstancesMutex);
                for (auto hidden = instances.begin(); hidden != instances.end();) {
                    if (g_hiddenInstances.contains(hidden->first)) {
                        hidden = instances.erase(hidden);
                    } else {
                        ++hidden;
                    }
                }
            }
            IndicatorKind indicator = GetIndicatorKind(instances);
            std::wstring renderSummary = BuildUiSignature(instances, settings);
            if (renderSummary != lastRenderSummary) {
                if (settings.debugLogging) {
                    Wh_Log(L"Render update: instances=%zu indicator=%d",
                           instances.size(), static_cast<int>(indicator));
                }
                {
                    std::lock_guard lock(g_instancesMutex);
                    g_uiInstances = instances;
                }
                lastRenderSummary = std::move(renderSummary);
                ScheduleWidgetRefresh();
            }
            lastRender = now;
        }

        if (now - lastRuntimeRefresh >= 1000) {
            bool hasActiveRuntime = std::any_of(
                instances.begin(), instances.end(),
                [](const auto& item) { return item.second.runStartedAt != 0; });
            if (hasActiveRuntime) {
                {
                    std::lock_guard lock(g_instancesMutex);
                    auto visibleInstances = instances;
                    {
                        std::lock_guard hiddenLock(g_hiddenInstancesMutex);
                        for (auto hidden = visibleInstances.begin();
                             hidden != visibleInstances.end();) {
                            if (g_hiddenInstances.contains(hidden->first)) {
                                hidden = visibleInstances.erase(hidden);
                            } else {
                                ++hidden;
                            }
                        }
                    }
                    g_uiInstances = std::move(visibleInstances);
                }
                ScheduleRuntimeRefresh();
            }
            lastRuntimeRefresh = now;
        }

        // Taskbar mods share the same XAML grid. Revalidate our object identity
        // even while OpenCode is idle, when no status-driven repaint occurs.
        if (ownsTaskbar && now - lastWidgetHealthCheck >= 1000) {
            ScheduleWidgetHealthCheck();
            lastWidgetHealthCheck = now;
        }

        WaitForSingleObject(g_stopEvent, 50);
    }

    if (listener != INVALID_SOCKET) closesocket(listener);
    if (settings.debugLogging) Wh_Log(L"Worker stopping");
    g_workerThreadId = 0;
    return 0;
}

void WINAPI TrayUI_StartTaskbar_Hook(void* taskbarUi) {
    TrayUI_StartTaskbar_Original(taskbarUi);
    if (g_unloading) return;
    g_taskbarWindow = FindPrimaryTaskbarWindow();
    if (!g_taskbarWindow) return;
    RunFromWindowThread(g_taskbarWindow, [](void*) {
        RemoveWidget();
        InjectWidget();
    }, nullptr);
}

bool HookTaskbarDllSymbols() {
    HMODULE taskbar = LoadLibraryExW(L"taskbar.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!taskbar) return false;
    WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {{LR"(const CTaskBand::`vftable'{for `ITaskListWndSite'})"},
         &CTaskBand_ITaskListWndSite_vftable},
        {{LR"(const CSecondaryTaskBand::`vftable'{for `ITaskListWndSite'})"},
         &CSecondaryTaskBand_ITaskListWndSite_vftable},
        {{LR"(public: virtual class std::shared_ptr<class TaskbarHost> __cdecl CTaskBand::GetTaskbarHost(void)const )"},
         &CTaskBand_GetTaskbarHost_Original},
        {{LR"(public: virtual class std::shared_ptr<class TaskbarHost> __cdecl CSecondaryTaskBand::GetTaskbarHost(void)const )"},
         &CSecondaryTaskBand_GetTaskbarHost_Original},
        {{LR"(public: int __cdecl TaskbarHost::FrameHeight(void)const )"},
         &TaskbarHost_FrameHeight_Original},
        {{LR"(public: void __cdecl std::_Ref_count_base::_Decref(void))"},
         &Std_Ref_Decref_Original},
        {{LR"(public: virtual void __cdecl TrayUI::StartTaskbar(void))"},
         &TrayUI_StartTaskbar_Original, TrayUI_StartTaskbar_Hook},
    };
    return WindhawkUtils::HookSymbols(taskbar, hooks, ARRAYSIZE(hooks));
}

}  // namespace

BOOL Wh_ModInit() {
    Wh_Log(L"Init");
    LoadSettings();
    g_unloading = false;

    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        Wh_Log(L"WSAStartup failed");
        return FALSE;
    }
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        WSACleanup();
        return FALSE;
    }
    if (!HookTaskbarDllSymbols()) {
        Wh_Log(L"HookTaskbarDllSymbols failed");
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
        WSACleanup();
        return FALSE;
    }
    return TRUE;
}

void Wh_ModAfterInit() {
    g_taskbarWindow = FindPrimaryTaskbarWindow();
    if (g_taskbarWindow) {
        RunFromWindowThread(g_taskbarWindow, [](void*) { InjectWidget(); }, nullptr);
    }
    g_workerThread = CreateThread(nullptr, 0, WorkerThreadProc, nullptr, 0, nullptr);
    if (!g_workerThread) {
        Wh_Log(L"CreateThread failed: %lu", GetLastError());
        g_unloading = true;
    }
}

void Wh_ModUninit() {
    Wh_Log(L"Uninit");
    g_unloading = true;
    if (g_stopEvent) SetEvent(g_stopEvent);
    if (g_workerThreadId) PostThreadMessageW(g_workerThreadId, kWakeMessage, 0, 0);
    if (g_workerThread) {
        WaitForSingleObject(g_workerThread, INFINITE);
        CloseHandle(g_workerThread);
        g_workerThread = nullptr;
    }
    if (g_taskbarWindow) {
        RunFromWindowThread(g_taskbarWindow, [](void*) {
            RemoveWidget();
            g_widgetButtonStyle = nullptr;
        }, nullptr);
    }
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }
    WSACleanup();
}

void Wh_ModSettingsChanged() {
    Wh_Log(L"SettingsChanged");
    LoadSettings();
    if (g_taskbarWindow) {
        RunFromWindowThread(g_taskbarWindow, [](void*) {
            if (!IsWidgetHealthy()) InjectWidget();
            RefreshWidgetContents();
        }, nullptr);
    }
    if (g_workerThreadId) PostThreadMessageW(g_workerThreadId, kReloadSettingsMessage, 0, 0);
}
