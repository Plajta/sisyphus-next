#include <pico.h>
#include <pico/time.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "class/cdc/cdc_device.h"
#include "lfs.h"
#include "tusb.h"
#include "littlefs-pico.h"
#include "protocol.h"
#include "audio.h"
#include "color.h"

extern lfs_t lfs;

void print_newline(char* c) {
    tud_cdc_write_str(c);
    tud_cdc_write_str("\r\n");
    tud_cdc_write_flush();
}

static bool wait_for_ack_with_timeout(uint32_t timeout_ms) {
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

void ls_command(){
    lfs_dir_t dir;
    struct lfs_info info;

    // Open the root directory
    if (lfs_dir_open(&lfs, &dir, "/") != LFS_ERR_OK) {
        print_newline("err cannot open root directory");
        return;
    }

    // Read entries
    while (lfs_dir_read(&lfs, &dir, &info) > 0) {
        char entry_info[64];
        if (info.type == LFS_TYPE_REG) {
            snprintf(entry_info, sizeof(entry_info), "%s f %u", info.name, info.size);
        } else if (info.type == LFS_TYPE_DIR) {
            snprintf(entry_info, sizeof(entry_info), "%s d", info.name);
        }
        print_newline(entry_info);
    }
    tud_cdc_write_char(EOT_CHAR);

    // Close the directory
    lfs_dir_close(&lfs, &dir);
}

void pull_command(char *filename){
    // yes this function reads the file two times, but for the CRC32 it's worth it

    // Open the file
    lfs_file_t file;
    if (lfs_file_open(&lfs, &file, filename, LFS_O_RDONLY) != LFS_ERR_OK) {
        print_newline("err cannot open file");
        return;
    }

    int32_t file_size = lfs_file_size(&lfs, &file);

    uint8_t crc_buffer[CRC_BUF_SIZE];
    uint32_t crc = 0xFFFFFFFF; // Initial CRC value

    lfs_ssize_t bytes_read;
    while ((bytes_read = lfs_file_read(&lfs, &file, crc_buffer, sizeof(crc_buffer))) > 0) {
        crc = lfs_crc(crc, crc_buffer, bytes_read);
    }

    if (bytes_read < 0) {
        print_newline("err calculate crc");
        lfs_file_close(&lfs, &file);
        return;
    }

    crc ^= 0xFFFFFFFF; // Finalize CRC value

    lfs_file_rewind(&lfs, &file);

    char ack_buf[64];
    snprintf(ack_buf, sizeof(ack_buf), "ack %u %u", file_size, crc);

    print_newline(ack_buf);

    // Read the file
    char buffer[MAX_BUF_SIZE];
    size_t read_size;
    while ((read_size = lfs_file_read(&lfs, &file, buffer, sizeof(buffer))) > 0) {
        if (!wait_for_ack_with_timeout(TIMEOUT_MS)) {
            print_newline("err timeout");
            break;
        }
        tud_cdc_write(buffer, read_size);
        tud_cdc_write_flush();
    }

    // Close the file
    lfs_file_close(&lfs, &file);
}

void push_command(const char *filename, uint32_t *size, uint32_t *expected_checksum) {
    lfs_file_t file;

    // Open (or create) the file with write access
    if(lfs_file_open(&lfs, &file, filename, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) != LFS_ERR_OK){
        print_newline("err open file");
    }

    char buffer[MAX_BUF_SIZE];

    size_t total_written = 0;

    uint32_t crc = 0xFFFFFFFF; // Initial CRC value

    while (total_written < *size) {
        print_newline("ack");

        size_t to_write = *size - total_written;
        if (to_write > MAX_BUF_SIZE) {
            to_write = MAX_BUF_SIZE;
        }

        size_t received = 0;
        absolute_time_t timeout = make_timeout_time_ms(TIMEOUT_MS);

        while (received < to_write) {
            uint16_t count = tud_cdc_read(&buffer[received], to_write - received);
            if (count > 0) {
                received += count;
                timeout = make_timeout_time_ms(TIMEOUT_MS);  // Reset timeout after successful read
            } else {
                sleep_ms(1);
                if (absolute_time_diff_us(get_absolute_time(), timeout) < 0) {
                    print_newline("err timeout");
                    lfs_file_close(&lfs, &file);
                    return;
                }
            }
        }

        // Calculate CRC
        crc = lfs_crc(crc, buffer, to_write);

        // Write the chunk
        int written = lfs_file_write(&lfs, &file, buffer, to_write);
        if (written < 0) {
            lfs_file_close(&lfs, &file);
            print_newline("err write file");
            return;
        }

        total_written += written;
    }
    lfs_file_close(&lfs, &file);

    crc ^= 0xFFFFFFFF; // Finalize CRC value

    if (crc == *expected_checksum){
        print_newline("ack");
    }
    else{
        print_newline("err checksum mismatch");
        lfs_remove(&lfs, filename);
    }
}

void rm_command(char *filename) {
    struct lfs_info info;

    // Check if the file exists
    if (lfs_stat(&lfs, filename, &info) < LFS_ERR_OK) {
        print_newline("err file not found");
        return;
    }

    // Delete the file
    if (lfs_remove(&lfs, filename) < LFS_ERR_OK) {
        print_newline("err delete file");
    }

    print_newline("ack");
}

void mv_command(char *old_filename, char *new_filename) {
    struct lfs_info info;

    // Check if the old file exists
    if (lfs_stat(&lfs, old_filename, &info) < LFS_ERR_OK) {
        print_newline("err file not found");
        return;
    }

    // Check if the destination file exists
    if (lfs_stat(&lfs, new_filename, &info) == LFS_ERR_OK) {
        print_newline("err destination exists");
        return;
    }

    // Rename the file
    if (lfs_rename(&lfs, old_filename, new_filename) < LFS_ERR_OK) {
        print_newline("err rename file");
    }

    print_newline("ack");
}

void info_command(){
    const struct lfs_config *used_lfs_config = lfs.cfg;
    char info_buffer[128];
    snprintf(info_buffer, sizeof(info_buffer), "sisyphus %s %s %s %s %u %u %u %b %b", DEVICE_NAME, GIT_COMMIT_SHA, PROTOCOL_VERSION, BUILD_DATE, used_lfs_config->block_count, lfs_fs_size(&lfs), used_lfs_config->block_size, USE_ETERNITY, SISYPHUS_DEBUG_STATUS);
    print_newline(info_buffer);
}

void measure_command(){
    color_measurement colorm;
    color_read_sensor(&colorm);
    char measure_buffer[32];
    snprintf(measure_buffer, sizeof(measure_buffer), "%d %d %d %d", colorm.hue, colorm.saturation, colorm.value, colorm.clear);
    print_newline(measure_buffer);
}

#ifdef SISYPHUS_DEBUG
void last_press_filename_command(){
    extern struct color_matched_entry matched_color;
    extern bool matched_color_valid;
    extern volatile uint8_t button_index;
    char filename[16]; // 16 should be enough
    if (matched_color_valid){
        snprintf(filename, sizeof(filename), "%c_%d.wav", matched_color.name, button_index);
    }
    else {
        snprintf(filename, sizeof(filename), "Invalid match");
    }

    print_newline(filename);
}
#endif // SISYPHUS_DEBUG

void play_command(char *filename) {
    struct lfs_info info;

    // Check if the file exists
    if (lfs_stat(&lfs, filename, &info) < LFS_ERR_OK) {
        print_newline("err file not found");
        return;
    }

    print_newline("ack");

    play_audio(filename, 0x7FFF);
}

void reset_command() {
    lfs_unmount(&lfs);
    #if USE_ETERNITY
        watchdog_reboot(0, 0, 0); // reboot to eternity
    #else
        rom_reset_usb_boot(0,0); // reboot to bootrom
    #endif
    while (1) tight_loop_contents(); // Just in case
}

void handle_command(char *cmd) {
    char *args[MAX_ARG_COUNT];
    int argc = 0;

    char *token = strtok(cmd, " ");
    while (token && argc < MAX_ARG_COUNT) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }

    if (argc == 0) return;

    if (strcmp(args[0], "ls") == 0) {
        ls_command();
    }
    else if (strcmp(args[0], "pull") == 0)
    {
        if (argc != 2) {
            print_newline("err invalid arguments");
            return;
        }

        pull_command(args[1]);
    }
    else if (strcmp(args[0], "push") == 0)
    {
        if (argc != 4) {
            print_newline("err invalid arguments");
            return;
        }

        const char *filename = args[1];
        uint32_t size = (uint32_t)strtoul(args[2], NULL, 10);
        uint32_t crc = (uint32_t)strtoul(args[3], NULL, 10);

        push_command(filename, &size, &crc);
    }
    else if (strcmp(args[0], "rm") == 0)
    {
        if (argc != 2) {
            print_newline("err invalid arguments");
            return;
        }

        rm_command(args[1]);
    }
    else if (strcmp(args[0], "mv") == 0)
    {
        if (argc != 3) {
            print_newline("err invalid arguments");
            return;
        }

        mv_command(args[1], args[2]);
    }
    else if (strcmp(args[0], "play") == 0)
    {
        if (argc != 2) {
            print_newline("err invalid arguments");
            return;
        }

        play_command(args[1]);
    }
    else if (strcmp(args[0], "info") == 0)
    {
        info_command();
    }
    else if (strcmp(args[0], "measure") == 0)
    {
        measure_command();
    }
    else if (strcmp(args[0], "reset") == 0)
    {
        if (argc == 1) {
            reset_command();
        }
        else{
            print_newline("err invalid arguments");
            return;
        }
    }
    #ifdef SISYPHUS_DEBUG
    else if (strcmp(args[0], "lastPressFilename") == 0)
    {
        last_press_filename_command();
    }
    #endif // SISYPHUS_DEBUG
    else{
        print_newline("err unknown command");
    }

    tud_cdc_write_flush();
}

void protocol_loop()
{
    char cmd_buf[CMD_BUF_SIZE];
    size_t cmd_len = 0;

    while (tud_cdc_connected()) {
        if (tud_cdc_available()) {
            int n = tud_cdc_read(&cmd_buf[cmd_len], CMD_BUF_SIZE - cmd_len);

            cmd_len += n;

            if (cmd_len >= CMD_BUF_SIZE) {
                cmd_len = 0;
                memset(cmd_buf, 0, CMD_BUF_SIZE);
                tud_cdc_read_flush();
                print_newline("err command buffer overflow");
                continue;
            }

            for (size_t i = cmd_len-n; i < cmd_len; ++i) {
                if (cmd_buf[i] == EOT_CHAR) { // 4 is EOT
                    cmd_buf[i] = '\0';  // Null-terminate line
                    handle_command(cmd_buf);  // Process command

                    cmd_len = 0;
                    memset(cmd_buf, 0, CMD_BUF_SIZE);
                    break;
                }
            }
        }
        sleep_ms(10);
    }
}
