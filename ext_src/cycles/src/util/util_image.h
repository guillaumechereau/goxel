/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __UTIL_IMAGE_H__
#define __UTIL_IMAGE_H__

/* OpenImageIO is used for all image file reading and writing. */

#include "util/util_vector.h"

#ifndef WITHOUT_OPENIMAGEIO
#include <OpenImageIO/imageio.h>
namespace IMG = OIIO;
#else

/* Dummy OpenImageIO implementation, just so that the compilation works. */
namespace IMG {

	struct TypeDesc {
		enum BASETYPE { UNKNOWN, UINT8, HALF, FLOAT };
		constexpr TypeDesc (BASETYPE btype=UNKNOWN) {}
		size_t basesize () const { return 0; }
		bool operator== (BASETYPE b) {return false;}
	};

	struct ImageSpec {
		int width;
		int height;
		int depth;
		int nchannels;
		TypeDesc format;
		std::vector<TypeDesc> channelformats;
		ImageSpec() {};
		ImageSpec(int xres, int yres, int nchans,
				  TypeDesc fmt = TypeDesc::UINT8) {}
		std::string get_string_attribute(std::string name) const {
			return std::string();
		}
		void attribute(std::string name, unsigned int value) {}
	};

	typedef ptrdiff_t stride_t;
	const stride_t AutoStride = std::numeric_limits<stride_t>::min();

	struct ImageInput {
		const char *format_name (void) const { return NULL; }
		static ImageInput *create (const std::string &filename) {
			return NULL;
		}
		bool open (const std::string &name, ImageSpec &newspec,
				   const ImageSpec &conf = ImageSpec()) {
			return false;
		}
		bool read_image (TypeDesc format, void *data,
						 stride_t xstride=AutoStride,
						 stride_t ystride=AutoStride,
						 stride_t zstride=AutoStride) {
			return false;
		}
		bool close () { return false; }
	};


	struct ImageOutput {
		static ImageOutput *create (const std::string &filename) {
			return NULL;
		}
		bool open (const std::string &name, const ImageSpec &newspec) {
			return false;
		}
		bool write_image (TypeDesc format, const void *data,
						  stride_t xstride=AutoStride,
						  stride_t ystride=AutoStride,
						  stride_t zstride=AutoStride) {
			return false;
		}
		bool close () { return false; }
	};
};


#endif

CCL_NAMESPACE_BEGIN

using namespace IMG;

template<typename T>
void util_image_resize_pixels(const vector<T>& input_pixels,
                              const size_t input_width,
                              const size_t input_height,
                              const size_t input_depth,
                              const size_t components,
                              vector<T> *output_pixels,
                              size_t *output_width,
                              size_t *output_height,
                              size_t *output_depth);

CCL_NAMESPACE_END

#endif /* __UTIL_IMAGE_H__ */

#include "util/util_image_impl.h"
