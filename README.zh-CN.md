# 使用说明与警示

英文版本：`README.md`

## 使用说明

1. 本项目使用 `PlatformIO` 构建，请先安装 VS Code 与 PlatformIO 插件。
2. 连接 `ESP32-2432S028` 开发板后，执行：
   - 编译：`pio run -e esp32-2432s028r`
   - 烧录：`pio run -e esp32-2432s028r -t upload`
   - 串口监视：`pio device monitor -b 115200`
3. 首次使用前可按需上传 `data/` 到 LittleFS：
   - `pio run -e esp32-2432s028r -t uploadfs`

## 警示

- 本项目**仅支持** `ESP32-2432S028/ESP32-2432S028R (CYD)` 硬件。
- 请勿直接用于屏幕分辨率、触摸芯片或引脚定义不同的板卡，否则可能出现黑屏、触摸失效或存储异常。

# CYDnote

CYDnote 是一个运行在 `ESP32-2432S028R` (CYD) 开发板上的本地文本笔记与文件管理器，基于 `LVGL + TFT_eSPI + XPT2046 + LittleFS + SdFat`。

## 功能

- 单栏文件管理器（自定义 UI）
- 盘符侧边栏：`L:` LittleFS，`D:` SD 卡（可选）
- 面包屑导航、返回上级、菜单操作
- 文件操作：新建（文件/文件夹）、重命名、删除（普通/强制递归）、复制/粘贴
- 文本编辑器：文件名标题、保存/返回、拼音输入法（9 键与 26 键）
- 粘贴进度、预计剩余时间与取消
- 文件系统信息（容量/已用）

## 引脚定义

```ini
# Display (ILI9341)
TFT_MOSI = 13
TFT_SCLK = 14
TFT_CS   = 15
TFT_DC   = 2
TFT_RST  = -1
TFT_BL   = 21

# Touch (XPT2046)
XPT2046_MOSI = 32
XPT2046_MISO = 39
XPT2046_CLK  = 25
XPT2046_CS   = 33

# SD card
SD_CS   = 5
SD_SCK  = 14
SD_MISO = 12
SD_MOSI = 13
SD_Freq = 25MHz
```

## 字体分区

字体来源：`src/font.c`，基于 `SourceHanSansCN-Regular`。

转换参数：

- 范围：`0x20-0x7F, 0x3000-0x303F, 0x3040-0x30FF, 0x4E00-0x9FAF, 0x0400-0x04FF`
- 字号：`14`
- 位深：`2-bit-per-pixel`
- 压缩：启用

按范围分区：

- CJK（主分区）：`0x4E00-0x9FAF`（20912）
- 日文假名：`0x3040-0x30FF`（192）
- 西里尔：`0x0400-0x04FF`（256，含扩展）

## 文件系统

- 使用 `LittleFS`（非 SPIFFS）
- `data/` 用于构建 LittleFS 镜像
- `L:` 映射 LittleFS，`D:` 映射 SD 卡（若存在）
