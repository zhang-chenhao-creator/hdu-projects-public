#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"

void clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void pause_and_wait(void) {
    printf("\n按 Enter 键继续...");
    while (getchar() != '\n');
}

int get_int_input(const char *prompt, int min, int max) {
    int value;
    char buf[100];
    while (1) {
        printf("%s [%d-%d]: ", prompt, min, max);
        if (fgets(buf, sizeof(buf), stdin) == NULL) continue;
        if (sscanf(buf, "%d", &value) == 1 && value >= min && value <= max) {
            return value;
        }
        printf("Invalid input，请重新输入！\n");
    }
}

float get_float_input(const char *prompt, float min, float max) {
    float value;
    char buf[100];
    while (1) {
        printf("%s [%.1f-%.1f]: ", prompt, min, max);
        if (fgets(buf, sizeof(buf), stdin) == NULL) continue;
        if (sscanf(buf, "%f", &value) == 1 && value >= min && value <= max) {
            return value;
        }
        printf("Invalid input，请重新输入！\n");
    }
}

void get_string_input(const char *prompt, char *buf, int max_len) {
    while (1) {
        printf("%s: ", prompt);
        if (fgets(buf, max_len, stdin) == NULL) continue;
        /* 去除末尾换行符 */
        buf[strcspn(buf, "\n")] = '\0';
        if (strlen(buf) > 0) return;
        printf("输入不能for空，请重新输入！\n");
    }
}

void trim_whitespace(char *str) {
    char *start, *end;
    /* 找到第一个非空白字符 */
    start = str;
    while (isspace((unsigned char)*start)) start++;
    if (*start == '\0') {
        str[0] = '\0';
        return;
    }
    /* 找到最后一个非空白字符 */
    end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    /* 移动字符串 */
    memmove(str, start, end - start + 1);
    str[end - start + 1] = '\0';
}

int confirm_action(const char *msg) {
    char buf[10];
    printf("%s (y/n): ", msg);
    if (fgets(buf, sizeof(buf), stdin) == NULL) return 0;
    return (buf[0] == 'y' || buf[0] == 'Y');
}
