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

## 在线刷机网站

仓库内提供了一个最小可用的在线刷机页面：`webflash/index.html`。

它提供两个刷机模式：

- **保留数据更新**：写入 `latest` release 中的 `bootloader.bin`、`partitions.bin`、`firmware.bin`，不覆盖 LittleFS 数据分区
- **全量覆盖恢复**：额外写入 `latest` release 中的 `littlefs.bin` 到 `0x310000`，会覆盖设备上已有的本地数据

搭建步骤：

1. 确保 GitHub Actions 已经把构建产物发布到仓库的 `latest` release
2. 直接发布 `webflash/` 目录即可：
   - `webflash/index.html`
   - `webflash/manifest-preserve.json`
   - `webflash/manifest-full.json`
3. 用 GitHub Pages、Netlify、Vercel 或任意 HTTPS 静态托管发布该目录

如果你想直接把当前仓库连到 Netlify 一键部署，仓库根目录已经提供 `netlify.toml`：

- Netlify 直接发布 `webflash/` 目录
- 页面会自动读取仓库 `latest` release 里的固件文件
- 因此不需要再在 Netlify 中构建固件
- 页面还提供可选的 `ghproxy` 国内镜像开关，用于加速固件 bin 下载
- 页面现在还提供日志窗口、基于 Release 资产时间的固件构建日期显示，以及从已连接设备备份 LittleFS 分区为 `.bin` 文件的功能

注意：

- 浏览器需使用支持 Web Serial 的 Chrome / Edge
- “保留数据更新”只在分区布局保持不变时才适合保留现有数据
- 若 GitHub 官方下载较慢，可在页面里勾选 `ghproxy` 镜像加速固件下载
- `ghproxy` 仅代理固件下载 URL；release 元数据检查仍通过 GitHub API 完成
- 页面加载时会先检查 `latest` release 资产；若缺少 `littlefs.bin`，会自动禁用“全量覆盖恢复”
- 若 `latest` release 中缺少基础固件文件（`bootloader.bin`、`partitions.bin`、`firmware.bin`），网页会阻止开始刷机
- 若浏览器环境暂时无法访问 GitHub API，页面会回退为只保留“保留数据更新”模式
- 本仓库当前板型配置为经典 `ESP32`，并非启用 OPI PSRAM 的 `ESP32-S3`

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
