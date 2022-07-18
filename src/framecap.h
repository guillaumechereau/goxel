#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

EXTERNC void framecap_init(void);
EXTERNC void framecap_sleep(void);

#undef EXTERNC