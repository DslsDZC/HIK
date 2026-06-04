/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 */

/**
 * 命令行服务（动态模块版）
 *
 * 作为动态模块由 module_manager 加载，通过串口提供交互式命令行。
 * 所有外部调用均已内联（动态模块不支持符号解析）。
 */

#include "service.h"

/* ===== 内联字符串函数（不依赖外部符号） ===== */

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int str_cmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

/* ===== 串口 I/O ===== */

#define COM1 0x3F8

static void putc(char c) {
    __asm__ volatile("outb %0, %w1" : : "a"(c), "Nd"((unsigned short)COM1));
    if (c == '\n') __asm__ volatile("outb %0, %w1" : : "a"('\r'), "Nd"((unsigned short)COM1));
}

static void puts(const char *s) {
    while (*s) putc(*s++);
}

/* 非阻塞读串口，返回 -1 表示无数据 */
static int getc(void) {
    unsigned char lsr;
    __asm__ volatile("inb %w1, %0" : "=a"(lsr) : "Nd"((unsigned short)(COM1 + 5)));
    if (!(lsr & 1)) return -1;
    unsigned char c;
    __asm__ volatile("inb %w1, %0" : "=a"(c) : "Nd"((unsigned short)COM1));
    return c;
}

/* ===== 命令系统 ===== */

#define MAX_COMMANDS 64
#define CMD_BUFFER_SIZE 256
#define INPUT_BUFFER_SIZE 256

typedef void (*cmd_handler_t)(const char *args);

typedef struct {
    const char *name;
    const char *description;
    cmd_handler_t handler;
} command_t;

static command_t g_command_table[MAX_COMMANDS];
static int g_command_count = 0;

static void int_to_str(unsigned long value, char *buf) {
    char tmp[20];
    int i = 0;
    if (value == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    while (value > 0) { tmp[i++] = '0' + (value % 10); value /= 10; }
    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

/* ===== 命令处理函数 ===== */

static void cmd_help(const char *args) {
    (void)args;
    puts("Available commands:\n");
    for (int i = 0; i < g_command_count; i++) {
        puts("  ");
        puts(g_command_table[i].name);
        int len = str_len(g_command_table[i].name);
        while (len < 16) { putc(' '); len++; }
        puts(g_command_table[i].description);
        puts("\n");
    }
}

static void cmd_echo(const char *args) {
    if (!args) return;
    puts(args);
    puts("\n");
}

static void cmd_version(const char *args) {
    (void)args;
    puts("HIC OS v0.1.0 (dynamic CLI module)\n");
}

static void cmd_clear(const char *args) {
    (void)args;
    /* 串口终端清屏 */
    puts("\033[2J\033[H");
}

static void cmd_mem(const char *args) {
    (void)args;
    puts("Memory info: not available from CLI module\n");
}

static void cmd_modlist(const char *args) {
    (void)args;
    puts("Module listing not available from CLI module\n");
}

static void cmd_modload(const char *args)   { (void)args; puts("modload: not available\n"); }
static void cmd_modunload(const char *args) { (void)args; puts("modunload: not available\n"); }
static void cmd_modinfo(const char *args)   { (void)args; puts("modinfo: not available\n"); }
static void cmd_modverify(const char *args) { (void)args; puts("modverify: not available\n"); }
static void cmd_modrestart(const char *args) { (void)args; puts("modrestart: not available\n"); }
static void cmd_modupdate(const char *args) { (void)args; puts("modupdate: not available\n"); }

/* ===== CLI 服务 API ===== */

void cli_service_register_command(const char *name, const char *desc, cmd_handler_t handler) {
    if (g_command_count >= MAX_COMMANDS) return;
    g_command_table[g_command_count].name = name;
    g_command_table[g_command_count].description = desc;
    g_command_table[g_command_count].handler = handler;
    g_command_count++;
}

void cli_service_process(const char *input) {
    if (!input || input[0] == '\0') return;
    while (*input == ' ' || *input == '\t') input++;
    char cmd_name[CMD_BUFFER_SIZE];
    const char *args;
    int i = 0;
    while (input[i] && input[i] != ' ' && input[i] != '\t' && input[i] != '\n' && i < CMD_BUFFER_SIZE - 1) {
        cmd_name[i] = input[i];
        i++;
    }
    cmd_name[i] = '\0';
    while (input[i] == ' ' || input[i] == '\t') i++;
    args = input[i] ? input + i : "";

    if (str_cmp(cmd_name, "help") == 0)    { cmd_help(args); return; }
    if (str_cmp(cmd_name, "echo") == 0)    { cmd_echo(args); return; }
    if (str_cmp(cmd_name, "version") == 0) { cmd_version(args); return; }
    if (str_cmp(cmd_name, "clear") == 0)   { cmd_clear(args); return; }
    if (str_cmp(cmd_name, "mem") == 0)     { cmd_mem(args); return; }
    if (str_cmp(cmd_name, "modlist") == 0) { cmd_modlist(args); return; }
    if (str_cmp(cmd_name, "modload") == 0) { cmd_modload(args); return; }
    if (str_cmp(cmd_name, "modunload") == 0)   { cmd_modunload(args); return; }
    if (str_cmp(cmd_name, "modinfo") == 0)     { cmd_modinfo(args); return; }
    if (str_cmp(cmd_name, "modverify") == 0)   { cmd_modverify(args); return; }
    if (str_cmp(cmd_name, "modrestart") == 0)  { cmd_modrestart(args); return; }
    if (str_cmp(cmd_name, "modupdate") == 0)   { cmd_modupdate(args); return; }

    puts("Unknown: ");
    puts(cmd_name);
    puts("\n");
}

/* ===== 初始化 ===== */

void cli_service_init(void) {
    g_command_count = 0;
    cli_service_register_command("help",    "show help",         cmd_help);
    cli_service_register_command("echo",    "echo text",         cmd_echo);
    cli_service_register_command("version", "show version",      cmd_version);
    cli_service_register_command("clear",   "clear screen",      cmd_clear);
    cli_service_register_command("mem",     "show memory info",  cmd_mem);
    cli_service_register_command("modlist", "list modules",      cmd_modlist);
}

/* ===== 主入口（由模块管理器调用） ===== */

void __attribute__((used)) service_start(void) {
    cli_service_init();
    puts("[CLI] Command line service started\n");
    puts("Type 'help' for available commands.\n");

    char line[INPUT_BUFFER_SIZE];
    int pos = 0;

    while (1) {
        int c = getc();
        if (c < 0) {
            /* 无输入，让出 CPU */
            __asm__ volatile("pause");
            continue;
        }

        if (c == '\r' || c == '\n') {
            putc('\n');
            line[pos] = '\0';
            if (pos > 0) {
                cli_service_process(line);
            }
            puts("$ ");
            pos = 0;
        } else if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--;
                putc('\b'); putc(' '); putc('\b');
            }
        } else if (pos < INPUT_BUFFER_SIZE - 1) {
            line[pos++] = (char)c;
            putc((char)c);
        }
    }
}
