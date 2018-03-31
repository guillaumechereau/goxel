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

#ifndef __UTIL_PARAM_H__
#define __UTIL_PARAM_H__

/* Parameter value lists from OpenImageIO are used to store custom properties
 * on various data, which can then later be used in shaders. */

#ifndef WITHOUT_OPENIMAGEIO

#include <OpenImageIO/paramlist.h>
#include <OpenImageIO/typedesc.h>

CCL_NAMESPACE_BEGIN

OIIO_NAMESPACE_USING

CCL_NAMESPACE_END

#else // WITHOUT_OPENIMAGEIO

#include <string>

CCL_NAMESPACE_BEGIN

class ParamValue {
	public:
};

class TypeDesc : public std::string {
public:
	enum BASETYPE { UNKNOWN, NONE,
					UCHAR, UINT8=UCHAR, CHAR, INT8=CHAR,
					USHORT, UINT16=USHORT, SHORT, INT16=SHORT,
					UINT, UINT32=UINT, INT, INT32=INT,
					ULONGLONG, UINT64=ULONGLONG, LONGLONG, INT64=LONGLONG,
					HALF, FLOAT, DOUBLE, STRING, PTR, LASTBASE };

	TypeDesc() {}
	TypeDesc(std::string name) : std::string(name) { }

	static const TypeDesc TypeColor;
	static const TypeDesc TypeFloat;
	static const TypeDesc TypePoint;
	static const TypeDesc TypeNormal;
	static const TypeDesc TypeVector;
	static const TypeDesc TypeMatrix;
};

CCL_NAMESPACE_END

#endif // WITHOUT_OPENIMAGEIO

#endif /* __UTIL_PARAM_H__ */

