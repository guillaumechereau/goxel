
/* vim: set et ts=3 sw=3 sts=3 ft=c:
 *
 * Copyright (C) 2014 James McLaughlin.  All rights reserved.
 * https://github.com/udp/json-builder
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _JSON_BUILDER_H
#define _JSON_BUILDER_H

/* Requires json.h from json-parser
 * https://github.com/udp/json-parser
 */
#include "json.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* IMPORTANT NOTE:  If you want to use json-builder functions with values
 * allocated by json-parser as part of the parsing process, you must pass
 * json_builder_extra as the value_extra setting in json_settings when
 * parsing.  Otherwise there will not be room for the extra state and
 * json-builder WILL invoke undefined behaviour.
 *
 * Also note that unlike json-parser, json-builder does not currently support
 * custom allocators (for no particular reason other than that it doesn't have
 * any settings or global state.)
 */
extern const size_t json_builder_extra;


/*** Arrays
 ***
 * Note that all of these length arguments are just a hint to allow for
 * pre-allocation - passing 0 is fine.
 */
json_value * json_array_new (size_t length);
json_value * json_array_push (json_value * array, json_value *);


/*** Objects
 ***/
json_value * json_object_new (size_t length);

json_value * json_object_push (json_value * object,
                               const json_char * name,
                               json_value *);

/* Same as json_object_push, but doesn't call strlen() for you.
 */
json_value * json_object_push_length (json_value * object,
                                      unsigned int name_length, const json_char * name,
                                      json_value *);

/* Same as json_object_push_length, but doesn't copy the name buffer before
 * storing it in the value.  Use this micro-optimisation at your own risk.
 */
json_value * json_object_push_nocopy (json_value * object,
                                      unsigned int name_length, json_char * name,
                                      json_value *);

/* Merges all entries from objectB into objectA and destroys objectB.
 */
json_value * json_object_merge (json_value * objectA, json_value * objectB);

/* Sort the entries of an object based on the order in a prototype object.
 * Helpful when reading JSON and writing it again to preserve user order.
 */
void json_object_sort (json_value * object, json_value * proto);



/*** Strings
 ***/
json_value * json_string_new (const json_char *);
json_value * json_string_new_length (unsigned int length, const json_char *);
json_value * json_string_new_nocopy (unsigned int length, json_char *);


/*** Everything else
 ***/
json_value * json_integer_new (json_int_t);
json_value * json_double_new (double);
json_value * json_boolean_new (int);
json_value * json_null_new ();


/*** Serializing
 ***/
#define json_serialize_mode_multiline     0
#define json_serialize_mode_single_line   1
#define json_serialize_mode_packed        2

#define json_serialize_opt_CRLF                    (1 << 1)
#define json_serialize_opt_pack_brackets           (1 << 2)
#define json_serialize_opt_no_space_after_comma    (1 << 3)
#define json_serialize_opt_no_space_after_colon    (1 << 4)
#define json_serialize_opt_use_tabs                (1 << 5)

typedef struct json_serialize_opts
{
   int mode;
   int opts;
   int indent_size;

} json_serialize_opts;


/* Returns a length in characters that is at least large enough to hold the
 * value in its serialized form, including a null terminator.
 */
size_t json_measure (json_value *);
size_t json_measure_ex (json_value *, json_serialize_opts);


/* Serializes a JSON value into the buffer given (which must already be
 * allocated with a length of at least json_measure(value, opts))
 */
void json_serialize (json_char * buf, json_value *);
void json_serialize_ex (json_char * buf, json_value *, json_serialize_opts);


/*** Cleaning up
 ***/
void json_builder_free (json_value *);

#ifdef __cplusplus
}
#endif

#endif



