/**
 * LocalDB Test Runner
 */
#include <stdio.h>
#include <string.h>

/* Test function declarations */
extern void test_open_close(void);
extern void test_document(void);
extern void test_sql(void);
extern void test_batch(void);
extern void test_ttl(void);

static int total_pass = 0;
static int total_fail = 0;

void test_assert(int cond, const char *name) {
    if (cond) {
        printf("  PASS: %s\n", name);
        total_pass++;
    } else {
        printf("  FAIL: %s\n", name);
        total_fail++;
    }
}

#define ASSERT(expr) test_assert((expr), #expr)

int main(void) {
    printf("=== LocalDB Test Suite ===\n\n");

    printf("[Open/Close]\n");
    test_open_close();

    printf("\n[Document API]\n");
    test_document();

    printf("\n[SQL]\n");
    test_sql();

    printf("\n[Batch]\n");
    test_batch();

    printf("\n[TTL]\n");
    test_ttl();

    printf("\n=== Results: %d passed, %d failed ===\n", total_pass, total_fail);
    return total_fail > 0 ? 1 : 0;
}
