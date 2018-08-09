#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size);

int main(int argc, char **argv) {
  int i;
  fprintf(stderr, "StandaloneFuzzTargetMain: running %d inputs\n", argc - 1);

  for (i = 1; i < argc; i++) {
    size_t len, n_read;
    unsigned char *buf;
    FILE *f = fopen(argv[i], "r");
    assert(f);
    fprintf(stderr, "Running: %s\n", argv[i]);
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc(len);
    n_read = fread(buf, 1, len, f);
    assert(n_read == len);
    LLVMFuzzerTestOneInput(buf, len);
    free(buf);
    fprintf(stderr, "Done:    %s: (%d bytes)\n", argv[i], (int)n_read);
  }

  return 0;
}
