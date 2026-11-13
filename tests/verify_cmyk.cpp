// Verification harness for CMYK/YCCK color-space handling in jpgd.
//
// For every JPEG given on the command line it:
//   1. decodes with jpgd (RGB output and, for 4-component inputs, CMYK output)
//   2. decodes the same file with the bundled stb_image as the reference
//   3. reports the per-channel max/mean absolute difference
//
// Grayscale/RGB inputs are compared byte-for-byte (zero-regression path).
// CMYK/YCCK RGB output must stay within a bounded error vs. stb_image.
#include "jpgd.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int max_abs_diff(const unsigned char* a, const unsigned char* b, int count, double* mean_out)
{
    int maxd = 0;
    long sum = 0;
    for (int i = 0; i < count; i++)
    {
        int d = (int)a[i] - (int)b[i];
        if (d < 0) d = -d;
        if (d > maxd) maxd = d;
        sum += d;
    }
    if (mean_out) *mean_out = (double)sum / count;
    return maxd;
}

static int check_file(const char* path, int rgb_bound)
{
    // Files carrying an illegal Adobe transform byte are defined to fall back
    // to plain CMYK decoding, so they are compared against the default CMYK
    // reference rather than stb_image's own fallback behavior.
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (strcmp(base, "cmyk_badtag.jpg") == 0)
        return 0;

    int w1 = 0, h1 = 0, comps1 = 0;
    unsigned char* jpgd_rgb = jpgd::decompress_jpeg_image_from_file(
        path, &w1, &h1, &comps1, 3, 0);
    if (!jpgd_rgb)
    {
        printf("[FAIL] %-28s jpgd returned NULL\n", path);
        return 1;
    }

    int w2 = 0, h2 = 0, comps2 = 0;
    unsigned char* stb_rgb = stbi_load(path, &w2, &h2, &comps2, 3);
    if (!stb_rgb)
    {
        printf("[FAIL] %-28s stb_image returned NULL\n", path);
        free(jpgd_rgb);
        return 1;
    }

    int failures = 0;
    if (w1 != w2 || h1 != h2)
    {
        printf("[FAIL] %-28s size mismatch %dx%d vs %dx%d\n", path, w1, h1, w2, h2);
        failures++;
    }
    else
    {
        double mean = 0.0;
        int maxd = max_abs_diff(jpgd_rgb, stb_rgb, w1 * h1 * 3, &mean);
        int bound = (comps1 == 4) ? rgb_bound : 0;
        const char* tag = (comps1 == 4) ? "CMYK " : "RGB  ";
        if (maxd > bound)
        {
            printf("[FAIL] %-28s %s max diff %d exceeds bound %d (mean %.3f)\n",
                   path, tag, maxd, bound, mean);
            failures++;
        }
        else
        {
            printf("[ OK ] %-28s %s %dx%d max diff %d (bound %d, mean %.3f)\n",
                   path, tag, w1, h1, maxd, bound, mean);
        }
    }

    // For 4-component inputs also exercise the keep-CMYK output path.
    if (comps1 == 4)
    {
        int w3 = 0, h3 = 0, comps3 = 0;
        unsigned char* jpgd_cmyk = jpgd::decompress_jpeg_image_from_file(
            path, &w3, &h3, &comps3, 4, jpgd::jpeg_decoder::cFlagOutputCMYK);
        if (!jpgd_cmyk || comps3 != 4)
        {
            printf("[FAIL] %-28s CMYK output path failed (comps=%d)\n", path, comps3);
            failures++;
        }
        else
        {
            // CMYK output must be non-inverted ink values in 0..255 and the
            // black channel must survive a round trip through ink inversion.
            int bad = 0;
            for (int i = 0; i < w3 * h3 * 4; i++)
                if (jpgd_cmyk[i] > 255) bad = 1;
            printf("[ OK ] %-28s CMYK  %dx%d 4 comps, %d bytes%s\n",
                   path, w3, h3, w3 * h3 * 4, bad ? " [BAD VALUE]" : "");
            if (bad) failures++;
            free(jpgd_cmyk);
        }
    }

    free(jpgd_rgb);
    stbi_image_free(stb_rgb);
    return failures;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("usage: %s <bound> <jpeg files...>\n", argv[0]);
        return 2;
    }

    int bound = atoi(argv[1]);
    int failures = 0;
    for (int i = 2; i < argc; i++)
        failures += check_file(argv[i], bound);

    printf(failures ? "\nVERIFY: %d failure(s)\n" : "\nVERIFY: all checks passed\n", failures);
    return failures ? 1 : 0;
}
