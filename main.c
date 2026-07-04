#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fetc.h"
#include "transport.h"

static struct timespec start_time;

static void format_bytes(size_t bytes, char *out, size_t out_len)
{
    if (bytes < 1024)
        snprintf(out, out_len, "%zu B", bytes);
    else if (bytes < 1024 * 1024)
        snprintf(out, out_len, "%.1f KB", bytes / 1024.0);
    else if (bytes < 1024ULL * 1024 * 1024)
        snprintf(out, out_len, "%.1f MB", bytes / (1024.0 * 1024));
    else
        snprintf(out, out_len, "%.1f GB", bytes / (1024.0 * 1024 * 1024));
}

static void format_speed(double bps, char *out, size_t out_len)
{
    if (bps < 1024)
        snprintf(out, out_len, "%.0f B/s", bps);
    else if (bps < 1024 * 1024)
        snprintf(out, out_len, "%.1f KB/s", bps / 1024.0);
    else
        snprintf(out, out_len, "%.1f MB/s", bps / (1024.0 * 1024));
}

static void progress_cb(size_t current, size_t total, void *ud)
{
    (void)ud;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    double elapsed = (now.tv_sec - start_time.tv_sec) +
                     (now.tv_nsec - start_time.tv_nsec) / 1e9;
    double speed = elapsed > 0 ? current / elapsed : 0;

    int width = 30;
    int filled = 0;

    if (total > 0) {
        filled = (int)((double)current / total * width);
        if (filled > width) filled = width;
    } else {
        filled = (int)((current / (256 * 1024)) % width);
    }

    char bar[31];
    for (int i = 0; i < width; i++) {
        if (i < filled) bar[i] = '=';
        else if (i == filled && total > 0) bar[i] = '>';
        else bar[i] = ' ';
    }
    bar[width] = '\0';

    char cur_str[32], tot_str[32], spd_str[32];
    format_bytes(current, cur_str, sizeof(cur_str));
    format_speed(speed, spd_str, sizeof(spd_str));

    if (total > 0) {
        format_bytes(total, tot_str, sizeof(tot_str));
        double pct = total > 0 ? (double)current / total * 100.0 : 0;
        double eta = speed > 0 ? (total - current) / speed : 0;
        int eta_sec = (int)eta;

        printf("\r[%s] %.1f%%  %s / %s  %s  ETA %ds", bar, pct, cur_str, tot_str, spd_str, eta_sec);
    } else {
        printf("\r[%s] %s  %s (unknown size)", bar, cur_str, spd_str);
    }
    fflush(stdout);
}

static void usage(const char *prog)
{
    printf("Usage: %s [options] <URL>\n", prog);
    printf("Options:\n");
    printf("  --info              Show file info without downloading\n");
    printf("  --segments <N>      Use N parallel segments (default: 4)\n");
    printf("  --insecure          Disable TLS certificate verification (DANGEROUS!)\n");
    printf("  -v, --verbose       Enable verbose output\n");
    printf("  -h, --help          Show this help\n");
}

int main(int argc, char *argv[])
{
    int info_mode = 0;
    int segments = 0;  /* 0 = single-threaded */
    int insecure_mode = 0;
    int verbose_mode = 0;
    const char *url_str = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--info") == 0) {
            info_mode = 1;
        } else if (strcmp(argv[i], "--segments") == 0) {
            if (i + 1 < argc) {
                segments = atoi(argv[++i]);
                if (segments < 1) segments = 1;
            }
        } else if (strcmp(argv[i], "--insecure") == 0) {
            insecure_mode = 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose_mode = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            url_str = argv[i];
        }
    }

    if (!url_str) {
        usage(argv[0]);
        return 1;
    }

    if (insecure_mode) {
        fprintf(stderr, "WARNING: --insecure mode enabled. TLS certificate verification is DISABLED.\n");
        fprintf(stderr, "This leaves you vulnerable to man-in-the-middle attacks!\n");
        transport_set_insecure(1);
    }

    if (verbose_mode) {
        transport_set_verbose(1);
        segmented_set_verbose(1);
    }

    URL url;
    if (url_parse(url_str, &url) != 0) {
        printf("Parse failed\n");
        return 1;
    }

    if (info_mode) {
        DownloadInfo info;
        if (download_info(&url, &info) != 0) {
            printf("Failed to get info\n");
            return 1;
        }
        printf("Status: %d\n", info.status_code);
        printf("Content-Length: %zu\n", info.content_length);
        printf("Accept-Ranges: %s\n", info.accept_ranges ? "yes" : "no");
        printf("Transfer-Encoding: %s\n", info.chunked ? "chunked" : "none");
        printf("Content-Type: %s\n", info.content_type[0] ? info.content_type : "unknown");
        printf("Last-Modified: %s\n", info.last_modified[0] ? info.last_modified : "unknown");
        return 0;
    }

    printf("URL: %s\nHost: %s\nPath: %s\nPort: %d\n\n",
           url_str, url.host, url.path, url.port);

    clock_gettime(CLOCK_MONOTONIC, &start_time);

    if (segments > 1) {
        SegmentedDownloader sd;
        segmented_download_init(&sd);
        sd.segments = segments;
        sd.on_progress = progress_cb;

        if (segmented_download_file(&sd, &url) != 0) {
            if (sd.error_message[0]) {
                printf("\nDownload failed: %s\n", sd.error_message);
            } else {
                printf("\nDownload failed\n");
            }
            return 1;
        }
    } else {
        Downloader dl;
        download_init(&dl);
        dl.on_progress = progress_cb;

        if (download_file(&dl, &url) != 0) {
            if (dl.error_message[0]) {
                printf("\nDownload failed: %s\n", dl.error_message);
            } else {
                printf("\nDownload failed\n");
            }
            download_destroy(&dl);
            return 1;
        }
        download_destroy(&dl);
    }

    printf("\n\nDone.\n");
    return 0;
}
