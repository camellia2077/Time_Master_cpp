#ifndef VERSION_H
#define VERSION_H

#include <string_view>

// 使用命名空间来组织应用配置信息，避免全局污染
namespace AppInfo {
    // 使用 constexpr 和 string_view 可以在编译期确定字符串，效率更高
    // 这也是现代C++的推荐做法
    constexpr std::string_view VERSION = "0.3.7.5";
    constexpr std::string_view LAST_UPDATED = "2025-08-01";
}

#endif // VERSION_H