#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "menu.h"
#include "student.h"
#include "user.h"
#include "file_io.h"
#include "utils.h"

#define DATA_DIR "data/"
#define STUDENT_FILE "data/students.txt"
#define USER_FILE "data/users.txt"
#define BACKUP_DIR "data/backup/"

/* === 内部函数声明 === */
static void menu_student_manage(StudentList *list);
static void menu_score_manage(StudentList *list);
static void menu_query_stats(StudentList *list);
static void menu_data_manage(StudentList **list);
static void menu_user_manage(UserNode **user_head);
static void handle_add_student(StudentList *list);
static void handle_delete_student(StudentList *list);
static void handle_update_student(StudentList *list);
static void handle_find_by_id(StudentList *list);
static void handle_find_by_name(StudentList *list);
static void handle_enter_scores(StudentList *list);
static void handle_update_score(StudentList *list);
static void handle_class_stats(StudentList *list);
static void handle_grade_stats(StudentList *list);
static void handle_failing_students(StudentList *list);
static void handle_score_distribution(StudentList *list);
static void handle_export_csv(StudentList *list);
static void handle_backup_data(StudentList *list);
static void handle_restore_data(StudentList **list);

/* === 主菜单 === */

void main_menu(StudentList *list, User *current_user, UserNode *user_head) {
    int choice;
    int max_choice = 5;

    while (1) {
        clear_screen();
        printf("\n");
        printf("========================================\n");
        printf("       Student Record System v1.0\n");
        printf("========================================\n");
        printf("  当前用户: %s (%s)\n", current_user->username, role_to_string(current_user->role));
        printf("----------------------------------------\n");
        printf("  1. Student Management\n");
        printf("  2. Score Management\n");
        printf("  3. 查询与统计\n");
        printf("  4. 数据管理\n");
        if (current_user->role == ROLE_ADMIN) {
            printf("  5. User Management\n");
        } else {
            max_choice = 4;
        }
        printf("  0. Save并Exit\n");
        printf("========================================\n");

        choice = get_int_input("Select", 0, max_choice);

        switch (choice) {
            case 1: menu_student_manage(list); break;
            case 2: menu_score_manage(list); break;
            case 3: menu_query_stats(list); break;
            case 4: menu_data_manage(&list); break;
            case 5:
                if (current_user->role == ROLE_ADMIN)
                    menu_user_manage(&user_head);
                break;
            case 0:
                if (list->is_modified) {
                    if (confirm_action("数据已修改，是否Save？")) {
                        save_students_to_file(list, STUDENT_FILE);
                    }
                }
                printf("感谢使用，再见！\n");
                return;
        }
    }
}

/* === Student Management === */

static void menu_student_manage(StudentList *list) {
    int choice;
    while (1) {
        clear_screen();
        printf("\n--- Student Management ---\n");
        printf("  1. Add Student\n");
        printf("  2. Delete Student\n");
        printf("  3. Edit Student\n");
        printf("  4. View All Students\n");
        printf("  5. Search by ID\n");
        printf("  6. Search by Name\n");
        printf("  0. Back上级\n");

        choice = get_int_input("Select", 0, 6);
        switch (choice) {
            case 1: handle_add_student(list); break;
            case 2: handle_delete_student(list); break;
            case 3: handle_update_student(list); break;
            case 4:
                clear_screen();
                student_print_list(list);
                pause_and_wait();
                break;
            case 5: handle_find_by_id(list); break;
            case 6: handle_find_by_name(list); break;
            case 0: return;
        }
    }
}

