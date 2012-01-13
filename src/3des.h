
/* 
 * Cryptographic API.
 *
 * DES & Triple DES EDE Cipher Algorithms.
 *
 * Originally released as descore by Dana L. How <how@isl.stanford.edu>.
 * Modified by Raimar Falke <rf13@inf.tu-dresden.de> for the Linux-Kernel.
 * Derived from Cryptoapi and Nettle implementations, adapted for in-place
 * scatterlist interface.  Changed LGPL to GPL per section 3 of the LGPL.
 *
 * Copyright (c) 1992 Dana L. How.
 * Copyright (c) Raimar Falke <rf13@inf.tu-dresden.de> 
 * Copyright (c) Gisle Slensminde <gisle@ii.uib.no>
 * Copyright (C) 2001 Niels Mller.
 * Copyright (c) 2002 James Morris <jmorris@intercode.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif
void des3_encrypt(char *key, char *data, char *outdata, int length);
void des3_decrypt(char *key, char *data, char *outdata, int length);
#ifdef __cplusplus
}
#endif