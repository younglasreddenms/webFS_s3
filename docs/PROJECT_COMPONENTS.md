# ESP32-S3 Base Template Component Guide

本文档描述当前基础模板的组件分层、依赖边界和通用 API。当前 WebFS 不再作为 app 组件存在，而是作为 `main` 可加载的服务文件存在。

## 1. 当前结构

```text
webFS_s3/
├── main/
│   ├── main.c              # 主函数, 集中配置和初始化
│   ├── webfs.c             # WebFS 服务封装
│   ├── webfs.h
│   └── CMakeLists.txt
├── components/
│   ├── middlewares/
│   │   ├── middleware_storage/
│   │   ├── middleware_wifi/
│   │   ├── middleware_http_file_server/
│   │   └── middleware_display/
│   └── BSP/
│       ├── bsp_oled/
│       └── bsp_button/
└── partitions.csv
```

启动流程：

```text
app_main()
  ├── init_nvs()
  ├── storage_mount()
  ├── wifi_manager_sta_start()
  └── webfs_service_start()
        └── http_file_server_start()
```

## 2. 依赖原则

依赖方向：

```text
main -> service files -> middlewares -> BSP
```

规则：

- `main.c` 负责项目配置、初始化顺序和服务加载。
- WebFS 只是 `main/webfs.c` 中的可加载服务，不持有 WiFi 或存储配置。
- middleware 是通用能力，不使用 `webfs_` 专用命名。
- `http_file_server` 不依赖 `storage`，容量信息通过回调注入。
- BSP 只负责硬件基础读写。

## 3. main 入口

文件：

- `main/main.c`

当前主配置：

```c
#define APP_STORAGE_BASE_PATH "/littlefs"
#define APP_STORAGE_PARTITION_LABEL "storage"

#define APP_WIFI_SSID "sensen"
#define APP_WIFI_PASSWORD "11111111"
#define APP_WIFI_MAX_RETRY 10
```

### `app_main(void)`

```c
void app_main(void);
```

职责：

1. 配置 `storage_config_t`。
2. 配置 `wifi_manager_sta_config_t`。
3. 配置 `webfs_service_config_t`。
4. 初始化 NVS。
5. 挂载存储。
6. 连接 WiFi。
7. 加载 WebFS 服务。

当前调用方式：

```c
ESP_ERROR_CHECK(init_nvs());
ESP_ERROR_CHECK(storage_mount(&storage_config));
ESP_ERROR_CHECK(wifi_manager_sta_start(&wifi_config));
ESP_ERROR_CHECK(webfs_service_start(&webfs_config));
```

### `app_get_storage_info`

```c
static esp_err_t app_get_storage_info(const void *ctx, size_t *total, size_t *used);
```

职责：

- 将通用 `storage_get_info()` 适配成 `http_file_server` 可使用的回调。

当前实现：

```c
static esp_err_t app_get_storage_info(const void *ctx, size_t *total, size_t *used)
{
    return storage_get_info((const char *)ctx, total, used);
}
```

## 4. WebFS 服务文件

文件：

- `main/webfs.h`
- `main/webfs.c`

WebFS 当前只是“文件管理服务封装”，用于让 `main` 像加载服务一样启动 Web 文件管理能力。

### `webfs_service_config_t`

```c
typedef struct {
    const char *base_path;
    http_file_server_storage_info_fn_t get_storage_info;
    const void *storage_info_ctx;
} webfs_service_config_t;
```

字段说明：

| 字段 | 说明 |
|---|---|
| `base_path` | 要暴露给 Web 文件管理器的 VFS 根路径, 例如 `/littlefs` |
| `get_storage_info` | 可选容量查询回调 |
| `storage_info_ctx` | 传给容量查询回调的上下文 |

### `webfs_service_start`

```c
esp_err_t webfs_service_start(const webfs_service_config_t *config);
```

职责：

- 校验服务配置。
- 将 WebFS 服务配置转换成 `http_file_server_config_t`。
- 调用 `http_file_server_start()`。

