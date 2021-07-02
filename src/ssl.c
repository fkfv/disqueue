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

#include <event2/bufferevent_ssl.h>
#include <openssl/err.h>
#include "ssl.h"
#include "ssl-internal.h"

struct ssl_context ssl_global_context_ = {0};

int ssl_setup(void)
{
  if (ssl_global_context_.ssl_initialised) {
    return 1;
  }

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  SSL_library_init();
  ERR_load_crypto_strings();
  SSL_load_error_strings();
  OPENSSL_add_all_algorithms();
#endif

  ssl_global_context_.ssl_ctx = SSL_CTX_new(SSLv23_server_method());
  if (!ssl_global_context_.ssl_ctx) {
    return 0;
  }

  ssl_global_context_.ssl_initialised = 1;
  ssl_global_context_.keypair_loaded = 0;

  return 1;
}

void ssl_destroy(void)
{
  if (ssl_global_context_.ssl_initialised) {
    SSL_CTX_free(ssl_global_context_.ssl_ctx);
    ssl_global_context_.ssl_initialised = 0;
    ssl_global_context_.keypair_loaded = 0;
  }
}

int ssl_load_certificate(const char *filename)
{
  if (ssl_global_context_.keypair_loaded) {
    return 1;
  }

  if (SSL_CTX_use_certificate_file(ssl_global_context_.ssl_ctx, filename,
                                   SSL_FILETYPE_PEM) <= 0) {
    ERR_print_errors_fp(stderr);
    return 0;
  }

  ssl_global_context_.keypair_loaded++;
  return 1;
}

int ssl_load_privatekey(const char *filename)
{
  if (ssl_global_context_.keypair_loaded == 2) {
    return 1;
  }

  if (SSL_CTX_use_PrivateKey_file(ssl_global_context_.ssl_ctx, filename,
                                  SSL_FILETYPE_PEM) <= 0) {
    ERR_print_errors_fp(stderr);
    return 0;
  }

  ssl_global_context_.keypair_loaded++;
  return 1;
}

void ssl_use(struct evhttp *http)
{
  evhttp_set_bevcb(http, ssl_create_bufferevent, NULL);
}

SSL *ssl_create_connection(void)
{
  SSL *ssl;

  if (!ssl_global_context_.ssl_initialised ||
      ssl_global_context_.keypair_loaded < 2) {
    return NULL;
  }

  ssl = SSL_new(ssl_global_context_.ssl_ctx);
  if (!ssl) {
    return NULL;
  }

  return ssl;
}

struct bufferevent *ssl_create_bufferevent(struct event_base *base, void *user)
{
  SSL *ssl;

  ssl = ssl_create_connection();
  if (!ssl) {
    return NULL;
  }

  return bufferevent_openssl_socket_new(base, -1, ssl,
                                        BUFFEREVENT_SSL_ACCEPTING,
                                        BEV_OPT_CLOSE_ON_FREE);
}
