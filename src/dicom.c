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
} element_t;

// Start of sequence item.
static const tag_t TAG_ITEM     = {{0xFFFE, 0xE000}};
// End of sequence item.
static const tag_t TAG_ITEM_DEL = {{0xFFFE, 0xE00D}};
// End of Sequence of undefined length.
static const tag_t TAG_SEQ_DEL  = {{0xFFFE, 0xE0DD}};

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
    int i, r;

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
    LOG_V("(%x, %x) %s, length:%d", tag.group, tag.element, vr, length);

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

    // For the moment we just skip the data.
    if (length != 0xffffffff) {
        assert(length >= 0);
        assert(length <= remain_size(in));
        r = fseek(in, length, SEEK_CUR);
        assert(r != -1);
    }

    if (out) {
        out->tag = tag;
        out->length = length;
    }
    if (tag.group == 0) return false;
    return true;
}

void dicom_parse(const char *path)
{
    FILE *in = fopen(path, "rb");
    parse_preamble(in);

    while (true) {
        if (!parse_element(in, 0, NULL)) break;
    }

    fclose(in);
}
