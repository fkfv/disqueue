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

#include <openssl/evp.h>
#include "queue-compat.h"

#include <sys/types.h>
#include <sys/stat.h>

struct auth_plaintext_entry {
  LIST_ENTRY(auth_plaintext_entry) next;

  /* verification function depending on codec used - the signature is
     int codec_verify(const char *file, const char *provided), returning a 1
     if the provided password matches the file */
  int (*codec_verify)(const char *, const char *);

  /* stored username and password */
  char *username;
  char *password;
};

/* http basic authentication */
struct auth_plaintext {
  /* source file for passwords */
  char *source;

  /* time the source was last read */
  time_t source_last_read;

  LIST_HEAD(ptehead, auth_plaintext_entry) passwords;
};

struct auth_method_callbacks {
  void *(*auth_new)(void);
  void(*auth_free)(void *);
  int(*auth_verify)(void *, const char *);
  int(*auth_set_password_file)(void *, const char *);
};

struct auth {
  /* auth method */
  enum auth_method method;
  void *methoddata;

  /* callbacks for the auth method */
  struct auth_method_callbacks callbacks;
};

struct auth_plaintext *auth_plaintext_new_(void);
void auth_plaintext_free_(struct auth_plaintext *auth);
int auth_plaintext_verify_(struct auth_plaintext *auth, const char *header);
int auth_plaintext_set_password_file_(struct auth_plaintext *auth,
                                      const char *file);

int auth_plaintext_plain_codec_(const char *file, const char *provided);

int auth_plaintext_update_(struct auth_plaintext *auth);
