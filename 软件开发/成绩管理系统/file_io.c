#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_io.h"

int save_students_to_file(StudentList *list, const char *filename) {
    if (!list) return 0;
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("无法打开文件 %s 进行写入！\n", filename);
        return 0;
    }

    calc_total_and_average(list);
    fprintf(fp, "%d\n", list->count);

    Node *cur = list->head;
    while (cur) {
        Student *s = &cur->data;
        fprintf(fp, "%s|%s|%s|%s", s->id, s->name, s->gender, s->class_name);
        for (int i = 0; i < SUBJECT_COUNT; i++) {
            fprintf(fp, "|%.1f", s->scores[i]);
        }
        fprintf(fp, "\n");
        cur = cur->next;
    }

    fclose(fp);
    printf("成功保存 %d 条记录到 %s\n", list->count, filename);
    return 1;
}

int load_students_from_file(StudentList *list, const char *filename) {
    if (!list) return 0;
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("数据文件 %s 不存在，将创建新文件。\n", filename);
        return 0;
    }

    int count = 0;
    if (fscanf(fp, "%d\n", &count) != 1 || count <= 0) {
        fclose(fp);
        return 0;
    }

    int loaded = 0;
    for (int i = 0; i < count; i++) {
        char line[512];
        if (!fgets(line, sizeof(line), fp)) break;

        Student stu;
        memset(&stu, 0, sizeof(Student));

        /* 用 strtok 解析 | 分隔的字段 */
        char *token = strtok(line, "|\n");
        if (!token) continue;
        strncpy(stu.id, token, sizeof(stu.id) - 1);

        token = strtok(NULL, "|\n");
        if (!token) continue;
        strncpy(stu.name, token, sizeof(stu.name) - 1);

        token = strtok(NULL, "|\n");
        if (!token) continue;
        strncpy(stu.gender, token, sizeof(stu.gender) - 1);

        token = strtok(NULL, "|\n");
        if (!token) continue;
        strncpy(stu.class_name, token, sizeof(stu.class_name) - 1);

        int valid = 1;
        for (int j = 0; j < SUBJECT_COUNT; j++) {
            token = strtok(NULL, "|\n");
            if (!token) { valid = 0; break; }
            stu.scores[j] = (float)atof(token);
        }
        if (!valid) continue;

        if (student_add(list, stu)) loaded++;
    }

    fclose(fp);
    calc_total_and_average(list);
    calc_rank(list);
    printf("从 %s 加载了 %d 条记录\n", filename, loaded);
    return loaded;
}

int save_users_to_file(UserNode *head, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("无法打开文件 %s 进行写入！\n", filename);
        return 0;
    }

    int count = 0;
    UserNode *cur = head;
    while (cur) {
        count++;
        cur = cur->next;
    }
    fprintf(fp, "%d\n", count);

    cur = head;
    while (cur) {
        fprintf(fp, "%s|%s|%d|%s\n",
                cur->data.username, cur->data.password,
                (int)cur->data.role, cur->data.bind_id);
        cur = cur->next;
    }

    fclose(fp);
    return 1;
}

int load_users_from_file(UserNode **head, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    int count = 0;
    if (fscanf(fp, "%d\n", &count) != 1 || count <= 0) {
        fclose(fp);
        return 0;
    }

    int loaded = 0;
    for (int i = 0; i < count; i++) {
        char line[256];
        if (!fgets(line, sizeof(line), fp)) break;

        User user;
        memset(&user, 0, sizeof(User));

        char *token = strtok(line, "|\n");
        if (!token) continue;
        strncpy(user.username, token, sizeof(user.username) - 1);

        token = strtok(NULL, "|\n");
        if (!token) continue;
        strncpy(user.password, token, sizeof(user.password) - 1);

        token = strtok(NULL, "|\n");
        if (!token) continue;
        user.role = (UserRole)atoi(token);

        token = strtok(NULL, "|\n");
        if (token) strncpy(user.bind_id, token, sizeof(user.bind_id) - 1);

        if (user_register(head, user)) loaded++;
    }

    fclose(fp);
    return loaded;
}
