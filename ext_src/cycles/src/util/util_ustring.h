/*
 * Copyright 2011-2018 Blender Foundation
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

#ifndef __UTIL_USTRING_H__
#define __UTIL_USTRING_H__

/* ustring class implementation.  Follow std::string, but optimized for
 * comparisons */

#ifndef WITHOUT_OPENIMAGEIO

#include <OpenImageIO/ustring.h>

CCL_NAMESPACE_BEGIN

OIIO_NAMESPACE_USING

CCL_NAMESPACE_END

#else // WITHOUT_OPENIMAGEIO

CCL_NAMESPACE_BEGIN

// ustring implementation based on std::string.
class ustring : public std::string
{
public:
	ustring() {}
	explicit ustring(const char *str) : std::string(str) {}
	ustring (const ustring &str) :std::string(str.c_str()) {}
	ustring (const std::string &str) : std::string(str) {}
	const std::string &string() const { return *this; }
	operator int(void) const { return !empty(); }
	const ustring &operator=(const char *str) { assign(str); return *this; }
};

typedef std::hash<std::string> ustringHash;

CCL_NAMESPACE_END

#endif // WITHOUT_OPENIMAGEIO

#endif /* __UTIL_USTRING_H__ */

