/*
  Copyright (c) 2021 Matthew (fkfv).

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include <stdio.h>
#include <limits.h>
#include <json-c/json_tokener.h>
#include <json-c/json_pointer.h>
#include <json-c/linkhash.h>
#include "config.h"
#include "config-internal.h"
#include "strcase.h"

enum config_default_type {
  config_int,
  config_short,
  config_string
};

struct config_default {
  const char *key;
  enum config_default_type type;
  union {
    const char *string;
    int _int;
    short _short;
  } value;
} config_defaults[] = {
  {"/port", config_short, {._short = 3682}},
  {"/host", config_string, {.string="127.0.0.1"}},
  {NULL, config_int, {._int = 0}}
};

/* a buffer size that is reasonable to use on stack and likely to include all
   of the configuration. must be less than INT_MAX */
#define CONFIG_LIKELY_LENGTH 4096

struct json_object *config_object = NULL;

struct config_default *config_get_default(const char *name,
                                          enum config_default_type type)
{
  struct config_default *default_value = config_defaults;

  while (default_value->key) {
    if (!strcasecmp(default_value->key, name)) {
      if (default_value->type != type) {
        return NULL;
      } else {
        return default_value;
      }
    }

    default_value++;
  }

  return NULL;
}

void config_reset(void)
{
  json_object_put(config_object);
  config_object = NULL;
}

int config_parse(const char *filename)
{
  FILE *fd = NULL;
  struct json_tokener *tokener = NULL;
  struct json_object *object;
  enum json_tokener_error error = json_tokener_continue;
  char buf[CONFIG_LIKELY_LENGTH];
  int retval = 0;
  size_t readsize = 0;

  fd = fopen(filename, "r");
  if (!fd) {
    goto cleanup;
  }

  tokener = json_tokener_new();
  if (!tokener) {
    goto cleanup;
  }

  do {
    if ((readsize = fread(buf, 1, CONFIG_LIKELY_LENGTH, fd)) <= 0) {
      /* break - error will be json_tokener_continue if we still excpected more
         json data */
      break;
    }

    if (ferror(fd)) {
      /* break - cannot read the file, error will be json_tokener_continue from
         the initialisation */
      break;
    }

    object = json_tokener_parse_ex(tokener, buf, (int)readsize);
  } while ((error = json_tokener_get_error(tokener)) == json_tokener_continue);

  if (error != json_tokener_success) {
    goto cleanup;
  }

  /* replace existing configuration with new values */
  if (config_object != NULL) {
    config_merge_objects_(config_object, object);
  } else {
    config_object = object;
  }

  /* successfully loaded configuration */
  retval = 1;

cleanup:
  if (fd) {
    fclose(fd);
  }

  if (tokener) {
    json_tokener_free(tokener);
  }

  return retval;
}

int config_load_defaults(void)
{
  struct config_default *default_value;

  if (!config_object) {
    config_object = json_object_new_object();
    if (!config_object) {
      return 0;
    }
  }

  default_value = config_defaults;
  while (default_value->key) {
    /* only add the default if the key does not exist */
    if (json_pointer_get(config_object, default_value->key, NULL) &&
        errno == ENOENT) {
      switch (default_value->type) {
      case config_string:
        if (!config_set_string(default_value->key,
                               default_value->value.string)) {
          return 0;
        }
        break;
      case config_int:
        if (!config_set_int(default_value->key,
                            default_value->value._int)) {
          return 0;
        }
        break;
      case config_short:
        if (!config_set_short(default_value->key,
                              default_value->value._short)) {
          return 0;
        }
        break;
      }
    }

    default_value++;
  }

  return 1;
}

int config_set_string(const char *option, const char *value)
{
  struct json_object *string;

  string = json_object_new_string(value);
  if (!string) {
    return 0;
  }

  return config_set_(option, string);
}

int config_set_int(const char *option, int value)
{
  struct json_object *integer;

  integer = json_object_new_int(value);
  if (!integer) {
    return 0;
  }

  return config_set_(option, integer);
}

int config_set_short(const char *option, short value)
{
  return config_set_int(option, (int)value);
}

const char *config_get_string(const char *name)
{
  struct json_object *string;
  struct config_default *default_value;

  string = config_get_(name);
  if (!string) {
    default_value = config_get_default(name, config_string);
    if (!default_value) {
      return NULL;
    }

    return default_value->value.string;
  }

  return json_object_get_string(string);
}

int config_get_int(const char *name)
{
  struct json_object *integer;
  struct config_default *default_value;

  /* json-c can coerce types, so we will let it try to do so. it fails by
     setting the integer to zero, which might be different from default_value
     but we can still return default_value if the key does not exist */
  integer = config_get_(name);
  if (!integer) {
    default_value = config_get_default(name, config_int);
    if (!default_value) {
      return 0;
    }

    return default_value->value._int;
  }

  return json_object_get_int(integer);
}

short config_get_short(const char *name)
{
  struct json_object *integer;
  struct config_default *default_value;
  int value;

  /* get the type as an integer, and place it in the bounds of short */
  integer = config_get_(name);
  if (!integer) {
    default_value = config_get_default(name, config_short);
    if (!default_value) {
      return (short)0;
    }

    return default_value->value._short;
  }

  value = config_get_int(name);
  if (value > SHRT_MAX) {
    value = SHRT_MAX;
  } else if (value < SHRT_MIN) {
    value = SHRT_MIN;
  }

  return value;
}

int config_merge_objects_(struct json_object *first,
                          struct json_object *second)
{
  int retval = 0;

  /* verify that the objects are valid for combining */
  if (json_object_get_object(second) == NULL &&
      json_object_get_type(first) == json_type_object) {
    goto cleanup;
  }

  /* add each key. increase the refcount, this leads to all of the keys from
     the second object being owned by the first while still being able to
     destroy the base object without going to the trouble of removing its
     keys */
  json_object_object_foreach(second, key, value) {
    if (json_object_object_add(first, key, value) != 0) {
      break;
    }
    json_object_get(value);
  }

  /* successfully combined */
  retval = 1;

cleanup:
  json_object_put(second);

  return retval;
}

int config_set_(const char *key, struct json_object *value)
{
  return json_pointer_set(&config_object, key, value) == 0;
}

struct json_object *config_get_(const char *key)
{
  struct json_object *object;

  if (json_pointer_get(config_object, key, &object) != 0) {
    return NULL;
  }

  return object;
}