返回值：

- `ESP_OK`: 启动成功。
- `ESP_ERR_INVALID_ARG`: 配置为空或 `base_path` 为空。
- 其他错误来自 `http_file_server_start()`。

示例：

```c
const webfs_service_config_t webfs_config = {
    .base_path = "/littlefs",
    .get_storage_info = app_get_storage_info,
    .storage_info_ctx = "storage",
};

ESP_ERROR_CHECK(webfs_service_start(&webfs_config));
```

### `webfs_service_stop`

```c
esp_err_t webfs_service_stop(void);
```

职责：

- 停止 WebFS 底层 HTTP 文件服务。

返回值：

- `ESP_OK`: 停止成功。
- `ESP_ERR_INVALID_STATE`: 服务未运行。

### `webfs_service_is_running`

```c
bool webfs_service_is_running(void);
```

职责：

- 查询 WebFS 服务是否正在运行。

## 5. middleware_storage

文件：

- `components/middlewares/middleware_storage/include/storage.h`
- `components/middlewares/middleware_storage/storage.c`

组件职责：

- 挂载 LittleFS。
- 查询容量。
- 提供通用 VFS 路径基础操作。

组件 CMake：

```cmake
idf_component_register(
    SRCS "storage.c"
    INCLUDE_DIRS "include"
    REQUIRES littlefs
)
```

### `storage_config_t`

```c
typedef struct {
    const char *base_path;
    const char *partition_label;
    bool format_if_mount_failed;
} storage_config_t;
```

字段说明：

| 字段 | 说明 |
|---|---|
| `base_path` | VFS 挂载点, 例如 `/littlefs` |
| `partition_label` | 分区名, 例如 `storage` |
| `format_if_mount_failed` | 挂载失败时是否格式化 |

### `storage_mount`

```c
esp_err_t storage_mount(const storage_config_t *config);
```

职责：

- 调用 `esp_vfs_littlefs_register()` 挂载 LittleFS。
- 挂载后打印总容量和已使用容量。

返回值：

- `ESP_OK`: 成功。
- `ESP_ERR_INVALID_ARG`: 参数无效。
- 其他错误来自 LittleFS。

示例：

```c
const storage_config_t storage_config = {
    .base_path = "/littlefs",
    .partition_label = "storage",
    .format_if_mount_failed = true,
};

ESP_ERROR_CHECK(storage_mount(&storage_config));
```

### `storage_get_info`

```c
esp_err_t storage_get_info(const char *partition_label, size_t *total, size_t *used);
```

职责：

- 查询指定 LittleFS 分区容量。

参数：

- `partition_label`: 分区名。
- `total`: 输出总容量。
- `used`: 输出已使用容量。

### `storage_stat`

```c
esp_err_t storage_stat(const char *path, struct stat *out_stat);
```

职责：

- 对 VFS 真实路径执行 `stat()`。

示例：

```c
struct stat st;
if (storage_stat("/littlefs/a.txt", &st) == ESP_OK) {
    ESP_LOGI("app", "size=%ld", (long)st.st_size);
}
```

### `storage_path_exists`

```c
bool storage_path_exists(const char *path);
```

职责：

- 判断 VFS 真实路径是否存在。

### `storage_path_is_dir`

```c
bool storage_path_is_dir(const char *path);
```

职责：

- 判断 VFS 真实路径是否为目录。

### `storage_create_dir`

```c
esp_err_t storage_create_dir(const char *path);
```

职责：

- 创建目录。
- 目录已存在时返回 `ESP_OK`。

### `storage_remove_path`

```c
esp_err_t storage_remove_path(const char *path);
```

职责：

- 删除文件。
- 如果目标是目录，则递归删除目录内容。

注意：

- 该 API 直接操作真实 VFS 路径，调用方应避免传入挂载根目录。

### `storage_rename_path`

```c
esp_err_t storage_rename_path(const char *from, const char *to);
```

