# CYDnote

CYDnote 是一个基于 `ESP32-2432S028R`（CYD）开发板的本地文本笔记与文件管理应用，使用 `LVGL + TFT_eSPI + XPT2046 + LittleFS + SdFat` 实现。

## 功能概览

- 单列文件管理器（非 LVGL 默认文件浏览器）
- 盘符侧栏：
  - `L:` 内部 `LittleFS`
  - `D:` 外部 `SD` 卡（可选）
- 面包屑导航、返回上级目录、菜单操作
- 文件操作：
  - `New`（文件/文件夹）
  - `Rename`
  - `Delete`（普通/强制递归）
  - `Copy/Paste`（重名自动 `name(1).ext`）
- 文本编辑器：
  - 文件名标题
  - 保存/返回
  - 拼音 IME（26 键）
- 复制进度：
  - 在 `Paste` 按钮内显示进度与 ETA
  - 支持 `Cancel`
- 文件系统信息显示（容量/占用）
- RGB 状态灯（可选）

## 硬件与引脚（当前项目配置）

### 显示（ILI9341）

- `TFT_MOSI = 13`
- `TFT_SCLK = 14`
- `TFT_CS = 15`
- `TFT_DC = 2`
- `TFT_RST = -1`
- `TFT_BL = 21`

### 触摸（XPT2046）

- `XPT2046_MOSI = 32`
- `XPT2046_MISO = 39`
- `XPT2046_CLK = 25`
- `XPT2046_CS = 33`

### SD 卡

- `SD_CS = 5`
- `SD_SCK = 14`
- `SD_MISO = 12`
- `SD_MOSI = 13`
- 默认频率：`25MHz`


## 软件架构

- `src/main.cpp`：硬件初始化、LVGL 驱动、主循环
- `src/app_manager.h`：页面切换与应用状态管理
- `src/ui/file_manager.h`：文件管理器 UI 与文件操作逻辑
- `src/ui/editor.h`：文本编辑器与 IME
- `src/utils/sd_helper.h`：SdFat 封装与 SD 信息读取
- `src/config.h`：项目硬件与功能宏配置

## 构建与烧录

### 依赖环境

- VSCode + PlatformIO
- 开发板：ESP32-2432S028R（4MB Flash）

### 常用命令（PlatformIO）

- 构建：`PlatformIO: Build`
- 烧录固件：`PlatformIO: Upload`
- 上传文件系统镜像（LittleFS）：`PlatformIO: Upload Filesystem Image`
- 串口监视器：`PlatformIO: Monitor`

## 文件系统说明

- 工程使用 `LittleFS`（非 SPIFFS）
- `data/` 目录用于生成并烧录文件系统镜像
- 运行时 `L:` 对应 LittleFS
- `D:` 则对应SD卡（如有）

## 已知限制

- 大字符集显示能力受当前编译字体集限制