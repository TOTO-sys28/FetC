#include <stdio.h>
#include <stdlib.h>

/* Forward declarations */
int test_url_main(void);
int test_headers_main(void);
int test_request_main(void);
int test_chunked_main(void);
int test_resume_main(void);

/* We compile each test as a separate binary and run them all */
int main(void)
{
    printf("========================================\n");
    printf("FetC Unit Tests\n");
    printf("========================================\n\n");

    int result = 0;

    printf("[1/5] Running URL tests...\n");
    if (system("./tests/test_url") != 0) result = 1;

    printf("[2/5] Running headers tests...\n");
    if (system("./tests/test_headers") != 0) result = 1;

    printf("[3/5] Running request tests...\n");
    if (system("./tests/test_request") != 0) result = 1;

    printf("[4/5] Running chunked tests...\n");
    if (system("./tests/test_chunked") != 0) result = 1;

    printf("[5/5] Running resume tests...\n");
    if (system("./tests/test_resume") != 0) result = 1;

    printf("========================================\n");
    if (result == 0)
        printf("All tests PASSED!\n");
    else
        printf("Some tests FAILED!\n");
    printf("========================================\n");

    return result;
}
