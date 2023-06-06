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
