#include "common.h"

void putchar(char ch);
void printf(const char *fmt, ...);
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *buf, char c, size_t n);
char *strcpy(char *dst, const char *src);
int strcmp(const char *s1, const char *s2);

static void print_string(const char *str) {
    while (*str)
        putchar(*str++);
}

static void print_dec(int num) {
    unsigned magnitude = num;
    if (num < 0) {
        putchar('-');
        magnitude = -magnitude;
    }

    unsigned divisor = 1;
    while (magnitude / divisor > 9)
        divisor *= 10;
    
    while (divisor) {
        putchar('0' + magnitude / divisor);
        magnitude %= divisor;
        divisor /= 10;
    }
}

static void print_hex(unsigned num) {
    for (int i=7; i>=0; i--) {
        unsigned nibble = (num >> (i * 4)) & 0xF;
        putchar("0123456789abcdef"[nibble]);
    }
}

void printf(const char *fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);

    while (*fmt) {
        if (*fmt != '%') { // 일반 문자 바로 출력
            putchar(*fmt++);
            continue;
        }

        fmt++; // % 건너뛰기
        switch (*fmt) {
            case '\0':
                putchar('%'); // % 다음에 아무것도 없으면 % 문자로 출력
                goto end;
            case '%':
                putchar('%'); // %%는 % 문자로 출력
                break;
            case 's':
                print_string(va_arg(vargs, const char *));
                break;
            case 'd':
                print_dec(va_arg(vargs, int));
                break;
            case 'x':
                print_hex(va_arg(vargs, unsigned));
                break;
        }
        fmt++;
    }
end:
    va_end(vargs);
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *) dst;
    const uint8_t *s = (const uint8_t *) src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memset(void *buf, char c, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    while (n--)
        *p++ = c;
    return buf;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    *d = '\0';
    return dst;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}
