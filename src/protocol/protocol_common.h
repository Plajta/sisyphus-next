#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PROTOCOL_VERSION "1"

#define CMD_BUF_SIZE 128
#define CRC_BUF_SIZE 1024
#define MAX_BUF_SIZE 1024
#define TIMEOUT_MS 1000

#define MAX_ARG_COUNT 4

#define EOT_CHAR 0x04  // EOT (End Of Transmission) character

void print_newline(char* c);
bool wait_for_ack_with_timeout(uint32_t timeout_ms);
