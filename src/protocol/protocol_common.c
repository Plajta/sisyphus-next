#include "protocol_common.h"
#include "class/cdc/cdc_device.h"
#include <pico/time.h>

void print_newline(char* c) {
    tud_cdc_write_str(c);
    tud_cdc_write_str("\r\n");
    tud_cdc_write_flush();
}

bool wait_for_ack_with_timeout(uint32_t timeout_ms) {
    absolute_time_t timeout = make_timeout_time_ms(timeout_ms);

    char ack_buffer[4] = {0};  // 'A', 'C', 'K', EOT
    uint8_t ack_index = 0;

    while (absolute_time_diff_us(get_absolute_time(), timeout) >= 0) {
        if (tud_cdc_available()) {
            char ch;
            if (tud_cdc_read(&ch, 1) == 1) {
                if (ack_index < sizeof(ack_buffer)) {
                    ack_buffer[ack_index++] = ch;
                }

                // Check if full ACK+EOT received
                if (ack_index == sizeof(ack_buffer)) {
                    bool ack_result = (memcmp(ack_buffer, "ack\x04", 4) == 0);
                    return ack_result;
                }
            }
        }
    }

    return false; // Timeout
}
