#include <stdio.h>
#include <unistd.h>
#include "fetc.h"
#include "pool.h"

int main(void) {
    printf("--- Manual Compression Test ---\n");
    
    URL u;
    url_parse("http://127.0.0.1/test.txt", &u);
    u.port = 8000; // override port
    
    Downloader dl;
    download_init(&dl);
    
    int rc = download_file(&dl, &u);
    if (rc != 0) {
        printf("Failed: %s\n", dl.error_message);
    } else {
        printf("Success!\n");
    }
    
    download_destroy(&dl);
    pool_shutdown();
    return rc;
}
