# Base Template Development Guide

本项目现在是一个通用 ESP32-S3 基础模板。WebFS 只是 `main` 可加载的一个服务，不再是 app 层组件。

## 1. 分层

```text
main
  ├── service files
  └── middlewares
        └── BSP
```

当前目录：

```text
main/
  main.c
  webfs.c
  webfs.h

components/middlewares/
  middleware_storage/
  middleware_wifi/
  middleware_http_file_server/
  middleware_display/

components/BSP/
  bsp_oled/
  bsp_button/
```

## 2. 依赖规则

- `main.c` 是配置中心。
- 服务文件放在 `main` 同级，例如 `main/webfs.c`。
- middleware 必须通用化，不能带业务名。
- BSP 只描述硬件。
- middleware 之间尽量不互相依赖。
- 需要跨组件协作时，用配置结构、回调或上层适配函数。

示例：

- `http_file_server` 不依赖 `storage`。
- `main.c` 用 `app_get_storage_info()` 把 `storage_get_info()` 适配成 HTTP 文件服务的容量查询回调。

## 3. 新增服务

服务适合放在 `main/` 下。

目录示例：

```text
main/my_service.c
main/my_service.h
```

接口示例：

```c
typedef struct {
    const char *name;
} my_service_config_t;

esp_err_t my_service_start(const my_service_config_t *config);
esp_err_t my_service_stop(void);
bool my_service_is_running(void);
```

使用方式：

```c
const my_service_config_t config = {
    .name = "demo",
};

ESP_ERROR_CHECK(my_service_start(&config));
```

## 4. 新增 middleware

目录示例：

```text
components/middlewares/middleware_xxx/
  include/xxx.h
  xxx.c
  CMakeLists.txt
```

CMake 示例：

```cmake
idf_component_register(
    SRCS "xxx.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer
)
```

接口建议：

```c
typedef struct {
    const char *name;
    const void *ctx;
} xxx_config_t;

esp_err_t xxx_start(const xxx_config_t *config);
esp_err_t xxx_stop(void);
bool xxx_is_running(void);
```

规则：

- 不硬编码项目配置。
- 不包含 `main` 下的头文件。
- 不引用具体业务服务。
- 对外返回 `esp_err_t`。

## 5. 新增 BSP

目录示例：

```text
components/BSP/bsp_xxx/
  include/bsp_xxx.h
  bsp_xxx.c
  CMakeLists.txt
```

规则：

- BSP 可以硬编码板级引脚。
- BSP 不实现业务状态机。
- BSP 不依赖 middleware。

## 6. 顶层 CMake

新增组件后，在根 `CMakeLists.txt` 里加入：

```cmake
set(EXTRA_COMPONENT_DIRS
    components/BSP/bsp_button
    components/BSP/bsp_oled
    components/middlewares/middleware_display
    components/middlewares/middleware_storage
    components/middlewares/middleware_wifi
    components/middlewares/middleware_http_file_server
)
```

使用某个组件的模块，还需要在自己的 `CMakeLists.txt` 中写 `REQUIRES`。

## 7. 当前 main 依赖

`main/CMakeLists.txt`：

```cmake
idf_component_register(
    SRCS "main.c" "webfs.c" "bitmap.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash middleware_storage middleware_wifi middleware_http_file_server
)
```

## 8. 推荐扩展方式

想增加业务功能时：

1. 如果是可复用能力，先做 middleware。
2. 如果是硬件底层，做 BSP。
3. 如果是项目服务，放到 `main/service_name.c`。
4. 在 `main.c` 中配置和启动。

例如新增时间同步：

```text
components/middlewares/middleware_time/
main/time_service.c
```

例如新增业务 HTTP API：

```text
components/middlewares/middleware_http_api/
main/user_api_service.c
```

