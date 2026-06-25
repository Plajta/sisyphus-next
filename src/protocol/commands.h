#pragma once

typedef void (*command_handler_t)(int argc, char **argv);

typedef struct {
    const char *name;
    int expected_argc;
    command_handler_t handler;
} command_entry_t;

const command_entry_t* find_command(const char *name);


// --- Command Prototypes ---
void do_ls(int argc, char **argv);
void do_pull(int argc, char **argv);
void do_push(int argc, char **argv);
void do_rm(int argc, char **argv);
void do_mv(int argc, char **argv);
void do_info(int argc, char **argv);
void do_measure(int argc, char **argv);
void do_play(int argc, char **argv);
void do_reset(int argc, char **argv);
