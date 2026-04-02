#pragma once

#include <cstdint>

int atoi(const char *str);

class Console;

class Shell
{
public:
    explicit Shell(Console* output_console);

    void execute(char* input);

    Console* get_console() const { return console; }

    void print(const char* s);
    void println(const char* s);
 
private:
    Console* console;

    void cmd_help();
    void cmd_clear();
    void cmd_echo(int argc, char* argv[]);
    void cmd_info();
    void cmd_page();
    void cmd_mk(int argument_count, char* argument_vector[]);
    void cmd_cat(int argument_count, char* argument_vector[]);
    void cmd_ls();
    void cmd_rm(int argc, char* argv[]);
    void cmd_end();
    void cmd_mkdir(int argc, char* argv[]);
    void cmd_cd(int argc, char* argv[]);
    void cmd_pwd();
    void cmd_search(int argument_count, char* argument_vector[]);
    void cmd_start(int argument_count, char* argument_vector[]);
    void cmd_grep(int argc, char* argv[]);
    void cmd_calc(int argc, char* argv[]);

    void cmd_madt();
    void cmd_i2c();
    void cmd_i2chid();
    void cmd_i2cpoll();

    void cmd_tasks();
    void cmd_spawn();
    void cmd_kill(int argc, char* argv[]);
    void cmd_bg();

    void cmd_mount();

    static int  tokenize(char* input, char* argv[], int max_args);
    static void str_toupper(char* s);


};