static void handle_add_student(StudentList *list) {
    Student stu;
    memset(&stu, 0, sizeof(Student));

    printf("\n--- Add Student ---\n");
    get_string_input("ID", stu.id, sizeof(stu.id));

    if (student_find_by_id(list, stu.id)) {
        printf("Error：ID %s already exists！\n", stu.id);
        pause_and_wait();
        return;
    }

    get_string_input("Name", stu.name, sizeof(stu.name));
    get_string_input("Gender (M/F)", stu.gender, sizeof(stu.gender));
    get_string_input("Class", stu.class_name, sizeof(stu.class_name));

    printf("Enter scores：\n");
    for (int i = 0; i < SUBJECT_COUNT; i++) {
        stu.scores[i] = get_float_input(SUBJECT_NAMES[i], 0, 100);
    }

    if (student_add(list, stu)) {
        calc_total_and_average(list);
        calc_rank(list);
        printf("添加success！\n");
    } else {
        printf("添加failed！\n");
    }
    pause_and_wait();
}

static void handle_delete_student(StudentList *list) {
    char id[20];
    get_string_input("Enter ID to delete", id, sizeof(id));

    Node *node = student_find_by_id(list, id);
    if (!node) {
        printf("Not foundIDfor %s 的Student！\n", id);
        pause_and_wait();
        return;
    }

    student_print_detail(&node->data);
    if (confirm_action("Confirm删除该Student？")) {
        student_delete(list, id);
        calc_rank(list);
        printf("删除success！\n");
    }
    pause_and_wait();
}

static void handle_update_student(StudentList *list) {
    char id[20];
    get_string_input("Enter ID to edit", id, sizeof(id));

    Node *node = student_find_by_id(list, id);
    if (!node) {
        printf("Not foundIDfor %s 的Student！\n", id);
        pause_and_wait();
        return;
    }

    printf("\n当前信息：\n");
    student_print_detail(&node->data);

    Student stu = node->data;
    printf("\n请输入新信息（直接回车保持不变）：\n");

    char buf[50];
    printf("Name [%s]: ", stu.name);
    if (fgets(buf, sizeof(buf), stdin) && buf[0] != '\n') {
        buf[strcspn(buf, "\n")] = '\0';
        strncpy(stu.name, buf, sizeof(stu.name) - 1);
    }
    printf("Gender [%s]: ", stu.gender);
    if (fgets(buf, sizeof(buf), stdin) && buf[0] != '\n') {
        buf[strcspn(buf, "\n")] = '\0';
        strncpy(stu.gender, buf, sizeof(stu.gender) - 1);
    }
    printf("Class [%s]: ", stu.class_name);
    if (fgets(buf, sizeof(buf), stdin) && buf[0] != '\n') {
        buf[strcspn(buf, "\n")] = '\0';
        strncpy(stu.class_name, buf, sizeof(stu.class_name) - 1);
    }

    if (student_update(list, id, stu)) {
        calc_total_and_average(list);
        calc_rank(list);
        printf("修改success！\n");
    }
    pause_and_wait();
}

static void handle_find_by_id(StudentList *list) {
    char id[20];
    get_string_input("Enter ID", id, sizeof(id));

    Node *node = student_find_by_id(list, id);
    if (node) {
        student_print_detail(&node->data);
    } else {
        printf("Not foundIDfor %s 的Student！\n", id);
    }
    pause_and_wait();
}

static void handle_find_by_name(StudentList *list) {
    char name[50];
    get_string_input("Enter name keyword", name, sizeof(name));

    StudentList *result = list_create();
    int count = student_find_by_name(list, name, result);
    if (count > 0) {
        printf("找到 %d 条记录：\n", count);
        student_print_list(result);
    } else {
        printf("Not foundcontaining \"%s\" 的Student！\n", name);
    }
    list_destroy(result);
    pause_and_wait();
}

/* === Score Management === */

