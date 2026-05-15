#ifndef UTILS_H
#define UTILS_H

void clear_screen(void);
void pause_and_wait(void);
int get_int_input(const char *prompt, int min, int max);
float get_float_input(const char *prompt, float min, float max);
void get_string_input(const char *prompt, char *buf, int max_len);
void trim_whitespace(char *str);
int confirm_action(const char *msg);

#endif
