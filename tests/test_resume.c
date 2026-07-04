#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include "resume.h"

static void test_resume_no_file(void)
{
    size_t size = 999;
    unlink("/tmp/test_resume_no_file");
    assert(resume_get_local_size("/tmp/test_resume_no_file", &size) == 0);
    assert(size == 0);
    printf("  test_resume_no_file: PASS\n");
}

static void test_resume_with_file(void)
{
    FILE *fp = fopen("/tmp/test_resume_file", "w");
    assert(fp != NULL);
    fprintf(fp, "hello world");
    fclose(fp);

    size_t size = 0;
    assert(resume_get_local_size("/tmp/test_resume_file", &size) == 0);
    assert(size == 11);

    unlink("/tmp/test_resume_file");
    printf("  test_resume_with_file: PASS\n");
}

int main(void)
{
    printf("test_resume:\n");
    test_resume_no_file();
    test_resume_with_file();
    printf("All resume tests passed.\n\n");
    return 0;
}
