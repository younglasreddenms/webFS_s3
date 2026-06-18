#include <ctype.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "bsp_button.h"
#include "display.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "storage.h"
#include "webfs.h"
#include "wifi_manager.h"

#define APP_STORAGE_BASE_PATH "/littlefs"
#define APP_STORAGE_PARTITION_LABEL "storage"

#define APP_WIFI_SSID "sensen"
#define APP_WIFI_PASSWORD "11111111"
#define APP_WIFI_MAX_RETRY 10

#define APP_MAX_BITMAP_FILES 32
#define APP_MAX_PATH_LEN 320
#define APP_SCAN_INTERVAL_MS 3000
#define APP_BUTTON_POLL_MS 20
#define APP_BUTTON_DEBOUNCE_MS 50

static const char *TAG = "main";

typedef struct {
    char paths[APP_MAX_BITMAP_FILES][APP_MAX_PATH_LEN];
    size_t count;
} bitmap_file_list_t;

static bitmap_file_list_t s_bitmap_files;
static bitmap_file_list_t s_fresh_bitmap_files;
static uint8_t s_bitmap_buffer[DISPLAY_BITMAP_SIZE];

static esp_err_t app_get_storage_info(const void *ctx, size_t *total, size_t *used)
{
    return storage_get_info((const char *)ctx, total, used);
}

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static bool path_has_c_extension(const char *path)
{
    const char *ext = strrchr(path, '.');
    return ext && strcasecmp(ext, ".c") == 0;
}

static bool append_path(char *out, size_t out_size, const char *dir, const char *name)
{
    int written = snprintf(out, out_size, "%s/%s", dir, name);
    return written > 0 && written < (int)out_size;
}

static bool stream_find_symbol(FILE *file, const char *symbol)
{
    size_t matched = 0;
    const size_t symbol_len = strlen(symbol);
    int ch;

    while ((ch = fgetc(file)) != EOF) {
        if ((char)ch == symbol[matched]) {
            matched++;
            if (matched == symbol_len) {
                return true;
            }
        } else {
            matched = ((char)ch == symbol[0]) ? 1 : 0;
        }
    }

    return false;
}

static void skip_c_comment(FILE *file, int second)
{
    int ch;
    int prev = 0;

    if (second == '/') {
        while ((ch = fgetc(file)) != EOF && ch != '\n') {
        }
        return;
    }

    if (second == '*') {
        while ((ch = fgetc(file)) != EOF) {
            if (prev == '*' && ch == '/') {
                return;
            }
            prev = ch;
        }
    }
}

static bool bitmap_file_load(const char *path, uint8_t *bitmap)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    bool ok = false;
    size_t count = 0;

    if (!stream_find_symbol(file, "oled_bitmap")) {
        goto cleanup;
    }

    int ch;
    while ((ch = fgetc(file)) != EOF && ch != '{') {
    }
    if (ch != '{') {
        goto cleanup;
    }

    while ((ch = fgetc(file)) != EOF) {
        if (ch == '}') {
            ok = count == DISPLAY_BITMAP_SIZE;
            break;
        }

        if (isspace(ch) || ch == ',') {
            continue;
        }

        if (ch == '/') {
            int second = fgetc(file);
            if (second == '/' || second == '*') {
                skip_c_comment(file, second);
                continue;
            }
            if (second != EOF) {
                ungetc(second, file);
            }
        }

        if (!isdigit(ch)) {
            continue;
        }

        char token[16] = {(char)ch};
        size_t token_len = 1;
        while ((ch = fgetc(file)) != EOF && token_len < sizeof(token) - 1 &&
               (isalnum(ch) || ch == 'x' || ch == 'X')) {
            token[token_len++] = (char)ch;
        }
        token[token_len] = '\0';
        if (ch != EOF) {
            ungetc(ch, file);
        }

        char *end = NULL;
        unsigned long value = strtoul(token, &end, 0);
        if (end == token || *end != '\0' || value > 0xFF || count >= DISPLAY_BITMAP_SIZE) {
            ok = false;
            break;
        }

        if (bitmap) {
            bitmap[count] = (uint8_t)value;
        }
        count++;
    }

cleanup:
    fclose(file);
    return ok;
}

static bool bitmap_list_contains(const bitmap_file_list_t *list, const char *path, size_t *index)
{
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->paths[i], path) == 0) {
            if (index) {
                *index = i;
            }
            return true;
        }
    }

    return false;
}

static void scan_bitmap_dir(const char *dir_path, bitmap_file_list_t *list, int depth)
{
    if (depth > 4 || list->count >= APP_MAX_BITMAP_FILES) {
        return;
    }

    DIR *dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && list->count < APP_MAX_BITMAP_FILES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[APP_MAX_PATH_LEN];
        struct stat st;
        if (!append_path(path, sizeof(path), dir_path, entry->d_name) || stat(path, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            scan_bitmap_dir(path, list, depth + 1);
        } else if (path_has_c_extension(path) && bitmap_file_load(path, NULL)) {
            strlcpy(list->paths[list->count], path, sizeof(list->paths[list->count]));
            list->count++;
        }
    }

    closedir(dir);
}

