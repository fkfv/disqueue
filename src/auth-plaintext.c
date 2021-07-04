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

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <openssl/crypto.h>
#include <openssl/bio.h>
#include "auth.h"
#include "auth-internal.h"
#include "strcase.h"

/* likely length of the lines - reading from a password file so a short length
   seems reasonable */
#define LINE_SIZE 128

/* extend the string origin with extension, not including EOL or spaces */
char *auth_string_extend(char *original, char *extension,
                         size_t extension_length)
{
  char *extended;

  /* don't include EOL and extra spaces */
  if (extension_length) {
    while (extension_length > 0 && !isgraph(extension[extension_length - 1])) {
      extension_length--;
    }
  }

  if (original) {
    extended = realloc(original, strlen(original) + extension_length + 1);
  } else {
    extended = calloc(1, extension_length + 1);
  }

  if (!extended) {
    if (original) {
      free(original);
    }
    return NULL;
  }

  strncat(extended, extension, extension_length);
  return extended;
}

/* read a line from a file until newline or EOF is reached, dynamically
   allocating memory. the line may be empty */
char *auth_read_line(FILE *fd)
{
  char *line = NULL;
  char buffer[LINE_SIZE];
  fpos_t pos;

  if (fgetpos(fd, &pos) != 0) {
    return NULL;
  }

  while (fgets(buffer, LINE_SIZE, fd) != NULL) {
    /* auth_string_extend cleans up line on error */
    line = auth_string_extend(line, buffer, strlen(buffer));
    if (!line) {
      goto error;
    }

    /* stop if we reach the end of the line */
    if (buffer[strlen(buffer) - 1] == '\n') {
      break;
    }
  }

  /* line is only non null if reading succeeds for at least one character */
  return line;

error:
  fsetpos(fd, &pos);

  if (line) {
    free(line);
  }

  return NULL;
}

const char *auth_skip_basic(const char *header)
{
  /* minimum is Basic and a space character */
  if (strncasecmp(header, "Basic ", 6)) {
    return NULL;
  }
  header += 6;

  /* skip any additional whitespace */
  while (!isgraph(*header)) {
    header++;
  }

  return header;
}

/* decode Basic auth header into username and password */
int auth_plaintext_decode_(const char *header, char **username, char **password)
{
  BIO *mem;
  BIO *base64;
  char *decoded;
  int retval = 0;
  int length;
  char *password_begin;

  /* check for Basic header prefix */
  header = auth_skip_basic(header);
  if (!header) {
    return 0;
  }

  /* base64 decode will always give less data than input but it is easier to
     just allocate the extra memory, and the memory is not kept around */
  decoded = calloc(1, strlen(header) + 1);
  if (!decoded) {
    return 0;
  }

  /* ensure outputs are null */
  *username = NULL;
  *password = NULL;

  mem = BIO_new_mem_buf(header, -1);
  base64 = BIO_new(BIO_f_base64());

  BIO_set_flags(base64, BIO_FLAGS_BASE64_NO_NL);
  base64 = BIO_push(base64, mem);

  /* same as above, easier to keep extra length */
  if ((length = BIO_read(base64, decoded, (int)strlen(header))) <= 0) {
    goto cleanup;
  }
  decoded[length] = '\0';
  BIO_free_all(base64);

  /* try to find the username and password */
  password_begin = strchr(decoded, ':');
  if (!password_begin) {
    goto cleanup;
  }

  /* turn the separator into a null byte */
  password_begin[0] = '\0';
  password_begin++;

  *username = strdup(decoded);
  *password = strdup(password_begin);

  if (*username && *password) {
    retval = 1;
  }

cleanup:
  if (decoded) {
    free(decoded);
  }

  /* cleanup possibly allocated values */
  if (!retval) {
    if (*username) {
      free(*username);
    }

    if (*password) {
      free(*password);
    }
  }

  return retval;
}

struct auth_plaintext *auth_plaintext_new_(void)
{
  struct auth_plaintext *auth;

  auth = calloc(1, sizeof(struct auth_plaintext));
  if (!auth) {
    return NULL;
  }

  LIST_INIT(&auth->passwords);

  return auth;
}

void auth_plaintext_entry_free_(struct auth_plaintext_entry *entry)
{
  LIST_REMOVE(entry, next);
  free(entry->username);
  free(entry->password);
  free(entry);
}

void auth_plaintext_free_(struct auth_plaintext *auth)
{
  struct auth_plaintext_entry *entry;

  while ((entry = LIST_FIRST(&auth->passwords)) != NULL) {
    /* removes self from list */
    auth_plaintext_entry_free_(entry);
  }

  if (auth->source) {
    free(auth->source);
  }
  free(auth);
}

