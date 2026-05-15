#ifndef FILE_IO_H
#define FILE_IO_H

#include "student.h"
#include "user.h"

int save_students_to_file(StudentList *list, const char *filename);
int load_students_from_file(StudentList *list, const char *filename);
int save_users_to_file(UserNode *head, const char *filename);
int load_users_from_file(UserNode **head, const char *filename);

#endif
