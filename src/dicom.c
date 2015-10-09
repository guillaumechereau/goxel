/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "goxel.h"

// Very basic implementation of DICOM files reader.

// Convenience macro.
#define READ(type, in) ({type v; fread(&v, sizeof(type), 1, in); v;})

typedef union {
    struct {
        uint16_t group;
        uint16_t element;
    };
    uint32_t v;
} tag_t;

typedef struct {
    tag_t tag;
    int   length;
    char  vr[2];

    union {
        uint16_t us;
    } value;

    // If set they we put the pixel data there.
    char  *buffer;
    int   buffer_size;

} element_t;

// Start of sequence item.
static const tag_t TAG_ITEM                             = {{0xFFFE, 0xE000}};
// End of sequence item.
static const tag_t TAG_ITEM_DEL                         = {{0xFFFE, 0xE00D}};
// End of Sequence of undefined length.
static const tag_t TAG_SEQ_DEL                          = {{0xFFFE, 0xE0DD}};

static const tag_t TAG_SAMPLES_PER_PIXEL                = {{0x0028, 0x0002}};
static const tag_t TAG_PHOTOMETRIC_INTERPRETATION       = {{0x0028, 0x0004}};
static const tag_t TAG_ROWS                             = {{0x0028, 0x0010}};
static const tag_t TAG_COLUMNS                          = {{0x0028, 0x0011}};
static const tag_t TAG_BITS_ALLOCATED                   = {{0x0028, 0x0100}};
static const tag_t TAG_BITS_STORED                      = {{0x0028, 0x0101}};
static const tag_t TAG_HIGH_BIT                         = {{0x0028, 0x0102}};
static const tag_t TAG_PIXEL_REPRESENTATION             = {{0x0028, 0x0103}};
static const tag_t TAG_PIXEL_DATA                       = {{0x7FE0, 0x0010}};


static const char EXTRA_LEN_VRS[][2] = {"OB", "OW", "OF", "SQ", "UN", "UT"};


static void parse_preamble(FILE *in)
{
    char magic[4];
    // Skip the first 128 bytes.
    fseek(in, 128, SEEK_CUR);
    fread(magic, 4, 1, in);
    assert(strncmp(magic, "DICM", 4) == 0);
}

static int remain_size(FILE *in)
{
    long cur;
    int end;
    cur = ftell(in);
    fseek(in, 0, SEEK_END);
    end = ftell(in);
    fseek(in, cur, SEEK_SET);
    return end - cur;
}

enum {
    STATE_SEQUENCE_ITEM = 1 << 0,
};

static bool parse_element(FILE *in, int state, element_t *out)
{
    tag_t tag;
    element_t item;
    int length;
    char vr[3] = "  ";
    bool extra_len = state & STATE_SEQUENCE_ITEM;
    int i;

    tag.group = READ(uint16_t, in);
    tag.element  = READ(uint16_t, in);

    if (!(state & STATE_SEQUENCE_ITEM)) {
        fread(vr, 2, 1, in);
        length = READ(uint16_t, in);
        for (i = 0; i < ARRAY_SIZE(EXTRA_LEN_VRS); i++) {
            if (strncmp(vr, EXTRA_LEN_VRS[i], 2) == 0) {
                extra_len = true;
                break;
            }
        }
    }
    if (extra_len) length = READ(uint32_t, in);
    LOG_V("(%.4x, %.4x) %s, length:%d", tag.group, tag.element, vr, length);

    // Read a sequence of undefined length.
    if (length == 0xffffffff && strncmp(vr, "SQ", 2) == 0) {
        while (true) {
            parse_element(in, STATE_SEQUENCE_ITEM, &item);
            if (item.tag.v == TAG_SEQ_DEL.v) {
                break;
            }
            if (item.tag.v != TAG_ITEM.v)
                LOG_E("Expected item tag");
        }
    }
    if (state & STATE_SEQUENCE_ITEM && length == 0xffffffff) {
        while (true) {
            parse_element(in, 0, &item);
            if (item.tag.v == TAG_ITEM_DEL.v) {
                break;
            }
        }
    }

    if (out) {
        out->tag = tag;
        out->length = length;
        memcpy(out->vr, vr, 2);
    }

    // For the moment we just skip the data.
    if (length != 0xffffffff) {
        assert(length >= 0);
        assert(length <= remain_size(in));
        if (out && length == 2 && strncmp(vr, "US", 2) == 0) {
            out->value.us = READ(uint16_t, in);
        } else if (out && tag.v == TAG_PIXEL_DATA.v && out->buffer) {
            assert(out->buffer_size >= length);
            fread(out->buffer, length, 1, in);
        } else {
            // Skip the data.
            fseek(in, length, SEEK_CUR);
        }
    }

    if (tag.group == 0) return false;
    return true;
}

void dicom_parse(const char *path, dicom_t *dicom,
                 char *out_buffer, int buffer_size)
{
    FILE *in = fopen(path, "rb");
    parse_preamble(in);
    element_t element = {
        .buffer = out_buffer,
        .buffer_size = buffer_size,
    };

    while (true) {
        if (!parse_element(in, 0, &element)) break;

        #define S(attr, tag_) \
            if (element.tag.v == tag_.v) \
                dicom->attr = element.value.us;

        S(samples_per_pixel,    TAG_SAMPLES_PER_PIXEL);
        S(rows,                 TAG_ROWS);
        S(columns,              TAG_COLUMNS);
        S(bits_allocated,       TAG_BITS_ALLOCATED);
        S(bits_stored,          TAG_BITS_STORED);
        S(high_bit,             TAG_HIGH_BIT);

        #undef S

        if (element.tag.v == TAG_PIXEL_DATA.v)
            dicom->data_size = element.length;
    }

    fclose(in);
}
