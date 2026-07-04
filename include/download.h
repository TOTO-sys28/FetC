#ifndef DOWNLOAD_H
#define DOWNLOAD_H

/* Downloader is defined once in fetc.h — this header just re-exposes it
 * for code that only needs the download-related API surface. Do NOT
 * redefine Downloader here; a duplicate definition with mismatched
 * fields (e.g. missing error_message) causes struct-size mismatches
 * and memory corruption if this header is ever included without fetc.h. */
#include "fetc.h"

#endif
