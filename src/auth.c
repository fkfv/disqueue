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

#include "auth.h"
#include "auth-internal.h"
#include "strcase.h"

struct auth_methods {
  const char *name;
  enum auth_method method;
  struct auth_method_callbacks callbacks;
} auth_methods_list[] = {
  {"plaintext", auth_plaintext, {
    auth_plaintext_new_,
    auth_plaintext_free_,
    auth_plaintext_verify_,
    auth_plaintext_set_password_file_
  }},
  {NULL} /* terminator */
};

struct auth *auth_new_(struct auth_methods *method)
{
  struct auth *auth = NULL;

  auth = calloc(1, sizeof(struct auth));
  if (!auth) {
    goto error;
  }

  auth->callbacks = method->callbacks;
  auth->methoddata = auth->callbacks.auth_new();
  if (!auth->methoddata) {
    goto error;
  }

  return auth;

error:
  if (auth) {
    if (auth->methoddata) {
      auth->callbacks.auth_free(auth->methoddata);
    }
    free(auth);
  }

  return NULL;
}

struct auth *auth_method_from_string(const char *method) {
  struct auth_methods *methodsearch;

  methodsearch = auth_methods_list;
  while (methodsearch->name) {
    if (!strcasecmp(methodsearch->name, method)) {
      return auth_new_(methodsearch);
    }

    methodsearch++;
  }

  return NULL;
}

struct auth *auth_new(enum auth_method method)
{
  struct auth_methods *methodsearch;

  methodsearch = auth_methods_list;
  while (methodsearch->name) {
    if (method == methodsearch->method) {
      return auth_new_(methodsearch);
    }

    methodsearch++;
  }

  return NULL;
}

void auth_free(struct auth *auth)
{
  auth->callbacks.auth_free(auth->methoddata);
  free(auth);
}

int auth_verify(struct auth *auth, const char *header)
{
  return auth->callbacks.auth_verify(auth->methoddata, header);
}

int auth_set_password_file(struct auth *auth, const char *file)
{
  if (!auth->callbacks.auth_set_password_file) {
    return 0;
  }

  return auth->callbacks.auth_set_password_file(auth->methoddata, file);
}
