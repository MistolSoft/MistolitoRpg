#ifndef USB_INIT_H
#define USB_INIT_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define USB_FILE_BUFFER_SIZE 8192
#define USB_DECODE_CHUNK_SIZE 128

typedef enum {
    USB_STATE_IDLE,
    USB_STATE_RECEIVING_FILE,
    USB_STATE_COMPLETE,
    USB_STATE_ERROR
} usb_init_state_e;

typedef struct {
    char filename[64];
    size_t total_size;
    size_t received_size;
    bool in_progress;
} usb_file_transfer_t;

void usb_init_driver(void);
bool usb_check_required_files(void);
void usb_get_missing_files(char *buf, size_t buf_len);
esp_err_t usb_process_command(const char *cmd_line, char *response, size_t resp_len);
bool usb_read_byte(uint8_t *byte, uint32_t timeout_ms);
void usb_write_response(const char *response);
bool usb_is_complete(void);
bool usb_start_requested(void);
void usb_reset_state(void);

#endif
