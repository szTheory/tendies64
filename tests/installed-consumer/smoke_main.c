/*
 * The smoke case alone (Phase 11, plan 11-19): the executable t64c_smoke, built
 * only by the negative controls of controls.tsv (doctored header, relocated
 * prefix). It runs the same case as `t64c --case smoke` and takes no argument.
 */
#include "t64c_cases.h"

int main(void) {
  return t64c_case_smoke();
}
