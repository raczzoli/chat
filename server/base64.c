/***********************************************************
* Base64 library implementation                            *
* @author Ahmed Elzoughby                                  *
* @date July 23, 2017                                      *
***********************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

#include "base64.h"


char* base64_encode(const unsigned char *input, size_t len) 
{
    BIO *bio, *b64;
    BUF_MEM *buffer_ptr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bio);

    // Do not add newlines
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    // Write data to BIO chain
    BIO_write(b64, input, len);
    BIO_flush(b64);

    // Extract encoded data
    BIO_get_mem_ptr(b64, &buffer_ptr);

    // Allocate output buffer with null terminator
    char *b64text = (char *)malloc(buffer_ptr->length + 1);
    memcpy(b64text, buffer_ptr->data, buffer_ptr->length);
    b64text[buffer_ptr->length] = '\0';

    BIO_free_all(b64);

    return b64text;
}