static void menu_score_manage(StudentList *list) {
    int choice;
    while (1) {
        clear_screen();
        printf("\n--- Score Management ---\n");
        printf("  1. 录入StudentScore\n");
        printf("  2. 修改单科Score\n");
        printf("  3. 按总分排records\n");
        printf("  4. 按单科排records\n");
        printf("  0. Back上级\n");

        choice = get_int_input("Select", 0, 4);
        switch (choice) {
            case 1: handle_enter_scores(list); break;
            case 2: handle_update_score(list); break;
            case 3:
                calc_total_and_average(list);
                list_sort_by_total(list);
                clear_screen();
                printf("\n--- 按总分排records ---\n");
                student_print_list(list);
                pause_and_wait();
                break;
            case 4: {
                printf("\n选择Subject：\n");
                for (int i = 0; i < SUBJECT_COUNT; i++) {
                    printf("  %d. %s\n", i + 1, SUBJECT_NAMES[i]);
                }
                int sub = get_int_input("SelectSubject", 1, SUBJECT_COUNT) - 1;
                list_sort_by_subject(list, (Subject)sub);
                clear_screen();
                printf("\n--- 按%s排records ---\n", SUBJECT_NAMES[sub]);
                student_print_list(list);
                pause_and_wait();
                break;
            }
            case 0: return;
        }
    }
}

static void handle_enter_scores(StudentList *list) {
    char id[20];
    get_string_input("Enter ID", id, sizeof(id));

    Node *node = student_find_by_id(list, id);
    if (!node) {
        printf("Not foundIDfor %s 的Student！\n", id);
        pause_and_wait();
        return;
    }

    printf("for %s 录入Score：\n", node->data.name);
    for (int i = 0; i < SUBJECT_COUNT; i++) {
        node->data.scores[i] = get_float_input(SUBJECT_NAMES[i], 0, 100);
    }
    list->is_modified = 1;
    calc_total_and_average(list);
    calc_rank(list);
    printf("Score录入success！\n");
    pause_and_wait();
}

static void handle_update_score(StudentList *list) {
    char id[20];
    get_string_input("Enter ID", id, sizeof(id));

    Node *node = student_find_by_id(list, id);
    if (!node) {
        printf("Not foundIDfor %s 的Student！\n", id);
        pause_and_wait();
        return;
    }

    printf("\n当前Score：\n");
    for (int i = 0; i < SUBJECT_COUNT; i++) {
        printf("  %s: %.1f\n", SUBJECT_NAMES[i], node->data.scores[i]);
    }

    printf("\n选择要修改的Subject：\n");
    for (int i = 0; i < SUBJECT_COUNT; i++) {
        printf("  %d. %s\n", i + 1, SUBJECT_NAMES[i]);
    }
    int sub = get_int_input("Select", 1, SUBJECT_COUNT) - 1;
    float score = get_float_input("新Score", 0, 100);

    student_update_score(list, id, (Subject)sub, score);
    calc_total_and_average(list);
    calc_rank(list);
    printf("修改success！\n");
    pause_and_wait();
}

/* === 查询与统计 === */

static void menu_query_stats(StudentList *list) {
    int choice;
    while (1) {
        clear_screen();
        printf("\n--- 查询与统计 ---\n");
        printf("  1. 查看StudentScore单\n");
        printf("  2. ClassScore统计\n");
        printf("  3. 年级Score统计\n");
        printf("  4. 不及格Studentrecords单\n");
        printf("  5. Score分段统计\n");
        printf("  0. Back上级\n");

        choice = get_int_input("Select", 0, 5);
        switch (choice) {
            case 1: handle_find_by_id(list); break;
            case 2: handle_class_stats(list); break;
            case 3: handle_grade_stats(list); break;
            case 4: handle_failing_students(list); break;
            case 5: handle_score_distribution(list); break;
            case 0: return;
        }
    }
}

static void handle_class_stats(StudentList *list) {
    char cls[30];
    get_string_input("Enter class name", cls, sizeof(cls));

    clear_screen();
    printf("\n--- %s ClassScore统计 ---\n", cls);
    printf("%-8s %-8s %-8s %-8s %-8s\n", "Subject", "最高分", "最低分", "平均分", "及格人数");
    printf("--------------------------------------------\n");

    for (int i = 0; i < SUBJECT_COUNT; i++) {
        float max, min, avg;
        int pass;
        class_statistics(list, cls, (Subject)i, &max, &min, &avg, &pass);
        printf("%-8s %-8.1f %-8.1f %-8.1f %-8d\n", SUBJECT_NAMES[i], max, min, avg, pass);
    }
    pause_and_wait();
}