int auth_plaintext_verify_(struct auth_plaintext *auth, const char *header)
{
  struct auth_plaintext_entry *entry;
  char *username;
  char *password;

  if (!auth_plaintext_update_(auth)) {
    return 0;
  }

  if (!auth_plaintext_decode_(header, &username, &password)) {
    return 0;
  }

  entry = LIST_FIRST(&auth->passwords);
  while (entry != NULL) {
    if (!strcasecmp(entry->username, username)) {
      if (entry->codec_verify(entry->password, password)) {
        return 1;
      }
    }

    entry = LIST_NEXT(entry, next);
  }

  return 0;
}

int auth_plaintext_set_password_file_(struct auth_plaintext *auth,
                                      const char *file)
{
  struct auth_plaintext_entry *entry;

  while ((entry = LIST_FIRST(&auth->passwords)) != NULL) {
    /* removes self from list */
    auth_plaintext_entry_free_(entry);
  }

  if (auth->source) {
    free(auth->source);
  }

  auth->source = strdup(file);
  if (!auth->source) {
    return 0;
  }

  return 1;
}

int auth_plaintext_plain_codec_(const char *file, const char *provided)
{
  const char *password_begin;
  size_t password_len;

  /* skip type - we know the type will be valid and equal to plain */
  password_begin = file + strlen("$plain$");
  password_len = strlen(password_begin);

  /* don't compare unless the lengths are the same */
  if (password_len != strlen(provided)) {
    return 0;
  }

  /* always take the same amount of time no matter the length of provided
     password */
  return CRYPTO_memcmp(password_begin, provided, password_len) == 0;
}

int auth_entry_plaintext_add_(struct auth_plaintext *auth,
                              const char *line)
{
  const char *password_begin = NULL;
  const char *salt_begin = NULL;
  size_t password_type_length = 0;
  struct auth_plaintext_entry *entry = NULL;

  /* check line is valid format */
  password_begin = strchr(line, ':');
  if (!password_begin) {
    goto error;
  }
  password_begin++;

  /* check for password format - begins with $ */
  if (password_begin[0] != '$') {
    goto error;
  }

  /* then the type followed by another $ */
  salt_begin = strchr(password_begin + 1, '$');
  if (!salt_begin) {
    goto error;
  }

  /* the type must have a length. if this all succeeds then the password is
     atleast valid enough to bother loading. take off 1 extra for the $ */
  password_type_length = salt_begin - password_begin - 1;
  if (password_type_length == 0) {
    goto error;
  }

  entry = calloc(1, sizeof(struct auth_plaintext_entry));
  if (!entry) {
    goto error;
  }

  /* length includes the separator, which we will use as the null terminator */
  entry->username = calloc(1, password_begin - line);
  if (!entry->username) {
    goto error;
  }
  strncpy(entry->username, line, password_begin - line - 1);

  entry->password = strdup(password_begin);
  if (!entry->password) {
    goto error;
  }

  /* try to figure out which method this password is encoded using. verify the
     length to make sure partial matches are not included */
  if (password_type_length == 5 &&
      !strncasecmp(password_begin + 1, "plain", 5)) {
    entry->codec_verify = auth_plaintext_plain_codec_;
  } else {
    goto error;
  }

  LIST_INSERT_HEAD(&auth->passwords, entry, next);
  return 1;

error:
  if (entry) {
    if (entry->username) {
      free(entry->username);
    }

    if (entry->password) {
      free(entry->password);
    }

    free(entry);
  }

  return 0;
}

int auth_plaintext_update_(struct auth_plaintext *auth)
{
  struct stat file_info;
  FILE *fd = NULL;
  int retval = 0;
  char *line = NULL;

  if (!auth->source || stat(auth->source, &file_info) != 0) {
    if (auth->source_last_read) {
      /* cannot update the file, just use what is already in memory */
      return 1;
    }

    if (auth->source) {
      fprintf(stderr, "cannot access file: %s\n", strerror(errno));
    }

    return 0;
  }

  if (auth->source_last_read) {
    if (file_info.st_mtime <= auth->source_last_read) {
      /* file not modified since last read */
      return 1;
    }
  }

  fd = fopen(auth->source, "r");
  if (!fd) {
    goto cleanup;
  }

  while ((line = auth_read_line(fd)) != NULL) {
    if (!auth_entry_plaintext_add_(auth, line)) {
      goto cleanup;
    }

    free(line);
  }

  /* mark the file as read */
  auth->source_last_read = file_info.st_mtime;
  retval = 1;

cleanup:
  if (fd) {
    fclose(fd);
  }

  if (line) {
    free(line);
  }

  return retval;
}
