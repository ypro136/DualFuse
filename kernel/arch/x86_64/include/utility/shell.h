#pragma once

#include <cstdint>

// Forward declaration — shell.cpp includes console.h, not the other way around
class Console;

class Shell
{
public:
    // Bind this shell to a console for all output
    explicit Shell(Console* output_console);

    // Feed a completed input line (null-terminated, no newline).
    // Called by the keyboard handler when Enter is pressed.
    void execute(char* input);

    // Expose the bound console (used by keyboard routing)
    Console* get_console() const { return console; }

private:
    Console* console;

    // ── Command handlers ──────────────────────────────────────────────────
    void cmd_help();
    void cmd_clear();
    void cmd_echo(int argc, char* argv[]);
    void cmd_info();
    void cmd_page();
    void cmd_mk(int argc, char* argv[]);
    void cmd_cat(int argc, char* argv[]);
    void cmd_ls();
    void cmd_rm(int argc, char* argv[]);
    void cmd_end();

    // ── Helpers ───────────────────────────────────────────────────────────
    static int  tokenize(char* input, char* argv[], int max_args);

    // Convenience: printf into this shell's console
    void print(const char* s);
    void println(const char* s);
};