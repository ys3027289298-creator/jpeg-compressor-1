// Standalone unit test for exif_orientation.h. Builds without the rest of the
// project:
//   g++ -std=c++11 -I.. output/test_rotation_unit.cpp -o /tmp/test_rotation

#include "exif_orientation.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <set>

static void test_bijection(int w, int h, int ch, int orientation)
{
	std::vector<unsigned char> px((size_t)w * h * ch);
	// Encode a unique linear index into every pixel. With 3 or 4 channels the
	// full index fits across the bytes; for 1 channel we stamp distinct values
	// and rely on the set check below (images here have <= 255 pixels).
	assert(w * h <= 255);
	for (int i = 0; i < w * h; i++)
	{
		unsigned int v = (unsigned int)i + 1u;
		for (int c = 0; c < ch; c++)
			px[(size_t)i * ch + c] = (unsigned char)((v >> (8 * c)) & 0xFF);
	}

	int rw = w, rh = h;
	exif::apply_orientation(px.data(), rw, rh, ch, orientation);

	int ew, eh;
	exif::oriented_dimensions(orientation, w, h, ew, eh);
	assert(rw == ew && rh == eh);

	// Every source index must appear exactly once: no lost/duplicated pixels.
	std::set<unsigned int> seen;
	for (int i = 0; i < rw * rh; i++)
	{
		unsigned int v = 0;
		for (int c = 0; c < ch; c++)
			v |= (unsigned int)px[(size_t)i * ch + c] << (8 * c);
		assert(v >= 1 && v <= (unsigned int)(w * h));
		assert(seen.insert(v).second);
	}
	assert(seen.size() == (size_t)(w * h));
}

static void test_exif_parser()
{
	// Minimal little-endian Exif APP1 carrying orientation 6.
	const unsigned char jpeg[] = {
		0xFF, 0xD8,
		0xFF, 0xE1, 0x00, 0x24,
		'E', 'x', 'i', 'f', 0x00, 0x00,
		'I', 'I', 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00,
		0x01, 0x00,
		0x12, 0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00,
		0x06, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0xFF, 0xD9
	};
	assert(exif::read_orientation_from_memory(jpeg, sizeof(jpeg)) == 6);

	const unsigned char empty_jpeg[] = { 0xFF, 0xD8, 0xFF, 0xD9 };
	assert(exif::read_orientation_from_memory(empty_jpeg, sizeof(empty_jpeg)) == 1);

	// Garbage and truncated buffers must be treated as normal orientation.
	const unsigned char garbage[] = { 0xFF, 0xD8, 0xFF, 0xE1, 0x00, 0x10, 'x', 'y' };
	assert(exif::read_orientation_from_memory(garbage, sizeof(garbage)) == 1);
	assert(exif::read_orientation_from_memory(NULL, 0) == 1);
}

int main()
{
	test_exif_parser();

	const int dims[][2] = { {1, 1}, {4, 3}, {3, 4}, {16, 15}, {17, 11} };
	for (size_t d = 0; d < sizeof(dims) / sizeof(dims[0]); d++)
		for (int ch = 1; ch <= 4; ch++)
			for (int o = 1; o <= 8; o++)
				test_bijection(dims[d][0], dims[d][1], ch, o);

	// Invalid orientation values must be no-ops.
	std::vector<unsigned char> px(9, 7);
	int w = 3, h = 3;
	exif::apply_orientation(px.data(), w, h, 1, 0);
	exif::apply_orientation(px.data(), w, h, 1, 9);
	assert(w == 3 && h == 3 && px[0] == 7);

	printf("rotation unit tests passed\n");
	return 0;
}
