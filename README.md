# ESP32-S3 WebFS OLED Bitmap Viewer

这是一个基于 ESP-IDF 的 ESP32-S3 项目，提供网页文件上传服务，并把 LittleFS 的 `bitmap` 文件夹中的 C 数组图片数据加载到内存后显示到 SSD1306 OLED 屏幕上。

## 功能

- ESP32-S3 连接 Wi-Fi 后启动 WebFS 网页文件管理器
- 支持在浏览器中上传、下载、重命名和删除文件
- 启动时一次性加载 `/bitmap` 文件夹中的 `.c` 文件，解析 `oled_bitmap[]` 数组
- OLED 显示 64x128 bitmap 图片
- 按钮按下后在内存中的 bitmap 图片之间切换

## 硬件连接

| 模块 | 引脚 |
| --- | --- |
| OLED SCL | GPIO18 |
| OLED SDA | GPIO17 |
| 按钮 | GPIO4 |

OLED 使用 I2C，默认地址为 `0x3C`。按钮按低电平触发，工程中启用了内部上拉。

## Bitmap 文件格式

网页上传到 `bitmap` 文件夹下的 `.c` 文件中需要包含如下数组：

```c
const unsigned char oled_bitmap[] = {
    0x00, 0x00, 0x00,
    /* ... */
};
```

每个有效 bitmap 文件对应一张图片。数组大小应为 1024 字节，对应 `64 x 128 / 8` 的单色 OLED 页面数据。程序启动后只加载一次，后续切换图片时不再读取文件系统。

## 项目结构

- `main/`：应用入口、WebFS 服务启动和 OLED bitmap 浏览逻辑
- `components/BSP/bsp_oled/`：SSD1306 OLED I2C 驱动封装
- `components/BSP/bsp_button/`：按钮 GPIO 封装
- `components/middlewares/middleware_display/`：OLED 显示中间层
- `components/middlewares/middleware_http_file_server/`：网页文件管理服务
- `components/middlewares/middleware_storage/`：LittleFS 存储封装
- `components/middlewares/middleware_wifi/`：Wi-Fi 连接管理

## 使用方式

1. 修改 `main/main.c` 中的 Wi-Fi SSID 和密码。
2. 烧录程序到 ESP32-S3。
3. 设备启动后，通过日志查看 IP 地址。
4. 在浏览器打开设备 IP，创建 `bitmap` 文件夹，并上传包含 `oled_bitmap[]` 的 `.c` 文件。
5. 按下 GPIO4 按钮切换下一张图片。
