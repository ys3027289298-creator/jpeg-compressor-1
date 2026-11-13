// Illegal/missing color-space markers must degrade gracefully instead of
// crashing. A 4-component JPEG with an out-of-range Adobe transform byte is
// expected to decode exactly like a default (transform = 0, plain CMYK) file.
#include "jpgd.h"

#include <stdio.h>
#include <stdlib.h>

static unsigned char* load_file(const char* path, int* size_out)
{
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    int size = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* buf = (unsigned char*)malloc(size);
    if (fread(buf, 1, size, f) != (size_t)size)
    {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size_out = size;
    return buf;
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printf("usage: %s <cmyk.jpg> <cmyk_badtag.jpg>\n", argv[0]);
        return 2;
    }

    int failures = 0;

    for (int i = 1; i <= 2; i++)
    {
        int size = 0;
        unsigned char* buf = load_file(argv[i], &size);
        if (!buf)
        {
            printf("[FAIL] cannot read %s\n", argv[i]);
            return 1;
        }

        int w = 0, h = 0, comps = 0;
        unsigned char* img = jpgd::decompress_jpeg_image_from_memory(
            buf, size, &w, &h, &comps, 3, 0);
        free(buf);

        if (!img)
        {
            printf("[FAIL] %s rejected instead of degrading\n", argv[i]);
            failures++;
            continue;
        }
        printf("[ OK ] %s decoded %dx%d comps=%d\n", argv[i], w, h, comps);
        free(img);
    }

    // Both files must produce identical pixels (illegal tag falls back to CMYK).
    int w_a = 0, h_a = 0, c_a = 0, w_b = 0, h_b = 0, c_b = 0;
    unsigned char* a = jpgd::decompress_jpeg_image_from_file(argv[1], &w_a, &h_a, &c_a, 3, 0);
    unsigned char* b = jpgd::decompress_jpeg_image_from_file(argv[2], &w_b, &h_b, &c_b, 3, 0);
    if (a && b && w_a == w_b && h_a == h_b)
    {
        int n = w_a * h_a * 3;
        int diff = 0;
        for (int i = 0; i < n; i++)
            if (a[i] != b[i]) diff++;
        if (diff)
        {
            printf("[FAIL] illegal tag changed %d pixels vs. default CMYK\n", diff);
            failures++;
        }
        else
            printf("[ OK ] illegal tag degrades to identical CMYK output\n");
    }
    else
    {
        printf("[FAIL] could not compare degraded outputs\n");
        failures++;
    }
    free(a);
    free(b);

    printf(failures ? "\nDEGRADE: %d failure(s)\n" : "\nDEGRADE: all checks passed\n", failures);
    return failures ? 1 : 0;
}
