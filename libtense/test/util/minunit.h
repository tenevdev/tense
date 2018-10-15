#ifndef __MINUNIT_H__
#define __MINUNIT_H__

#define mu_assert(test, message, ...) do { \
        if (!(test)) printf(message "\n", ##__VA_ARGS__); } while (0)

#define mu_error(ret, message, ...) do { \
        if (ret == -1) { \
                printf("error: " message "\n", ##__VA_ARGS__); \
                return -1; \
        } \
} while (0)

#endif /* __MINUNIT_H__ */