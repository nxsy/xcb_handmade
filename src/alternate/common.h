#if !defined(HHXCB_COMMON_H)


#if defined(HHXCB_SLOW)
#define HHXCB_ASSERT(expression) do { if (!(expression)) {*(int *)0 = 0;} } while (0)
#else
#define HHXCB_ASSERT(expression) do {} while (0)
#endif

#define HHXCB_COMMON_H
#endif