static void handle_grade_stats(StudentList *list) {
    clear_screen();
    printf("\n--- 年级Score统计 ---\n");
    printf("%-8s %-8s %-8s %-8s %-8s\n", "Subject", "最高分", "最低分", "平均分", "及格人数");
    printf("--------------------------------------------\n");

    for (int i = 0; i < SUBJECT_COUNT; i++) {
        float max, min, avg;
        int pass;
        grade_statistics(list, (Subject)i, &max, &min, &avg, &pass);
        printf("%-8s %-8.1f %-8.1f %-8.1f %-8d\n", SUBJECT_NAMES[i], max, min, avg, pass);
    }
    printf("\nStudent总数: %d\n", list->count);
    pause_and_wait();
}

static void handle_failing_students(StudentList *list) {
    clear_screen();
    printf("\n--- 不及格Studentrecords单 ---\n");

    int found = 0;
    Node *cur = list->head;
    while (cur) {
        for (int i = 0; i < SUBJECT_COUNT; i++) {
            if (cur->data.scores[i] < 60) {
                if (!found) {
                    printf("%-12s %-10s %-8s %-10s %-8s\n", "ID", "Name", "Class", "Subject", "Score");
                    printf("------------------------------------------------\n");
                }
                printf("%-12s %-10s %-8s %-10s %-8.1f\n",
                       cur->data.id, cur->data.name, cur->data.class_name,
                       SUBJECT_NAMES[i], cur->data.scores[i]);
                found = 1;
            }
        }
        cur = cur->next;
    }

    if (!found) printf("没有不及格的Student！\n");
    pause_and_wait();
}

static void handle_score_distribution(StudentList *list) {
    clear_screen();
    printf("\n--- Score分段统计（字符柱状图）---\n");

    for (int s = 0; s < SUBJECT_COUNT; s++) {
        int dist[6] = {0}; /* 0-59, 60-69, 70-79, 80-89, 90-99, 100 */
        Node *cur = list->head;
        while (cur) {
            float score = cur->data.scores[s];
            if (score < 60) dist[0]++;
            else if (score < 70) dist[1]++;
            else if (score < 80) dist[2]++;
            else if (score < 90) dist[3]++;
            else if (score < 100) dist[4]++;
            else dist[5]++;
            cur = cur->next;
        }

        printf("\n%s:\n", SUBJECT_NAMES[s]);
        const char *labels[] = {"<60 ", "60-69", "70-79", "80-89", "90-99", "100  "};
        for (int i = 0; i < 6; i++) {
            printf("  %s |", labels[i]);
            int bar_len = dist[i];
            for (int j = 0; j < bar_len; j++) printf("█");
            printf(" %d\n", dist[i]);
        }
    }
    pause_and_wait();
}

/* === 数据管理 === */

static void menu_data_manage(StudentList **list) {
    int choice;
    while (1) {
        clear_screen();
        printf("\n--- 数据管理 ---\n");
        printf("  1. 从文件导入\n");
        printf("  2. Save到文件\n");
        printf("  3. 备份数据\n");
        printf("  4. 恢复数据\n");
        printf("  5. 导出forCSV\n");
        printf("  0. Back上级\n");

        choice = get_int_input("Select", 0, 5);
        switch (choice) {
            case 1: {
                char path[256];
                get_string_input("请输入导入文件路径", path, sizeof(path));
                load_students_from_file(*list, path);
                pause_and_wait();
                break;
            }
            case 2:
                save_students_to_file(*list, STUDENT_FILE);
                pause_and_wait();
                break;
            case 3: handle_backup_data(*list); break;
            case 4: handle_restore_data(list); break;
            case 5: handle_export_csv(*list); break;
            case 0: return;
        }
    }
}

