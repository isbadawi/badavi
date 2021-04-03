#include "clar.h"

int main(int argc, char *argv[]) {
  setenv("BADAVI_TEST", "1", 1);
  return clar_test(argc, argv);
}
