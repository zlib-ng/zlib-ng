static const char hello[] = "hello, hello!";
static const int hello_len = sizeof(hello);

/* Display error message and exit */
void error(const char *format, ...) {
    va_list va;

    va_start(va, format);
    vfprintf(stderr, format, va);
    va_end(va);

    exit(EXIT_FAILURE);
}

#define CHECK_ERR(err, msg) { \
    if (err != Z_OK) \
        error("%s error: %d\n", msg, err); \
}
