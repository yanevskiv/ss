#include <util.h>
#include <stdarg.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#define BUF_DEFAULT_SIZE 10
#define MAX_REGEX_MATCHES 100

int strsel(const char *str, ...)
{
    va_list va;
    va_start(va, str);
    int index = 0;
    char *arg = va_arg(va, char*);
    while (arg)  {
        if (strcmp(arg, str) == 0) {
            va_end(va);
            return index;
        }

        index += 1;
        arg = va_arg(va, char*);
    }
    va_end(va);
    return -1;
}

char *Str_RmQuotes(char *str)
{
    char *head = str;
    char *iter = str;
    enum {
        SKIPPING = 0,
        READING = 1
    };
    int mode = SKIPPING;
    char prev = '\0';
    while (*iter) {
        if (*iter == '"')  {
            if (prev == '\\')  {
                *head++ = '"';
            } else {
                mode = (mode + 1) % 2;
            }
        } else {
            if (mode == READING) {
                *head++ = *iter;
            }
            prev = *iter;
        }
        *iter = '\0';
        iter++;
    }
    return str;
}

int Str_Equals(const char *str1, const char *str2)
{
    return strcmp(str1, str2) == 0;
}


char *Str_Create(const char *str)
{
    return strdup(str);
}

char* Str_RmSpaces(char *str)
{
    char* curs = str;
    while (*str) {
        if (! isspace(*str)) {
            *curs = *str;
            curs++;
        }
        str++;
    }
    *curs = '\0';
}

char* str_rmquotes(char *str)
{
    char *cur = str;
    enum {
        SKIP_SPACES,
        SKIP_DATA
    };
}


typedef struct buf_s {
    char *b_buffer;
    int b_size;
    int b_count;
} buf_t;

void buf_init(buf_t *buf)
{
    buf->b_buffer = (char*) calloc(BUF_DEFAULT_SIZE, 1);
    assert(buf->b_buffer != NULL);
    buf->b_buffer[0] = '\0';
    buf->b_size = BUF_DEFAULT_SIZE;
    buf->b_count = 0;
}

void buf_putc(buf_t *buf, char ch) 
{
    if (buf->b_count == buf->b_size) {
        buf->b_buffer = realloc(buf->b_buffer, (buf->b_size *= 2));
        assert(buf->b_buffer != NULL);
    }
    buf->b_buffer[buf->b_count] = ch;
    buf->b_count += 1;
    buf->b_buffer[buf->b_count] = '\0';
}

int Str_EqualsIn(const char *str, ...)
{
    int result = 0;
    va_list va;
    va_start(va, str);
    const char *arg = va_arg(va, const char*);
    while (arg) {
        if (Str_Equals(str, arg)) {
            result = 1;
            break;
        }
        arg = va_arg(va, const char*);
    }
    va_end(va);
    return result;
}

// Return substring
char *Str_Substr(const char *str, int from, int to)
{
    int len = strlen(str);
    if (from > to)  {
        int tmp = from;
        from = to;
        to = from;
    }
    if (from < 0) 
        from = 0;
    if (from > len)
        from = len;
    if (to < 0)
        to = len;
    if (to > len)
        to = len;
    char *substr = calloc(to - from + 1, sizeof(char));
    assert(substr != NULL);
    strncpy(substr, str + from, to - from);
    return substr;
}

// Delete a part of a string
// NOTE: This destroys `str` and returns a new one
// If you need the sliced part, use Str_Substr() first and then Str_Slice()
void Str_Cut(char **str, int from, int to)
{
    int len = strlen(*str);
    if (from > to)  {
        int tmp = from;
        from = to;
        to = from;
    }
    if (from < 0) 
        from = 0;
    if (from > len)
        from = len;
    if (to < 0)
        to = len;
    if (to > len)
        to = len;
    char *newstr = calloc(len - (to - from) + 1, sizeof(char));
    assert(newstr != NULL);
    int i, j = 0;
    for (i = 0; i < from; i++, j++)
        newstr[j] = (*str)[i];
    for (i = to; i < len; i++, j++)
        newstr[j] = (*str)[i];
    newstr[j++] = '\0';
    free(*str);
    *str = newstr;
}

int Str_RegexMatch(const char *str, const char *re, int match_size, regmatch_t *matches) 
{
    regex_t regex; 
    if (regcomp(&regex, re, REG_EXTENDED))  {
        fprintf(stderr, "Error (Assembler): Failed to compile regex '%s'\n", re);
        return 0;
    }
    int result = 0;
    if (regexec(&regex, str, match_size, matches, 0) == 0) {
        result = 1;
    }
    regfree(&regex);
    return result;
}

int Str_RegexExtract(const char *str, const char *re, int size, char **matches)
{
    regex_t regex;
    if (regcomp(&regex, re, REG_EXTENDED))  {
        fprintf(stderr, "Error (Assembler): Failed to compile regex '%s'\n", re);
        return 0;
    }
    int i, count = 0;
    regmatch_t regMatches[MAX_REGEX_MATCHES];
    if (regexec(&regex, str, ARR_SIZE(regMatches), regMatches, 0) == 0) {
        for (i = 0; i < MAX_REGEX_MATCHES; i++) {
            if (regMatches[i].rm_so == -1 || regMatches[i].rm_eo == -1) 
                break;
            char *substr = Str_Substr(str, regMatches[i].rm_so, regMatches[i].rm_eo);
            if (matches[i]) {
                free(matches[i]);
            }
            matches[i] = substr;
            count += 1;
        }
    }
    regfree(&regex);
    return count;
}

int Str_ArrayLength(const char **strarr)
{

}

int Str_CheckMatch(const char *str, const char *re)
{
    return Str_RegexMatch(str, re, 0, NULL);
}

char *Str_Concat(char *str1, char *str2)
{
    int len1 = strlen(str1);
    int len2 = strlen(str2);

    char *str = malloc(len1 + len2 + 1);
    assert(str != NULL);
    strcat(str, str1);
    strcat(str, str2);
    return str;
}

int Str_ParseInt(const char *str)
{
    int sign = 1;
    if (*str == '+') {
        str ++;
    }
    if (*str == '-') {
        sign = -1;
        str ++;
    }
    int value = 0;
    if (*str == '0')  {
        // If 0 is the only character, the value is 0
        if (*(str + 1) == '\0') 
            return 0;

        // Potentially octal or hexadecimal
        if (*(str + 1) == 'x')  {
            // Hexadecimal
            return sign * strtol(str + 2, NULL, 16);
        } else {
            // Octal
            return sign * strtol(str + 1, NULL, 8);
        }
    } else {
        // Decimal
        return sign * strtol(str, NULL, 10);
    }
    return value;
}


void Str_Trim(char *str)
{
    int i, from, to, len = strlen(str);
    if (len == 0)
        return;
    if (len == 1) {
        if (isspace(str[0]))
            str[0] = '\0';
    }
    for (to = len - 1; to >= 0; to -= 1) {
        if (! isspace(str[to])) 
            break;
    }
    for (from = 0; from < len; from++) {
         if (! isspace(str[from])) 
            break;
    }
    if (from > to) {
        str[0] = '\0';
    }
    for (i = 0; i < to - from + 1; i++) {
        str[i] = str[from + i];
    }
    str[i] = '\0';
}