static void bitmap_list_refresh(bitmap_file_list_t *list)
{
    memset(list, 0, sizeof(*list));
    scan_bitmap_dir(APP_STORAGE_BASE_PATH, list, 0);
}

static void show_status_text(const char *line0, const char *line1)
{
    display_clear();
    display_text(0, line0);
    display_text(1, line1);
}

static bool show_bitmap_path(const char *path)
{
    if (!bitmap_file_load(path, s_bitmap_buffer)) {
        ESP_LOGW(TAG, "Invalid bitmap file: %s", path);
        show_status_text("Bad file", "Upload .c");
        return false;
    }

    display_bitmap(s_bitmap_buffer);
    ESP_LOGI(TAG, "Displayed bitmap: %s", path);
    return true;
}

static void bitmap_viewer_task(void *arg)
{
    (void)arg;

    char current_path[APP_MAX_PATH_LEN] = "";
    size_t current_index = 0;
    bool last_pressed = false;
    TickType_t next_scan = 0;
    bitmap_file_list_t *files = &s_bitmap_files;
    bitmap_file_list_t *fresh = &s_fresh_bitmap_files;

    display_init();
    ESP_ERROR_CHECK(bsp_button_init());
    show_status_text("WebFS", "Ready");

    while (true) {
        TickType_t now = xTaskGetTickCount();
        if (now >= next_scan) {
            bitmap_list_refresh(fresh);

            size_t kept_index = 0;
            bool current_still_exists = current_path[0] != '\0' &&
                                        bitmap_list_contains(fresh, current_path, &kept_index);
            bool should_redraw = files->count == 0 && fresh->count > 0;

            *files = *fresh;
            if (files->count == 0) {
                current_path[0] = '\0';
                current_index = 0;
                show_status_text("No image", "Upload .c");
            } else if (!current_still_exists) {
                current_index = 0;
                strlcpy(current_path, files->paths[current_index], sizeof(current_path));
                show_bitmap_path(current_path);
            } else {
                current_index = kept_index;
                if (should_redraw) {
                    show_bitmap_path(current_path);
                }
            }

            next_scan = now + pdMS_TO_TICKS(APP_SCAN_INTERVAL_MS);
        }

        bool pressed = bsp_button_get_level() == BSP_BUTTON_ACTIVE_LEVEL;
        if (pressed && !last_pressed) {
            vTaskDelay(pdMS_TO_TICKS(APP_BUTTON_DEBOUNCE_MS));
            if (bsp_button_get_level() == BSP_BUTTON_ACTIVE_LEVEL) {
                bitmap_list_refresh(files);
                if (files->count > 0) {
                    if (current_path[0] != '\0') {
                        bitmap_list_contains(files, current_path, &current_index);
                    }
                    current_index = (current_index + 1) % files->count;
                    strlcpy(current_path, files->paths[current_index], sizeof(current_path));
                    show_bitmap_path(current_path);
                    ESP_LOGI(TAG, "Switched to %u/%u", (unsigned)(current_index + 1),
                             (unsigned)files->count);
                } else {
                    current_path[0] = '\0';
                    current_index = 0;
                    show_status_text("No image", "Upload .c");
                }

                while (bsp_button_get_level() == BSP_BUTTON_ACTIVE_LEVEL) {
                    vTaskDelay(pdMS_TO_TICKS(APP_BUTTON_POLL_MS));
                }
                pressed = false;
            }
        }
        last_pressed = pressed;

        vTaskDelay(pdMS_TO_TICKS(APP_BUTTON_POLL_MS));
    }
}

void app_main(void)
{
    const storage_config_t storage_config = {
        .base_path = APP_STORAGE_BASE_PATH,
        .partition_label = APP_STORAGE_PARTITION_LABEL,
        .format_if_mount_failed = true,
    };
    const wifi_manager_sta_config_t wifi_config = {
        .ssid = APP_WIFI_SSID,
        .password = APP_WIFI_PASSWORD,
        .max_retry = APP_WIFI_MAX_RETRY,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    const webfs_service_config_t webfs_config = {
        .base_path = APP_STORAGE_BASE_PATH,
        .get_storage_info = app_get_storage_info,
        .storage_info_ctx = APP_STORAGE_PARTITION_LABEL,
    };

    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(storage_mount(&storage_config));
    ESP_ERROR_CHECK(wifi_manager_sta_start(&wifi_config));
    ESP_ERROR_CHECK(webfs_service_start(&webfs_config));
    xTaskCreate(bitmap_viewer_task, "bitmap_viewer", 6144, NULL, 5, NULL);

    ESP_LOGI(TAG, "Services are ready");
}
