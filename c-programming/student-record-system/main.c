#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "student.h"
#include "user.h"
#include "file_io.h"
#include "menu.h"
#include "utils.h"

#define STUDENT_FILE "data/students.txt"
#define USER_FILE "data/users.txt"

static void init_default_users(UserNode **head) {
    /* 如果没有用户文件，创建默认Admin */
    User admin;
    memset(&admin, 0, sizeof(User));
    strcpy(admin.username, "admin");
    strcpy(admin.password, "admin123");
    admin.role = ROLE_ADMIN;
    user_register(head, admin);

    User teacher;
    memset(&teacher, 0, sizeof(User));
    strcpy(teacher.username, "teacher");
    strcpy(teacher.password, "teacher123");
    teacher.role = ROLE_TEACHER;
    user_register(head, teacher);

    User student;
    memset(&student, 0, sizeof(User));
    strcpy(student.username, "student");
    strcpy(student.password, "student123");
    student.role = ROLE_STUDENT;
    strcpy(student.bind_id, "2024001");
    user_register(head, student);

    save_users_to_file(*head, USER_FILE);
}

int main(void) {
    /* 创建数据目录 */
    system("mkdir -p data 2>/dev/null || mkdir data 2>nul");
    system("mkdir -p data/backup 2>/dev/null || mkdir data\\backup 2>nul");

    /* 初始化用户链表 */
    UserNode *user_head = user_list_create();
    if (!load_users_from_file(&user_head, USER_FILE)) {
        printf("首次运行，创建默认用户...\n");
        printf("默认账号: admin/admin123, teacher/teacher123, student/student123\n");
        init_default_users(&user_head);
        pause_and_wait();
    }

    /* 用户登录 */
    User current_user;
    memset(&current_user, 0, sizeof(User));
    if (!login(user_head, &current_user)) {
        user_list_destroy(user_head);
        return 1;
    }
    pause_and_wait();

    /* 初始化Student链表 */
    StudentList *list = list_create();
    if (!list) {
        printf("内存分配failed！\n");
        user_list_destroy(user_head);
        return 1;
    }

    /* 加载Student数据 */
    load_students_from_file(list, STUDENT_FILE);
    calc_total_and_average(list);
    calc_rank(list);

    /* 进入主菜单 */
    main_menu(list, &current_user, user_head);

    /* Exit前清理 */
    list_destroy(list);
    user_list_destroy(user_head);

    return 0;
}
