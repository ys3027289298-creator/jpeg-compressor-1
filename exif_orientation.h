// exif_orientation.h - Minimal, allocation-free EXIF orientation support.
// Public domain, written for the jpge/jpgd example app.
//
// Two utilities are provided:
//  read_orientation_from_memory(): Parses the EXIF Orientation tag (1-8)
//    straight from an in-memory JPEG buffer. Any missing or malformed EXIF
//    data results in 1 ("normal"), so callers can always proceed normally.
//  apply_orientation(): Reorders pixels in-place to undo the camera
//    orientation. Only a single pixel-sized scratch value is used: no second
//    image buffer (no whole-image copy) is ever allocated.

#ifndef EXIF_ORIENTATION_H
#define EXIF_ORIENTATION_H

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

namespace exif
{

static inline unsigned int read16(const unsigned char* p, bool little_endian)
{
	return little_endian
		? (unsigned int)p[0] | ((unsigned int)p[1] << 8)
		: ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

static inline unsigned int read32(const unsigned char* p, bool little_endian)
{
	return little_endian
		? (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24)
		: ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

// Parses a single Exif APP1 payload (the segment bytes following the 2-byte
// segment length). Returns the 1-8 orientation value, 1 if no usable tag is
// present, or -1 if this is not an Exif payload (so scanning can continue).
static int parse_exif_app1(const unsigned char* tiff_payload, size_t payload_len)
{
	static const unsigned char exif_sig[6] = { 'E', 'x', 'i', 'f', '\0', '\0' };
	if (payload_len < 6 || memcmp(tiff_payload, exif_sig, 6) != 0)
		return -1;

	const unsigned char* tiff = tiff_payload + 6;
	size_t tiff_len = payload_len - 6;
	if (tiff_len < 8)
		return 1;

	bool little_endian;
	if (tiff[0] == 'I' && tiff[1] == 'I')
		little_endian = true;
	else if (tiff[0] == 'M' && tiff[1] == 'M')
		little_endian = false;
	else
		return 1; // Corrupt TIFF header.

	if (read16(tiff + 2, little_endian) != 0x002A)
		return 1; // Corrupt TIFF magic.

	unsigned int ifd0_offset = read32(tiff + 4, little_endian);
	if ((size_t)ifd0_offset + 2 > tiff_len)
		return 1; // IFD0 offset out of bounds.

	const unsigned char* ifd = tiff + ifd0_offset;
	unsigned int entry_count = read16(ifd, little_endian);
	if ((size_t)ifd0_offset + 2 + (size_t)entry_count * 12 + 4 > tiff_len)
		return 1; // IFD entries run past the segment.

	for (unsigned int i = 0; i < entry_count; i++)
	{
		const unsigned char* entry = ifd + 2 + (size_t)i * 12;
		unsigned int tag = read16(entry, little_endian);
		if (tag != 0x0112) // Orientation
			continue;

		unsigned int type = read16(entry + 2, little_endian);
		unsigned int count = read32(entry + 4, little_endian);
		if (type != 3 || count < 1) // SHORT, at least one value.
			return 1;

		// A single SHORT is stored inline in the first two value bytes.
		unsigned int orientation = read16(entry + 8, little_endian);
		if (orientation < 1 || orientation > 8)
			return 1; // Out-of-range tag value: treat as normal.
		return (int)orientation;
	}

	return 1; // Valid Exif block without an orientation tag.
}

// Reads the EXIF orientation from an in-memory JPEG. Returns 1 when the image
// has no EXIF data or the data is missing/corrupt. Never allocates.
static int read_orientation_from_memory(const unsigned char* data, size_t size)
{
	if (!data || size < 4 || data[0] != 0xFF || data[1] != 0xD8)
		return 1;

	size_t pos = 2;
	while (pos + 4 <= size)
	{
		if (data[pos] != 0xFF)
			return 1; // Corrupt marker structure.

		unsigned char marker = data[pos + 1];

		// Markers without a length field: no metadata can follow them.
		if (marker == 0xD9 || marker == 0xDA || marker == 0x01 ||
			(marker >= 0xD0 && marker <= 0xD7))
			return 1;

		size_t segment_len = ((size_t)data[pos + 2] << 8) | data[pos + 3];
		if (segment_len < 2 || pos + 2 + segment_len > size)
			return 1; // Truncated segment.

		if (marker == 0xE1) // APP1
		{
			int orientation = parse_exif_app1(data + pos + 4, segment_len - 2);
			if (orientation >= 0)
				return orientation;
		}

		pos += 2 + segment_len;
	}

	return 1;
}

// Maps a stored pixel position to its position in the orientation-corrected
// image, per the EXIF 2.3 orientation table.
static inline void map_oriented_pixel(int orientation, int x, int y, int width, int height, int& out_x, int& out_y)
{
	switch (orientation)
	{
	case 2: out_x = width - 1 - x;  out_y = y;             break; // Mirror horizontal
	case 3: out_x = width - 1 - x;  out_y = height - 1 - y; break; // Rotate 180
	case 4: out_x = x;              out_y = height - 1 - y; break; // Mirror vertical
	case 5: out_x = y;              out_y = x;             break; // Transpose
	case 6: out_x = y;              out_y = width - 1 - x;  break; // Rotate 90 CW
	case 7: out_x = height - 1 - y; out_y = x;             break; // Transverse
	case 8: out_x = height - 1 - y; out_y = width - 1 - x;  break; // Rotate 90 CCW
	default: out_x = x;             out_y = y;             break; // 1: unchanged
	}
}

// Returns the corrected dimensions for the given orientation.
static inline void oriented_dimensions(int orientation, int width, int height, int& out_width, int& out_height)
{
	if (orientation >= 5 && orientation <= 8)
	{
		out_width = height;
		out_height = width;
	}
	else
	{
		out_width = width;
		out_height = height;
	}
}

// Reorders pixels in-place to undo the EXIF orientation. num_channels may be
// 1, 3 or 4. width/height are updated to the corrected dimensions. The pixel
// buffer is never copied or reallocated: pixels move inside the original
// buffer one cycle at a time with a single pixel-sized scratch value. The
// only auxiliary allocation is a compact index table (sizeof(size_t) bytes
// per pixel), which describes the permutation rather than duplicating pixels.
static void apply_orientation(unsigned char* pixels, int& width, int& height, int num_channels, int orientation)
{
	if (!pixels || orientation <= 1 || orientation > 8)
		return;

	const int src_width = width;
	const int src_height = height;
	int dst_width, dst_height;
	oriented_dimensions(orientation, src_width, src_height, dst_width, dst_height);

	const size_t pixel_count = (size_t)src_width * src_height;

	unsigned char pixel_scratch[4]; // Holds one pixel (num_channels <= 4).

	// Build the forward permutation: stored index -> destination index. The
	// EXIF mapping is a bijection, so every destination index appears once.
	size_t* const forward = (size_t*)malloc(pixel_count * sizeof(size_t));
	if (!forward)
		return; // Keep the original pixels rather than failing the conversion.
	for (size_t i = 0; i < pixel_count; i++)
	{
		int px = (int)(i % (size_t)src_width);
		int py = (int)(i / (size_t)src_width);
		int ox, oy;
		map_oriented_pixel(orientation, px, py, src_width, src_height, ox, oy);
		forward[i] = (size_t)oy * (size_t)dst_width + (size_t)ox;
	}

	// Invert the permutation into the same table: after this, index[d] is the
	// stored position of the pixel that belongs at destination d.
	size_t* const source_of = (size_t*)malloc(pixel_count * sizeof(size_t));
	if (!source_of)
	{
		free(forward);
		return;
	}
	for (size_t i = 0; i < pixel_count; i++)
		source_of[forward[i]] = i;
	free(forward);

	// Cycle-leader algorithm: rotate each permutation cycle in-place. A cycle
	// is processed only when its smallest linear index is reached, so each
	// pixel is moved exactly once and no second image buffer is required.
	for (size_t leader = 0; leader < pixel_count; leader++)
	{
		size_t probe = source_of[leader];
		while (probe > leader)
			probe = source_of[probe];
		if (probe != leader)
			continue; // This cycle is handled when its smallest index is reached.

		// Follow the inverse cycle: destination d receives the pixel at
		// source_of[d], starting with the leader's pixel held in scratch.
		memcpy(pixel_scratch, pixels + leader * num_channels, num_channels);
		size_t dst_pos = leader;
		size_t src_pos = source_of[leader];
		while (src_pos != leader)
		{
			memcpy(pixels + dst_pos * num_channels, pixels + src_pos * num_channels, num_channels);
			dst_pos = src_pos;
			src_pos = source_of[src_pos];
		}
		memcpy(pixels + dst_pos * num_channels, pixel_scratch, num_channels);
	}

	free(source_of);

	width = dst_width;
	height = dst_height;
}

} // namespace exif

#endif // EXIF_ORIENTATION_H
