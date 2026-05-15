#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "student.h"

const char *SUBJECT_NAMES[SUBJECT_COUNT] = {
    "数学", "语文", "英语", "物理", "化学"
};

/* === 链表生命周期 === */

StudentList* list_create(void) {
    StudentList *list = (StudentList*)malloc(sizeof(StudentList));
    if (!list) return NULL;
    list->head = NULL;
    list->count = 0;
    list->is_modified = 0;
    return list;
}

void list_destroy(StudentList *list) {
    if (!list) return;
    Node *cur = list->head;
    while (cur) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    free(list);
}

/* === 增删改 === */

int student_add(StudentList *list, Student stu) {
    if (!list) return 0;
    /* 检查学号重复 */
    if (student_find_by_id(list, stu.id)) return 0;

    Node *node = (Node*)malloc(sizeof(Node));
    if (!node) return 0;
    node->data = stu;
    node->next = NULL;

    /* 插入到链表尾部 */
    if (!list->head) {
        list->head = node;
    } else {
        Node *cur = list->head;
        while (cur->next) cur = cur->next;
        cur->next = node;
    }
    list->count++;
    list->is_modified = 1;
    return 1;
}

int student_delete(StudentList *list, const char *id) {
    if (!list || !list->head) return 0;

    Node *cur = list->head;
    Node *prev = NULL;

    while (cur) {
        if (strcmp(cur->data.id, id) == 0) {
            if (prev) {
                prev->next = cur->next;
            } else {
                list->head = cur->next;
            }
            free(cur);
            list->count--;
            list->is_modified = 1;
            return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}

int student_update(StudentList *list, const char *id, Student new_data) {
    Node *node = student_find_by_id(list, id);
    if (!node) return 0;
    /* 保留原学号 */
    new_data.total = node->data.total;
    new_data.average = node->data.average;
    new_data.rank = node->data.rank;
    node->data = new_data;
    list->is_modified = 1;
    return 1;
}

int student_update_score(StudentList *list, const char *id, Subject sub, float score) {
    Node *node = student_find_by_id(list, id);
    if (!node || sub < 0 || sub >= SUBJECT_COUNT) return 0;
    node->data.scores[sub] = score;
    list->is_modified = 1;
    return 1;
}

/* === 查找 === */

Node* student_find_by_id(StudentList *list, const char *id) {
    if (!list) return NULL;
    Node *cur = list->head;
    while (cur) {
        if (strcmp(cur->data.id, id) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

int student_find_by_name(StudentList *list, const char *name, StudentList *result) {
    if (!list) return 0;
    int count = 0;
    Node *cur = list->head;
    while (cur) {
        if (strstr(cur->data.name, name) != NULL) {
            student_add(result, cur->data);
            count++;
        }
        cur = cur->next;
    }
    return count;
}

int student_find_by_class(StudentList *list, const char *cls, StudentList *result) {
    if (!list) return 0;
    int count = 0;
    Node *cur = list->head;
    while (cur) {
        if (strcmp(cur->data.class_name, cls) == 0) {
            student_add(result, cur->data);
            count++;
        }
        cur = cur->next;
    }
    return count;
}

/* === 内部辅助：按总分排序（不触发排名计算） === */
static void sort_by_total_internal(StudentList *list) {
    if (!list || !list->head || !list->head->next) return;
    Node *i, *j, *max_node;
    Student temp;
    for (i = list->head; i->next != NULL; i = i->next) {
        max_node = i;
        for (j = i->next; j != NULL; j = j->next) {
            if (j->data.total > max_node->data.total) {
                max_node = j;
            }
        }
        if (max_node != i) {
            temp = i->data;
            i->data = max_node->data;
            max_node->data = temp;
        }
    }
}

/* === 统计 === */

void calc_total_and_average(StudentList *list) {
    if (!list) return;
    Node *cur = list->head;
    while (cur) {
        float sum = 0;
        for (int i = 0; i < SUBJECT_COUNT; i++) {
            sum += cur->data.scores[i];
        }
        cur->data.total = sum;
        cur->data.average = sum / SUBJECT_COUNT;
        cur = cur->next;
    }
}

void calc_rank(StudentList *list) {
    if (!list) return;
    sort_by_total_internal(list);
    int rank = 1;
    Node *cur = list->head;
    while (cur) {
        cur->data.rank = rank++;
        cur = cur->next;
    }
}

/* === 排序（选择排序，交换数据域） === */

void list_sort_by_total(StudentList *list) {
    sort_by_total_internal(list);
    calc_rank(list);
}

void list_sort_by_id(StudentList *list) {
    if (!list || !list->head || !list->head->next) return;
    Node *i, *j, *min_node;
    Student temp;
    for (i = list->head; i->next != NULL; i = i->next) {
        min_node = i;
        for (j = i->next; j != NULL; j = j->next) {
            if (strcmp(j->data.id, min_node->data.id) < 0) {
                min_node = j;
            }
        }
        if (min_node != i) {
            temp = i->data;
            i->data = min_node->data;
            min_node->data = temp;
        }
    }
}

void list_sort_by_subject(StudentList *list, Subject sub) {
    if (!list || !list->head || !list->head->next || sub < 0 || sub >= SUBJECT_COUNT) return;
    Node *i, *j, *max_node;
    Student temp;
    for (i = list->head; i->next != NULL; i = i->next) {
        max_node = i;
        for (j = i->next; j != NULL; j = j->next) {
            if (j->data.scores[sub] > max_node->data.scores[sub]) {
                max_node = j;
            }
        }
        if (max_node != i) {
            temp = i->data;
            i->data = max_node->data;
            max_node->data = temp;
        }
    }
}

/* === 统计 === */

void class_statistics(StudentList *list, const char *cls, Subject sub,
                      float *max, float *min, float *avg, int *pass_count) {
    *max = -1; *min = 101; *avg = 0; *pass_count = 0;
    float sum = 0;
    int count = 0;
    Node *cur = list->head;
    while (cur) {
        if (strcmp(cur->data.class_name, cls) == 0) {
            float s = cur->data.scores[sub];
            if (s > *max) *max = s;
            if (s < *min) *min = s;
            sum += s;
            if (s >= 60) (*pass_count)++;
            count++;
        }
        cur = cur->next;
    }
    if (count > 0) *avg = sum / count;
    if (*max == -1) *max = 0;
    if (*min == 101) *min = 0;
}

void grade_statistics(StudentList *list, Subject sub,
                      float *max, float *min, float *avg, int *pass_count) {
    *max = -1; *min = 101; *avg = 0; *pass_count = 0;
    float sum = 0;
    int count = 0;
    Node *cur = list->head;
    while (cur) {
        float s = cur->data.scores[sub];
        if (s > *max) *max = s;
        if (s < *min) *min = s;
        sum += s;
        if (s >= 60) (*pass_count)++;
        count++;
        cur = cur->next;
    }
    if (count > 0) *avg = sum / count;
    if (*max == -1) *max = 0;
    if (*min == 101) *min = 0;
}

/* === 显示 === */

void student_print_header(void) {
    printf("%-12s %-10s %-6s %-16s", "学号", "姓名", "性别", "班级");
    for (int i = 0; i < SUBJECT_COUNT; i++) {
        printf(" %-6s", SUBJECT_NAMES[i]);
    }
    printf(" %-8s %-8s %-4s\n", "总分", "平均分", "排名");
    printf("--------------------------------------------------------------------------------");
    for (int i = 0; i < SUBJECT_COUNT; i++) printf("------");
    printf("----------------\n");
}

void student_print_one(const Student *stu) {
    printf("%-12s %-10s %-6s %-16s", stu->id, stu->name, stu->gender, stu->class_name);
    for (int i = 0; i < SUBJECT_COUNT; i++) {
        printf(" %-6.1f", stu->scores[i]);
    }
    printf(" %-8.1f %-8.1f %-4d\n", stu->total, stu->average, stu->rank);
}

void student_print_list(StudentList *list) {
    if (!list || !list->head) {
        printf("暂无学生数据！\n");
        return;
    }
    student_print_header();
    Node *cur = list->head;
    while (cur) {
        student_print_one(&cur->data);
        cur = cur->next;
    }
    printf("\n共 %d 名学生\n", list->count);
}

void student_print_detail(const Student *stu) {
    printf("\n========== 学生详细信息 ==========\n");
    printf("学号: %s\n", stu->id);
    printf("姓名: %s\n", stu->name);
    printf("性别: %s\n", stu->gender);
    printf("班级: %s\n", stu->class_name);
    printf("----------------------------------\n");
    for (int i = 0; i < SUBJECT_COUNT; i++) {
        printf("%s: %.1f\n", SUBJECT_NAMES[i], stu->scores[i]);
    }
    printf("----------------------------------\n");
    printf("总分: %.1f\n", stu->total);
    printf("平均分: %.1f\n", stu->average);
    printf("排名: %d\n", stu->rank);
    printf("==================================\n");
}
