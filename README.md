# Time Master

**Time Master** 是一套功能强大的个人时间管理与数据可视化工具集，旨在帮助您精确追踪、分析并优化您的时间利用。

本套件包含三个核心组件：

  * **`Time_Master` (C++)**: 核心命令行程序，负责解析原始日志、将数据存入数据库，并提供丰富的查询与报告导出功能。
  * **`graph_generator` (Python)**: 数据可视化工具，能够读取 `Time_Master` 生成的数据库，并创建多种图表，如每日时间线、活动热力图等。
  * **`log_generator` (C++)**: 一个便捷的测试数据生成器，用于快速创建符合格式的日志文件。

-----

## 🚀 快速开始

### 1\. 依赖项

在开始之前，请确保您的系统已安装以下依赖：

  * **C++ 部分 (`Time_Master`, `log_generator`)**:
      * **MSYS2 UCRT64** 环境 (推荐用于 Windows)
      * **CMake** \>= 3.10
      * **GCC** (支持 C++23)
      * **SQLite3** 库
      * **nlohmann/json** 库
  * **Python 部分 (`graph_generator`)**:
      * **Python** \>= 3.8
      * **Matplotlib** 库

### 2\. 编译与安装

我们为所有C++组件提供了详细的编译指南，包括如何配置MSYS2 UCRT64环境和安装必要的库。

➡️ **详细步骤请参考：[编译指南](https://www.google.com/search?q=./docs/COMPILING.md)** *(您之后需要创建这个文件)*

### 3\. 基本使用示例

以下是 `Time_Master` 命令行工具的一些常用命令，让您快速感受其功能。

**示例 1：完整处理数据**
(检验源文件 -\> 转换 -\> 检验输出 -\> 存入数据库)

```bash
# 假设您的原始日志放在 "raw_logs" 文件夹下
time_tracker_cli -a "path/to/your/raw_logs"
```

**示例 2：查询指定日期的报告**

```bash
# 查询 2025年7月21日 的日报，并以 Markdown 格式输出
time_tracker_cli -q d 20250721 -f md
```

**示例 3：查询上个月的报告**

```bash
# 查询 2025年6月 的月报，并以 LaTeX 格式输出
time_tracker_cli -q m 202506 -f tex
```

-----

## 📚 详细文档

想要深入了解本项目吗？

关于本项目的**详细架构图**、**完整的命令参考**、**API文档**和**各模块使用示例**，请查阅docs/
```
docs/
├── compilation_guide.md          # 详细的编译步骤 (从原1.4节迁移)
│
├── time_master/                  # Time_Master主程序的专属文档
│   ├── architecture.md           # 包含目录结构和架构图 (从原1.1, 1.2节迁移)
│   └── usage.md                  # 包含完整的命令行参数表格和说明 (从原1.3节迁移)
│
├── graph_generator/              # 图表生成器的专属文档
│   └── usage.md                  # 包含其结构、命令和使用示例 (从原第2大点迁移)
│
└── log_generator/                # 日志生成器的专属文档
    └── usage.md                  # 包含其结构、用法和示例 (从原第3大点迁移)
```

-----

## 致谢与许可证

本项目的实现离不开以下这些出色的开源库。我向这些项目的开发者们表示感谢！

  * **[SQLite C Library](https://www.sqlite.org/index.html)**: 用于数据存储 (Public Domain)。
  * **[nlohmann/json](https://github.com/nlohmann/json)**: 用于读取配置 (MIT License)。
  * **[Matplotlib](https://matplotlib.org/)**: 用于数据可视化 (BSD-style License)。