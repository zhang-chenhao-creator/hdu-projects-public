#ifndef STUDENT_H
#define STUDENT_H

/* Subject枚举 */
typedef enum {
    MATH = 0,
    CHINESE,
    ENGLISH,
    PHYSICS,
    CHEMISTRY,
    SUBJECT_COUNT
} Subject;

/* Subjectrecords称（在 student.c 中定义） */
extern const char *SUBJECT_NAMES[SUBJECT_COUNT];

/* Student信息结构体 */
typedef struct {
    char id[20];
    char name[50];
    char gender[10];
    char class_name[30];
    float scores[SUBJECT_COUNT];
    float total;
    float average;
    int rank;
} Student;

/* 链表节点 */
typedef struct Node {
    Student data;
    struct Node *next;
} Node;

/* 链表管理结构 */
typedef struct {
    Node *head;
    int count;
    int is_modified;
} StudentList;

/* === 链表生命周期 === */
StudentList* list_create(void);
void list_destroy(StudentList *list);

/* === 增删改 === */
int student_add(StudentList *list, Student stu);
int student_delete(StudentList *list, const char *id);
int student_update(StudentList *list, const char *id, Student new_data);
int student_update_score(StudentList *list, const char *id, Subject sub, float score);

/* === 查找 === */
Node* student_find_by_id(StudentList *list, const char *id);
int student_find_by_name(StudentList *list, const char *name, StudentList *result);
int student_find_by_class(StudentList *list, const char *cls, StudentList *result);

/* === 排序 === */
void list_sort_by_total(StudentList *list);
void list_sort_by_id(StudentList *list);
void list_sort_by_subject(StudentList *list, Subject sub);

/* === 统计 === */
void calc_total_and_average(StudentList *list);
void calc_rank(StudentList *list);
void class_statistics(StudentList *list, const char *cls, Subject sub,
                      float *max, float *min, float *avg, int *pass_count);
void grade_statistics(StudentList *list, Subject sub,
                      float *max, float *min, float *avg, int *pass_count);

/* === 显示 === */
void student_print_header(void);
void student_print_one(const Student *stu);
void student_print_list(StudentList *list);
void student_print_detail(const Student *stu);

#endif
