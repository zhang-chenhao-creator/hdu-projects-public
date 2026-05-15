#ifndef USER_H
#define USER_H

typedef enum {
    ROLE_ADMIN = 0,
    ROLE_TEACHER,
    ROLE_STUDENT
} UserRole;

typedef struct {
    char username[30];
    char password[30];
    UserRole role;
    char bind_id[20];
} User;

typedef struct UserNode {
    User data;
    struct UserNode *next;
} UserNode;

/* ÓÃ»§²Ù×÷ */
UserNode* user_list_create(void);
void user_list_destroy(UserNode *head);
UserNode* user_find_by_username(UserNode *head, const char *username);
int user_register(UserNode **head, User user);
int login(UserNode *head, User *current_user);
const char* role_to_string(UserRole role);

#endif
