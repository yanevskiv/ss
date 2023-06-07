#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>
#include <regex.h>

#define ARR_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
#define NORMAL 0
#define FAINT  2
#define RED    31
#define GREEN  32
#define YELLOW 33
#define BLUE   34