职责：

- 重命名或移动文件/目录。

## 6. middleware_wifi

文件：

- `components/middlewares/middleware_wifi/include/wifi_manager.h`
- `components/middlewares/middleware_wifi/wifi_manager.c`

组件职责：

- 初始化 WiFi STA。
- 连接指定 AP。
- 提供连接状态和 IP 查询。

### `wifi_manager_sta_config_t`

```c
typedef struct {
    const char *ssid;
    const char *password;
    int max_retry;
    wifi_auth_mode_t authmode;
} wifi_manager_sta_config_t;
```

字段说明：

| 字段 | 说明 |
|---|---|
| `ssid` | WiFi 名称 |
| `password` | WiFi 密码 |
| `max_retry` | 最大重试次数 |
| `authmode` | 最低认证模式, 例如 `WIFI_AUTH_WPA2_PSK` |

### `wifi_manager_sta_start`

```c
esp_err_t wifi_manager_sta_start(const wifi_manager_sta_config_t *config);
```

职责：

- 初始化 `esp_netif`。
- 创建默认事件循环。
- 创建默认 STA 网络接口。
- 初始化 WiFi 驱动。
- 注册 WiFi/IP 事件。
- 启动 STA 并等待连接成功或失败。

示例：

```c
const wifi_manager_sta_config_t wifi_config = {
    .ssid = "sensen",
    .password = "11111111",
    .max_retry = 10,
    .authmode = WIFI_AUTH_WPA2_PSK,
};

ESP_ERROR_CHECK(wifi_manager_sta_start(&wifi_config));
```

### `wifi_manager_get_ip_info`

```c
esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t *ip_info);
```

职责：

- 获取最近一次 STA 成功连接时的 IP 信息。

返回值：

- `ESP_OK`: 获取成功。
- `ESP_ERR_INVALID_ARG`: 参数为空。
- `ESP_ERR_INVALID_STATE`: 尚未连接。

### `wifi_manager_is_connected`

```c
bool wifi_manager_is_connected(void);
```

职责：

- 返回当前连接状态。

## 7. middleware_http_file_server

文件：

- `components/middlewares/middleware_http_file_server/include/http_file_server.h`
- `components/middlewares/middleware_http_file_server/http_file_server.c`

组件职责：

- 提供浏览器文件管理页面。
- 提供文件列表、上传、下载、创建目录、删除、重命名 API。
- 操作任意已挂载的 VFS 基础路径。

### `http_file_server_storage_info_fn_t`

```c
typedef esp_err_t (*http_file_server_storage_info_fn_t)(const void *ctx,
                                                        size_t *total,
                                                        size_t *used);
```

职责：

- 由上层注入容量查询能力。
- 让 HTTP 文件服务不直接依赖具体存储组件。

### `http_file_server_config_t`

```c
typedef struct {
    const char *base_path;
    http_file_server_storage_info_fn_t get_storage_info;
    const void *storage_info_ctx;
} http_file_server_config_t;
```

字段说明：

| 字段 | 说明 |
|---|---|
| `base_path` | 暴露给 Web 的 VFS 根路径 |
| `get_storage_info` | 可选容量查询回调 |
| `storage_info_ctx` | 回调上下文 |

### `http_file_server_start`

```c
esp_err_t http_file_server_start(const http_file_server_config_t *config);
```

职责：

- 启动 ESP-IDF HTTP server。
- 注册文件管理页面和 API。
- 防止重复启动。

返回值：

- `ESP_OK`: 成功。
- `ESP_ERR_INVALID_ARG`: 配置无效。
- `ESP_ERR_INVALID_STATE`: 服务已运行。

### `http_file_server_stop`

```c
esp_err_t http_file_server_stop(void);
```

职责：

- 停止 HTTP 文件服务。

### `http_file_server_is_running`

```c
bool http_file_server_is_running(void);
```

职责：

- 查询服务是否运行。

### HTTP API

