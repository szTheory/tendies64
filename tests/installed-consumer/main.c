/*
 * Independent consumer of the installed tendies64 package (Phase 11, plan
 * 11-16, VER-05). It includes the public header of the package and system
 * headers only, links the archive through find_package(tendies64), and proves
 * that header and library agree.
 *
 *   t64c --list-cases                print the case names, one per line
 *   t64c --case NAME [OPTIONS]       run one case
 *
 * Options (the cases of tests/installed-consumer/cases.txt): --rom PATH is the
 * authored composite ROM of the build tree, --print writes machine-readable
 * "print KEY VALUE" lines, --out DIR writes the run's outputs as files, --slice
 * N sets the ticks per advance of a run.
 *
 * Exit status: 0 the case passed, 1 a check failed or ran no check, 2 usage.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tendies64/t64_machine.h>

#include "t64c_alloc.h"
#include "t64c_cases.h"
#include "t64c_check.h"

static int case_smoke_entry(const t64c_args *args) {
  (void)args;
  return t64c_case_smoke();
}

typedef struct {
  const char *name;
  int (*run)(const t64c_args *args);
} t64c_case;

static const t64c_case t64c_cases[] = {
    {"smoke", case_smoke_entry},
    {"composite", t64c_case_composite},
};

static bool parse_u64(const char *text, uint64_t *value_out) {
  uint64_t value = 0u;

  if (text == NULL || text[0] == '\0') {
    return false;
  }
  for (; *text != '\0'; text++) {
    if (*text < '0' || *text > '9' || value > (UINT64_MAX - 9u) / 10u) {
      return false;
    }
    value = value * 10u + (uint64_t)(*text - '0');
  }
  *value_out = value;
  return true;
}

int main(int argc, char **argv) {
  t64c_args args = {NULL, false, NULL, 0u};
  const char *name = NULL;
  int i;
  size_t k;

  if (argc == 2 && strcmp(argv[1], "--list-cases") == 0) {
    for (k = 0u; k < sizeof t64c_cases / sizeof t64c_cases[0]; k++) {
      (void)puts(t64c_cases[k].name);
    }
    t64c_ops_list();
    t64c_hostloop_list();
    return 0;
  }
  if (argc >= 3 && strcmp(argv[1], "--case") == 0) {
    name = argv[2];
    for (i = 3; i < argc; i++) {
      if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
        args.rom_path = argv[++i];
      } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
        args.out_dir = argv[++i];
      } else if (strcmp(argv[i], "--print") == 0) {
        args.print = true;
      } else if (strcmp(argv[i], "--slice") == 0 && i + 1 < argc) {
        if (!parse_u64(argv[++i], &args.slice)) {
          (void)fprintf(stderr, "t64c: --slice needs a decimal number\n");
          return 2;
        }
      } else {
        (void)fprintf(stderr, "t64c: unknown option: %s\n", argv[i]);
        return 2;
      }
    }
    for (k = 0u; k < sizeof t64c_cases / sizeof t64c_cases[0]; k++) {
      if (strcmp(name, t64c_cases[k].name) == 0) {
        return t64c_cases[k].run(&args);
      }
    }
    if (t64c_ops_known(name)) {
      return t64c_ops_run(name, &args);
    }
    if (t64c_hostloop_known(name)) {
      return t64c_hostloop_run(name, &args);
    }
    (void)fprintf(stderr, "t64c: unknown case: %s\n", name);
    return 2;
  }
  (void)fprintf(stderr,
                "usage: t64c --list-cases | --case NAME [--rom PATH] [--print] "
                "[--out DIR] [--slice N]\n");
  return 2;
}
