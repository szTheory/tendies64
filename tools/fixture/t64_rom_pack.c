#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define T64_ROM_LIMIT_BYTES ((size_t)1048576)
#define T64_FLAT_PAYLOAD_OFFSET ((size_t)4096)

static bool flat_payload_fits(size_t payload_bytes)
{
    (void)payload_bytes;
    return false;
}

static void report_test(unsigned number, const char *name, bool passed)
{
    printf("%s %u - %s\n", passed ? "ok" : "not ok", number, name);
}

static int run_selftest(void)
{
    unsigned failures = 0;
    const size_t max_payload = T64_ROM_LIMIT_BYTES - T64_FLAT_PAYLOAD_OFFSET;

    puts("TAP version 13");
    puts("1..4");

    bool valid_payload_accepted = flat_payload_fits(4096);
    report_test(1, "flat payload range accepts valid payload", valid_payload_accepted);
    failures += valid_payload_accepted ? 0u : 1u;

    bool zero_payload_rejected = !flat_payload_fits(0);
    report_test(2, "flat payload range rejects zero bytes", zero_payload_rejected);
    failures += zero_payload_rejected ? 0u : 1u;

    bool maximum_payload_accepted = flat_payload_fits(max_payload);
    report_test(3, "flat payload range accepts exact cartridge limit", maximum_payload_accepted);
    failures += maximum_payload_accepted ? 0u : 1u;

    bool oversized_payload_rejected = !flat_payload_fits(max_payload + 1);
    report_test(4, "flat payload range rejects cartridge overflow", oversized_payload_rejected);
    failures += oversized_payload_rejected ? 0u : 1u;

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--selftest") == 0) {
        return run_selftest();
    }
    fputs("usage: t64_rom_pack --selftest\n", stderr);
    return EXIT_FAILURE;
}
