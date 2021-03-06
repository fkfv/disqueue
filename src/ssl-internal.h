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

#ifndef SSL_INTERNAL_H
#define SSL_INTERNAL_H

#include <openssl/ssl.h>
#include <openssl/safestack.h>

struct ssl_context {
  int ssl_initialised;

  /* has the keypair loaded - 0 indicates none, 1 indicates partial and 2
     indicates fully loaded */
  int keypair_loaded;

  /* openssl context */
  SSL_CTX *ssl_ctx;
};

SSL *ssl_create_connection(void);
struct bufferevent *ssl_create_bufferevent(struct event_base *base, void *user);

extern struct ssl_context ssl_global_context_;

#endif