static void handle_backup_data(StudentList *list) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[256];
    snprintf(filename, sizeof(filename),
             "data/backup/students_%04d%02d%02d_%02d%02d%02d.txt",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);

    /* 确保 backup 目录存在 */
    system("mkdir -p data/backup 2>/dev/null || mkdir data\\backup 2>nul");

    if (save_students_to_file(list, filename)) {
        printf("备份success: %s\n", filename);
    }
    pause_and_wait();
}

static void handle_restore_data(StudentList **list) {
    char path[256];
    get_string_input("请输入备份文件路径", path, sizeof(path));

    if (confirm_action("恢复将覆盖当前数据，Confirm？")) {
        list_destroy(*list);
        *list = list_create();
        load_students_from_file(*list, path);
        calc_total_and_average(*list);
        calc_rank(*list);
        printf("恢复success！\n");
    }
    pause_and_wait();
}

static void handle_export_csv(StudentList *list) {
    const char *csv_file = "data/students_export.csv";
    FILE *fp = fopen(csv_file, "w");
    if (!fp) {
        printf("无法创建CSV文件！\n");
        pause_and_wait();
        return;
    }

    /* BOM for Excel UTF-8 compatibility */
    fprintf(fp, "\xEF\xBB\xBF");

    /* 表头 */
    fprintf(fp, "ID,Name,Gender,Class");
    for (int i = 0; i < SUBJECT_COUNT; i++) {
        fprintf(fp, ",%s", SUBJECT_NAMES[i]);
    }
    fprintf(fp, ",总分,平均分,排records\n");

    /* 数据 */
    calc_total_and_average(list);
    calc_rank(list);
    Node *cur = list->head;
    while (cur) {
        Student *s = &cur->data;
        fprintf(fp, "%s,%s,%s,%s", s->id, s->name, s->gender, s->class_name);
        for (int i = 0; i < SUBJECT_COUNT; i++) {
            fprintf(fp, ",%.1f", s->scores[i]);
        }
        fprintf(fp, ",%.1f,%.1f,%d\n", s->total, s->average, s->rank);
        cur = cur->next;
    }

    fclose(fp);
    printf("CSV导出success: %s\n", csv_file);
    pause_and_wait();
}

/* === User Management === */

static void menu_user_manage(UserNode **user_head) {
    int choice;
    while (1) {
        clear_screen();
        printf("\n--- User Management ---\n");
        printf("  1. 注册新用户\n");
        printf("  2. 查看所有用户\n");
        printf("  0. Back上级\n");

        choice = get_int_input("Select", 0, 2);
        switch (choice) {
            case 1: {
                User user;
                memset(&user, 0, sizeof(User));
                get_string_input("Username", user.username, sizeof(user.username));

                if (user_find_by_username(*user_head, user.username)) {
                    printf("Usernamealready exists！\n");
                    pause_and_wait();
                    break;
                }

                get_string_input("Password", user.password, sizeof(user.password));

                printf("Role: 0-Admin 1-Teacher 2-Student\n");
                user.role = (UserRole)get_int_input("SelectRole", 0, 2);

                if (user.role == ROLE_STUDENT) {
                    get_string_input("Linked ID", user.bind_id, sizeof(user.bind_id));
                } else if (user.role == ROLE_TEACHER) {
                    get_string_input("Teacher编号", user.bind_id, sizeof(user.bind_id));
                }

                if (user_register(user_head, user)) {
                    save_users_to_file(*user_head, USER_FILE);
                    printf("注册success！\n");
                } else {
                    printf("注册failed！\n");
                }
                pause_and_wait();
                break;
            }
            case 2: {
                clear_screen();
                printf("\n--- 用户列表 ---\n");
                printf("%-15s %-10s %-15s\n", "Username", "Role", "Linked ID");
                printf("----------------------------------------\n");
                UserNode *cur = *user_head;
                while (cur) {
                    printf("%-15s %-10s %-15s\n",
                           cur->data.username,
                           role_to_string(cur->data.role),
                           cur->data.bind_id);
                    cur = cur->next;
                }
                pause_and_wait();
                break;
            }
            case 0: return;
        }
    }
}
