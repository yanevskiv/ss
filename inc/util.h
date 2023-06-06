#ifndef UTIL_H
#define UTIL_H
#include <stdarg.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#define ARR_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
#define BUF_DEFAULT_SIZE 10
#define MAX_REGEX_MATCHES 100

#ifdef DEBUG
#define Show_Debug(...)  \
    do { \
        fprintf(stdout, "\e[32mDebug: \e[0m"); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
        fflush(stdout); \
    } while (0)
#else
#define Debug(...)
#endif

#define Show_Error(...) \
    do { \
        fprintf(stderr, "\e[31mError: \e[0m"); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        fflush(stderr); \
    } while (0)
#define TRUE 1
#define FALSE 0

char *Str_RmQuotes(char *str);
int Str_Equals(const char *str1, const char *str2);
int Str_CheckMatch(const char *str, const char *re);
char *Str_Concat(char *str1, char *str2);
int Str_RegexMatch(const char *str, const char *re, int match_size, regmatch_t *matches) ;
char *Str_Substr(const char *str, int from, int to);
void Str_Cut(char **str, int from, int to);
void Str_Trim(char *str);
int Str_ParseInt(const char *str);



#endif
