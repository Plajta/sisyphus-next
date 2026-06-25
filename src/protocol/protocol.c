#include <pico.h>
#include <pico/time.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "commands.h"
#include "class/cdc/cdc_device.h"
#include "protocol_common.h"
#include "protocol.h"


void handle_command(char *cmd) {
    char *argv[MAX_ARG_COUNT];
    int argc = 0;

    char *token = strtok(cmd, " ");
    while (token && argc < MAX_ARG_COUNT) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    if (argc == 0) return;

    const command_entry_t *cmd_entry = find_command(argv[0]);

    if (cmd_entry != NULL) {
        if (cmd_entry->expected_argc != -1 && argc != cmd_entry->expected_argc) {
            print_newline("err invalid arguments");
        } else {
            cmd_entry->handler(argc, argv);
        }
    } else {
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