| URI | 方法 | 功能 |
|---|---|---|
| `/` | `GET` | Web 文件管理页面 |
| `/api/list?path=/` | `GET` | 目录列表 |
| `/api/stat?path=/a.txt` | `GET` | 文件大小和修改时间 |
| `/api/download?path=/a.txt` | `GET` | 下载文件 |
| `/api/upload?path=/a.txt&offset=0&truncate=1` | `POST` | 分块上传 |
| `/api/mkdir?path=/dir` | `POST` | 创建目录 |
| `/api/delete?path=/dir` | `POST` | 删除文件或目录 |
| `/api/rename?from=/a.txt&to=/b.txt` | `POST` | 重命名 |

路径说明：

- API 使用虚拟路径，例如 `/dir/a.txt`。
- 组件内部会映射到 `base_path` 下，例如 `/littlefs/dir/a.txt`。
- 禁止 `..`、反斜杠和控制字符。

## 8. middleware_display

文件：

- `components/middlewares/middleware_display/include/display.h`
- `components/middlewares/middleware_display/display.c`

职责：

- 基于 `bsp_oled` 提供逻辑 64x128 竖屏画布。
- 支持文本、页写入、位图和 framebuffer。

主要 API：

```c
void display_init(void);
void display_clear(void);
void display_text(int page, const char *str);
void display_write_page(int page, int column, const uint8_t *data, size_t len);
void display_bitmap(const uint8_t *bitmap_64x128);
void display_framebuffer(const uint8_t *fb_64x128);
```

常量：

```c
#define DISPLAY_WIDTH          64
#define DISPLAY_HEIGHT         128
#define DISPLAY_PAGE_COUNT     (DISPLAY_HEIGHT / 8)
#define DISPLAY_BITMAP_SIZE    (DISPLAY_WIDTH * DISPLAY_PAGE_COUNT)
#define DISPLAY_MAX_TEXT_CHARS (DISPLAY_WIDTH / 8)
```

## 9. bsp_oled

文件：

- `components/BSP/bsp_oled/include/bsp_oled.h`
- `components/BSP/bsp_oled/bsp_oled.c`

职责：

- SSD1306 OLED I2C 底层驱动。

硬件参数：

```c
#define BSP_OLED_I2C_PORT    I2C_NUM_0
#define BSP_OLED_I2C_ADDR    0x3C
#define BSP_OLED_SDA_PIN     GPIO_NUM_33
#define BSP_OLED_SCL_PIN     GPIO_NUM_32
#define BSP_OLED_I2C_FREQ_HZ 400000
#define BSP_OLED_WIDTH       128
#define BSP_OLED_HEIGHT      64
```

主要 API：

```c
void bsp_oled_init(void);
void bsp_oled_write_cmd(uint8_t cmd);
void bsp_oled_write_data(const uint8_t *data, size_t len);
void bsp_oled_set_page_column(uint8_t page, uint8_t column);
void bsp_oled_write_page(uint8_t page, uint8_t column,
                         const uint8_t *data, size_t len);
```

## 10. bsp_button

文件：

- `components/BSP/bsp_button/include/bsp_button.h`
- `components/BSP/bsp_button/bsp_button.c`

职责：

- GPIO22 按钮基础读写。
- 不负责消抖、长按、短按识别。

硬件参数：

```c
#define BSP_BUTTON_GPIO          GPIO_NUM_22
#define BSP_BUTTON_ACTIVE_LEVEL  0
```

API：

```c
esp_err_t bsp_button_init(void);
int bsp_button_get_level(void);
```

## 11. 分区

文件：`partitions.csv`

```csv
nvs,      data, nvs,     0x9000,   24K
phy_init, data, phy,     0xf000,   4K,
factory,  app,  factory, 0x10000,  8M,
storage,  data, littlefs,,  7M,
```

`main.c` 中的 `APP_STORAGE_PARTITION_LABEL` 必须与 `storage` 一致。

