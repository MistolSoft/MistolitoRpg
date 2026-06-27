#include "usb_init.h"
#include "storage_task.h"
#include "esp_log.h"
#include "driver/usb_serial_jtag.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "USB_INIT";

static const char *REQUIRED_FILES[] = {
    "/DATA/game_tables.json",
    NULL
};

static usb_file_transfer_t s_transfer = {0};
static FILE *s_sd_file = NULL;
static usb_init_state_e s_state = USB_STATE_IDLE;
static bool s_start_loop_requested = false;

static int base64_decode_char(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static size_t base64_decode(const char *input, size_t input_len, uint8_t *output)
{
    if (input_len % 4 != 0) return 0;

    size_t output_len = (input_len / 4) * 3;
    if (input_len >= 1 && input[input_len - 1] == '=') output_len--;
    if (input_len >= 2 && input[input_len - 2] == '=') output_len--;

    size_t j = 0;
    for (size_t i = 0; i < input_len; i += 4) {
        int a = base64_decode_char(input[i]);
        int b = base64_decode_char(input[i + 1]);
        int c = (input[i + 2] == '=') ? 0 : base64_decode_char(input[i + 2]);
        int d = (input[i + 3] == '=') ? 0 : base64_decode_char(input[i + 3]);

        if (a < 0 || b < 0 || c < 0 || d < 0) return 0;

        output[j++] = (a << 2) | (b >> 4);
        if (input[i + 2] != '=') {
            output[j++] = ((b & 0x0F) << 4) | (c >> 2);
        }
        if (input[i + 3] != '=') {
            output[j++] = ((c & 0x03) << 6) | d;
        }
    }

    return j;
}

static esp_err_t handle_file_start(const char *params, char *response, size_t resp_len)
{
    char *saveptr;
    char params_copy[128];
    strncpy(params_copy, params, sizeof(params_copy) - 1);
    params_copy[sizeof(params_copy) - 1] = '\0';

    char *filename = strtok_r(params_copy, ":", &saveptr);
    char *size_str = strtok_r(NULL, ":", &saveptr);

    if (!filename || !size_str) {
        snprintf(response, resp_len, "ERROR:INVALID_PARAMS");
        return ESP_FAIL;
    }

    size_t file_size = (size_t)strtoul(size_str, NULL, 10);

    if (file_size == 0) {
        snprintf(response, resp_len, "ERROR:INVALID_SIZE:%zu", file_size);
        return ESP_FAIL;
    }

    strncpy(s_transfer.filename, filename, sizeof(s_transfer.filename) - 1);
    s_transfer.filename[sizeof(s_transfer.filename) - 1] = '\0';
    s_transfer.total_size = file_size;
    s_transfer.received_size = 0;
    s_transfer.in_progress = true;
    s_state = USB_STATE_RECEIVING_FILE;

    if (s_sd_file) {
        fclose(s_sd_file);
        s_sd_file = NULL;
    }

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s%s", MOUNT_POINT, s_transfer.filename);
    s_sd_file = fopen(full_path, "w");
    if (!s_sd_file) {
        ESP_LOGE(TAG, "fopen failed: %s", full_path);
        snprintf(response, resp_len, "ERROR:OPEN_FAILED:%s", s_transfer.filename);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "File opened for writing: %s (%zu bytes)", full_path, file_size);

    snprintf(response, resp_len, "FILE_RECV_START:%s:%zu", filename, file_size);
    return ESP_OK;
}

static esp_err_t handle_file_data(const char *params, char *response, size_t resp_len)
{
    if (!s_transfer.in_progress || !s_sd_file) {
        snprintf(response, resp_len, "ERROR:NO_FILE_IN_PROGRESS");
        return ESP_FAIL;
    }

    size_t params_len = strlen(params);
    uint8_t chunk[USB_DECODE_CHUNK_SIZE];
    size_t decoded_len = base64_decode(params, params_len, chunk);

    if (decoded_len == 0) {
        ESP_LOGE(TAG, "BASE64 decode failed, params_len=%zu", params_len);
        snprintf(response, resp_len, "ERROR:BASE64_DECODE_FAILED");
        return ESP_FAIL;
    }

    size_t written = fwrite(chunk, 1, decoded_len, s_sd_file);
    if (written != decoded_len) {
        ESP_LOGE(TAG, "fwrite failed: wrote %zu of %zu", written, decoded_len);
        snprintf(response, resp_len, "ERROR:WRITE_FAILED");
        s_transfer.in_progress = false;
        s_state = USB_STATE_ERROR;
        return ESP_FAIL;
    }

    s_transfer.received_size += decoded_len;

    if (s_transfer.received_size > s_transfer.total_size) {
        snprintf(response, resp_len, "ERROR:SIZE_OVERFLOW");
        s_transfer.in_progress = false;
        s_state = USB_STATE_ERROR;
        return ESP_FAIL;
    }

    int percent = (int)((s_transfer.received_size * 100) / s_transfer.total_size);
    snprintf(response, resp_len, "FILE_RECV_PROGRESS:%zu:%zu:%d%%", s_transfer.received_size, s_transfer.total_size, percent);
    return ESP_OK;
}

static esp_err_t handle_file_end(const char *params, char *response, size_t resp_len)
{
    (void)params;
    
    if (!s_transfer.in_progress || !s_sd_file) {
        snprintf(response, resp_len, "ERROR:NO_FILE_IN_PROGRESS");
        return ESP_FAIL;
    }

    fclose(s_sd_file);
    s_sd_file = NULL;

    snprintf(response, resp_len, "FILE_RECV_OK:%s:%zu bytes saved", s_transfer.filename, s_transfer.received_size);

    s_transfer.in_progress = false;
    s_state = USB_STATE_IDLE;

    return ESP_OK;
}

static esp_err_t handle_status(const char *params, char *response, size_t resp_len)
{
    (void)params;
    char missing[256] = {0};
    usb_get_missing_files(missing, sizeof(missing));

    if (strlen(missing) == 0) {
        snprintf(response, resp_len, "STATUS:READY_FOR_INIT");
    } else {
        snprintf(response, resp_len, "STATUS:WAITING_FOR_FILES|Missing: %s", missing);
    }
    return ESP_OK;
}

static esp_err_t handle_init_complete(const char *params, char *response, size_t resp_len)
{
    (void)params;
    char missing[256] = {0};
    usb_get_missing_files(missing, sizeof(missing));

    if (strlen(missing) > 0) {
        snprintf(response, resp_len, "ERROR:MISSING_FILES:%s", missing);
        return ESP_FAIL;
    }

    s_state = USB_STATE_COMPLETE;
    snprintf(response, resp_len, "INIT_COMPLETE:All files received. Starting...");
    return ESP_OK;
}

static esp_err_t handle_list_files(const char *params, char *response, size_t resp_len)
{
    (void)params;
    snprintf(response, resp_len, "FILES:game_tables.json");
    return ESP_OK;
}

static esp_err_t handle_wipe(const char *params, char *response, size_t resp_len)
{
    (void)params;
    ESP_LOGI(TAG, "Wiping all data...");

    char path[128];
    snprintf(path, sizeof(path), "%s/DATA/game_tables.json", MOUNT_POINT);
    storage_delete_file(path);
    snprintf(path, sizeof(path), "%s/BRAIN/PET/pet_data.json", MOUNT_POINT);
    storage_delete_file(path);

    snprintf(response, resp_len, "WIPE_COMPLETE");
    return ESP_OK;
}

static esp_err_t handle_start_loop(const char *params, char *response, size_t resp_len)
{
    (void)params;
    ESP_LOGI(TAG, "Start loop requested");
    s_start_loop_requested = true;
    s_state = USB_STATE_COMPLETE;
    snprintf(response, resp_len, "START_LOOP:Starting game loop...");
    return ESP_OK;
}

static const char *base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode_chunk(const uint8_t *data, size_t len, char *out, size_t *out_len)
{
    size_t i = 0;
    size_t j = 0;
    *out_len = 0;
    while (i < len) {
        uint32_t octet_a = i < len ? data[i++] : 0;
        uint32_t octet_b = i < len ? data[i++] : 0;
        uint32_t octet_c = i < len ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        out[j++] = base64_chars[(triple >> 18) & 0x3F];
        out[j++] = base64_chars[(triple >> 12) & 0x3F];
        out[j++] = base64_chars[(triple >> 6) & 0x3F];
        out[j++] = base64_chars[triple & 0x3F];
    }
    size_t padding = (3 - (len % 3)) % 3;
    for (size_t p = 0; p < padding; p++) {
        out[j - 1 - p] = '=';
    }
    *out_len = j;
}

static void usb_send_line(const char *line)
{
    usb_serial_jtag_write_bytes((const uint8_t *)line, strlen(line), pdMS_TO_TICKS(100));
    usb_serial_jtag_write_bytes((const uint8_t *)"\n", 1, pdMS_TO_TICKS(100));
}

static void handle_dump_replay(const char *params, char *response, size_t resp_len)
{
    (void)params;
    ESP_LOGI(TAG, "DUMP_REPLAY requested");

    char found_dir[64] = {0};
    char test_path[128];

    for (uint8_t a = 1; a <= 10; a++) {
        snprintf(test_path, sizeof(test_path), "/sdcard/BRAIN/COMBAT/replay_a%u/header.bin", a);
        if (fopen(test_path, "rb")) {
            snprintf(found_dir, sizeof(found_dir), "/sdcard/BRAIN/COMBAT/replay_a%u", a);
            ESP_LOGI(TAG, "Found replay data in %s", found_dir);
            break;
        }
    }

    if (!found_dir[0]) {
        snprintf(response, resp_len, "DUMP_ERROR:NO_REPLAY_DATA");
        return;
    }

    char header_path[128];
    snprintf(header_path, sizeof(header_path), "%s/header.bin", found_dir);
    FILE *hf = fopen(header_path, "rb");
    if (!hf) {
        snprintf(response, resp_len, "DUMP_ERROR:NO_REPLAY_DATA");
        return;
    }

    replay_header_t header;
    if (fread(&header, sizeof(replay_header_t), 1, hf) != 1) {
        fclose(hf);
        snprintf(response, resp_len, "DUMP_ERROR:HEADER_READ_FAILED");
        return;
    }
    fclose(hf);

    char chunk_path[128];
    for (uint32_t c = 1; c <= header.total_chunks; c++) {
        snprintf(chunk_path, sizeof(chunk_path), "%s/chunk_%04lu.bin", found_dir, (unsigned long)c);
        FILE *cf = fopen(chunk_path, "rb");
        if (!cf) continue;

        fseek(cf, 0, SEEK_END);
        long file_size = ftell(cf);
        fseek(cf, 0, SEEK_SET);

        char start_line[256];
        snprintf(start_line, sizeof(start_line), "FILE_START:%s/chunk_%04lu.bin:%ld", found_dir, (unsigned long)c, file_size);
        usb_send_line(start_line);

        uint8_t read_buf[48];
        char b64_buf[64];
        while (file_size > 0) {
            size_t to_read = file_size > 48 ? 48 : (size_t)file_size;
            size_t read_bytes = fread(read_buf, 1, to_read, cf);
            if (read_bytes == 0) break;
            file_size -= read_bytes;

            size_t b64_len;
            base64_encode_chunk(read_buf, read_bytes, b64_buf, &b64_len);
            b64_buf[b64_len] = '\0';

            char data_line[128];
            snprintf(data_line, sizeof(data_line), "FILE_DATA:%s", b64_buf);
            usb_send_line(data_line);
        }
        fclose(cf);
        usb_send_line("FILE_END");
    }

    snprintf(response, resp_len, "DUMP_END:%lu", (unsigned long)header.total_transitions);
}

esp_err_t usb_process_command(const char *cmd_line, char *response, size_t resp_len)
{
    if (strncmp(cmd_line, "CMD:", 4) != 0) {
        snprintf(response, resp_len, "ERROR:INVALID_FORMAT");
        return ESP_FAIL;
    }

    const char *cmd = cmd_line + 4;
    const char *colon = strchr(cmd, ':');

    char command[32];
    const char *params = "";

    if (colon) {
        size_t cmd_len = colon - cmd;
        if (cmd_len >= sizeof(command)) cmd_len = sizeof(command) - 1;
        memcpy(command, cmd, cmd_len);
        command[cmd_len] = '\0';
        params = colon + 1;
    } else {
        strncpy(command, cmd, sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';
    }

    if (strlen(command) == 0) {
        snprintf(response, resp_len, "ERROR:NO_COMMAND");
        return ESP_FAIL;
    }

    if (strcmp(command, "FILE_START") == 0) {
        return handle_file_start(params, response, resp_len);
    } else if (strcmp(command, "FILE_DATA") == 0) {
        return handle_file_data(params, response, resp_len);
    } else if (strcmp(command, "FILE_END") == 0) {
        return handle_file_end(params, response, resp_len);
    } else if (strcmp(command, "STATUS") == 0) {
        return handle_status(params, response, resp_len);
    } else if (strcmp(command, "INIT_COMPLETE") == 0) {
        return handle_init_complete(params, response, resp_len);
    } else if (strcmp(command, "LIST_FILES") == 0) {
        return handle_list_files(params, response, resp_len);
    } else if (strcmp(command, "WIPE") == 0) {
        return handle_wipe(params, response, resp_len);
    } else if (strcmp(command, "START_LOOP") == 0) {
        return handle_start_loop(params, response, resp_len);
    } else if (strcmp(command, "DUMP_REPLAY") == 0) {
        handle_dump_replay(params, response, resp_len);
        return ESP_OK;
    } else {
        snprintf(response, resp_len, "ERROR:UNKNOWN_COMMAND:%s", command);
        return ESP_FAIL;
    }
}

void usb_init_driver(void)
{
    usb_serial_jtag_driver_config_t usb_config = {
        .tx_buffer_size = 2048,
        .rx_buffer_size = 8192,
    };
    usb_serial_jtag_driver_install(&usb_config);
}

bool usb_check_required_files(void)
{
    for (int i = 0; REQUIRED_FILES[i] != NULL; i++) {
        char full_path[128];
        snprintf(full_path, sizeof(full_path), "%s%s", MOUNT_POINT, REQUIRED_FILES[i]);

        if (!storage_file_exists(full_path)) {
            return false;
        }
    }
    return true;
}

void usb_get_missing_files(char *buf, size_t buf_len)
{
    buf[0] = '\0';
    size_t pos = 0;

    for (int i = 0; REQUIRED_FILES[i] != NULL; i++) {
        char full_path[128];
        snprintf(full_path, sizeof(full_path), "%s%s", MOUNT_POINT, REQUIRED_FILES[i]);

        if (!storage_file_exists(full_path)) {
            const char *filename = REQUIRED_FILES[i];
            if (strncmp(filename, "/", 1) == 0) {
                filename++;
            }

            if (pos > 0 && pos < buf_len - 2) {
                buf[pos++] = ',';
                buf[pos++] = ' ';
            }

            if (pos < buf_len - strlen(filename) - 1) {
                strcpy(buf + pos, filename);
                pos += strlen(filename);
            }
        }
    }

    buf[pos] = '\0';
}

bool usb_read_byte(uint8_t *byte, uint32_t timeout_ms)
{
    return usb_serial_jtag_read_bytes(byte, 1, pdMS_TO_TICKS(timeout_ms)) > 0;
}

void usb_write_response(const char *response)
{
    usb_serial_jtag_write_bytes((const uint8_t *)response, strlen(response), pdMS_TO_TICKS(100));
    usb_serial_jtag_write_bytes((const uint8_t *)"\n", 1, pdMS_TO_TICKS(100));
}

bool usb_is_complete(void)
{
    return s_state == USB_STATE_COMPLETE;
}

bool usb_start_requested(void)
{
    return s_start_loop_requested;
}

void usb_reset_state(void)
{
    s_state = USB_STATE_IDLE;
    s_start_loop_requested = false;
    s_transfer.in_progress = false;
    if (s_sd_file) {
        fclose(s_sd_file);
        s_sd_file = NULL;
    }
}
