#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "user.h"

UserNode* user_list_create(void) {
    return NULL;
}

void user_list_destroy(UserNode *head) {
    while (head) {
        UserNode *next = head->next;
        free(head);
        head = next;
    }
}

UserNode* user_find_by_username(UserNode *head, const char *username) {
    UserNode *cur = head;
    while (cur) {
        if (strcmp(cur->data.username, username) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

int user_register(UserNode **head, User user) {
    if (user_find_by_username(*head, user.username)) return 0;

    UserNode *node = (UserNode*)malloc(sizeof(UserNode));
    if (!node) return 0;
    node->data = user;
    node->next = *head;
    *head = node;
    return 1;
}

const char* role_to_string(UserRole role) {
    switch (role) {
        case ROLE_ADMIN:   return "Admin";
        case ROLE_TEACHER: return "Teacher";
        case ROLE_STUDENT: return "Student";
        default:           return "未知";
    }
}

int login(UserNode *head, User *current_user) {
    char username[30], password[30];
    int attempts = 0;

    while (attempts < 3) {
        printf("\n========== 用户登录 ==========\n");
        printf("Username: ");
        if (fgets(username, sizeof(username), stdin) == NULL) continue;
        username[strcspn(username, "\n")] = '\0';

        printf("密  码: ");
        if (fgets(password, sizeof(password), stdin) == NULL) continue;
        password[strcspn(password, "\n")] = '\0';

        UserNode *node = user_find_by_username(head, username);
        if (node && strcmp(node->data.password, password) == 0) {
            *current_user = node->data;
            printf("\n登录success！欢迎, %s (%s)\n", current_user->username,
                   role_to_string(current_user->role));
            return 1;
        }

        attempts++;
        printf("Username或PasswordError！remaining %d attempts。\n", 3 - attempts);
    }

    printf("登录failed次数过多，程序Exit。\n");
    return 0;
}
