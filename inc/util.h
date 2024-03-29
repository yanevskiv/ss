#ifndef UTIL_H
#define UTIL_H
#include <stdarg.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#define PACK(x, y) ((((x) & 0xf) << 4) | (((y) & 0xf) << 0))
#define ARR_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
#define BUF_DEFAULT_SIZE 10
#define MAX_REGEX_MATCHES 100

#ifdef DEBUG
#define Show_Debug(...)  \
    do { \
        fprintf(stdout, "\e[32mDebug\e[0m: "); \
        fprintf(stdout, __VA_ARGS__); \
        fprintf(stdout, "\n"); \
        fflush(stdout); \
    } while (0)
#else
#define Show_Debug(...)
#endif

#define Show_Error(...) \
    do { \
        fprintf(stderr, "\e[31mError: \e[0m"); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        fflush(stderr); \
    } while (0)
#define Show_Warning(...) \
    do { \
        fprintf(stderr, "\e[33mWarning: \e[0m"); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        fflush(stderr); \
    } while (0)

#define TRUE 1
#define FALSE 0

// String utilities
char *Str_Create(const char *str);
void Str_Destroy(char *str);
char *Str_RmQuotes(char *str);
int Str_Equals(const char *str1, const char *str2);
int Str_CheckMatch(const char *str, const char *re);
char *Str_Concat(const char *str1, const char *str2);
int Str_RegexMatch(const char *str, const char *re, int match_size, regmatch_t *matches) ;
int Str_RegexMatchStr(const char *str, const char *re, int match_size, char **matches) ;
void Str_RegexClean(int match_size, char **matches);
char *Str_Substr(const char *str, int from, int to);
void Str_Cut(char **str, int from, int to);
void Str_Trim(char *str);
int Str_ParseInt(const char *str);
char Str_UnescapeChar(const char *str);
void Str_UnescapeStr(char *str);
int Str_RegexSplit(const char *str, const char *re, int split_size, char **split);
int Str_RegexExtract(const char *str, const char *re, int size, char **matches);
int Str_IsEmpty(const char *str);

#endif
