// Decodes a JPEG to a raw packed-RGB dump used for byte-level regression
// comparisons between the pristine and modified jpgd.
#include "jpgd.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;

    int w = 0, h = 0, comps = 0;
    unsigned char* img = jpgd::decompress_jpeg_image_from_file(
        argv[1], &w, &h, &comps, 3, 0);
    if (!img)
    {
        fprintf(stderr, "decode failed: %s\n", argv[1]);
        return 1;
    }

    FILE* f = fopen(argv[2], "wb");
    if (!f)
        return 1;
    fwrite(img, 1, (size_t)w * h * 3, f);
    fclose(f);
    free(img);
    return 0;
}
