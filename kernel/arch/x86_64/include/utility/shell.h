#pragma once

#include <cstdint>

class Console;

class Shell
{
public:
    explicit Shell(Console* output_console);

    void execute(char* input);

    Console* get_console() const { return console; }

private:
    Console* console;

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
    void cmd_mkdir(int argc, char* argv[]);
    void cmd_cd(int argc, char* argv[]);
    void cmd_pwd();
    void cmd_search(int argc, char* argv[]);
    void cmd_start(int argc, char* argv[]);
    void cmd_grep(int argc, char* argv[]);

    static int  tokenize(char* input, char* argv[], int max_args);
    static void str_toupper(char* s);

    void print(const char* s);
    void println(const char* s);
};