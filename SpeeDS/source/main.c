#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <3ds.h>

u32 reported_size = 0;

time_t start;

Result http_download(const char *url) {
    time(&start);

    Result ret = 0;
    httpcContext context;
    char *newurl = NULL;
    u8 *framebuf_top;
    u32 statuscode = 0;
    u32 contentsize = 0, readsize = 0, size = 0;
    u8 *buf, *lastbuf;


    do {
        ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);

        // This disables SSL cert verification, so https:// will be usable
        ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);

        // Set a User-Agent header so websites can identify your application
        ret = httpcAddRequestHeaderField(&context, "User-Agent", "httpc-example/1.0.0");

        // Tell the server we can support Keep-Alive connections.
        // This will delay connection teardown momentarily (typically 5s)
        // in case there is another request made to the same server.
        ret = httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive");

        ret = httpcBeginRequest(&context);
        if (ret != 0) {
            httpcCloseContext(&context);
            if (newurl != NULL) free(newurl);
            return ret;
        }

        ret = httpcGetResponseStatusCode(&context, &statuscode);
        if (ret != 0) {
            httpcCloseContext(&context);
            if (newurl != NULL) free(newurl);
            return ret;
        }

        if ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308)) {
            if (newurl == NULL) newurl = malloc(0x1000); // One 4K page for new URL
            if (newurl == NULL) {
                httpcCloseContext(&context);
                return -1;
            }
            ret = httpcGetResponseHeader(&context, "Location", newurl, 0x1000);
            url = newurl; // Change pointer to the url that we just learned
            httpcCloseContext(&context); // Close this context before we try the next
        }
    } while ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308));

    if (statuscode != 200) {
        httpcCloseContext(&context);
        if (newurl != NULL) free(newurl);
        return -2;
    }

    // This relies on an optional Content-Length header and may be 0
    ret = httpcGetDownloadSizeState(&context, NULL, &contentsize);
    if (ret != 0) {
        httpcCloseContext(&context);
        if (newurl != NULL) free(newurl);
        return ret;
    }

    // Start with a single page buffer
    buf = (u8 *) malloc(0x1000);
    if (buf == NULL) {
        httpcCloseContext(&context);
        if (newurl != NULL) free(newurl);
        return -1;
    }

    do {
        // This download loop resizes the buffer as data is read.
        ret = httpcDownloadData(&context, buf + size, 0x1000, &readsize);
        size += readsize;
        if (ret == (s32) HTTPC_RESULTCODE_DOWNLOADPENDING) {
            lastbuf = buf; // Save the old pointer, in case realloc() fails.
            buf = realloc(buf, size + 0x1000);
            if (buf == NULL) {
                httpcCloseContext(&context);
                free(lastbuf);
                if (newurl != NULL) free(newurl);
                return -1;
            }
        }
    } while (ret == (s32) HTTPC_RESULTCODE_DOWNLOADPENDING);

    if (ret != 0) {
        httpcCloseContext(&context);
        if (newurl != NULL) free(newurl);
        free(buf);
        return -1;
    }

    // Resize the buffer back down to our actual final size
    lastbuf = buf;
    buf = realloc(buf, size);
    if (buf == NULL) { // realloc() failed.
        httpcCloseContext(&context);
        free(lastbuf);
        if (newurl != NULL) free(newurl);
        return -1;
    }

    if (size > (240 * 400 * 3 * 2))size = 240 * 400 * 3 * 2;

    framebuf_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    memcpy(framebuf_top, buf, size);

    framebuf_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    memcpy(framebuf_top, buf, size);

    gspWaitForVBlank();

    httpcCloseContext(&context);
    free(buf);
    if (newurl != NULL) free(newurl);

    return 0;
}

int main() {
    Result ret = 0;
    gfxInitDefault();
    httpcInit(0); // Buffer size when POST/PUT.

    consoleInit(GFX_BOTTOM, NULL);

    printf("\x1b[4;11HTesting, please wait! \n");

    gfxFlushBuffers();

    ret = http_download("http://mirrors.standaloneinstaller.com/video-sample/page18-movie-4.3gp");
    time_t end;
    time(&end);

    double time_diff = difftime(end, start);
    double speed = 6200 / time_diff;
    double mbps  = speed * 0.008;

    printf ("\x1b[11;16H%f Mbps", mbps);

    printf("\x1b[20;12HPress Start to exit.");

    consoleInit(GFX_TOP, NULL);

    printf ("\x1b[20;19HTest Completed");

    gfxFlushBuffers();

    // Main loop
    while (aptMainLoop()) {
        gspWaitForVBlank();
        hidScanInput();

        // Your code goes here

        u32 kDown = hidKeysDown();
        if (kDown & KEY_START)
            break; // break in order to return to hbmenu
    }

    // Exit services
    httpcExit();
    gfxExit();
    return 0;
}

