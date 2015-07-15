/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2015 Intel Corporation.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  Contact Information:
  Intel Corporation, www.intel.com

  BSD LICENSE

  Copyright(c) 2015 Intel Corporation.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/* Copyright (c) 2003-2014 Intel Corporation. All rights reserved. */

#include <stdint.h>

#if defined(__x86_64__)
#define hfi_dwordcpy hfi_dwordcpy_safe
#define hfi_qwordcpy hfi_qwordcpy_safe
#endif

void hfi_dwordcpy(uint32_t *dest, const uint32_t *src, uint32_t ndwords)
{
	uint_fast32_t ndw = ndwords;
	uint64_t *src64[4];
	uint64_t *dst64[4];
	src64[0] = (uint64_t *) src;
	dst64[0] = (uint64_t *) dest;

	while (ndw >= 8) {
		*dst64[0] = *src64[0];
		src64[1] = src64[0] + 1;
		src64[2] = src64[0] + 2;
		src64[3] = src64[0] + 3;
		ndw -= 8;
		dst64[1] = dst64[0] + 1;
		dst64[2] = dst64[0] + 2;
		dst64[3] = dst64[0] + 3;
		*dst64[1] = *src64[1];
		*dst64[2] = *src64[2];
		*dst64[3] = *src64[3];
		src64[0] += 4;
		dst64[0] += 4;
	}
	if (ndw) {
		src = (uint32_t *) src64[0];
		dest = (uint32_t *) dst64[0];

		switch (ndw) {
		case 7:
			*dest++ = *src++;
		case 6:
			*dest++ = *src++;
		case 5:
			*dest++ = *src++;
		case 4:
			*dest++ = *src++;
		case 3:
			*dest++ = *src++;
		case 2:
			*dest++ = *src++;
		case 1:
			*dest++ = *src++;
		}

	}
}

void hfi_qwordcpy(uint64_t *dest, const uint64_t *src, uint32_t nqwords)
{
	uint_fast32_t nqw = nqwords;
	const uint64_t *src64[4];
	uint64_t *dst64[4];
	src64[0] = src;
	dst64[0] = dest;

	while (nqw >= 8) {
		*dst64[0] = *src64[0];
		src64[1] = src64[0] + 1;
		src64[2] = src64[0] + 2;
		src64[3] = src64[0] + 3;
		dst64[1] = dst64[0] + 1;
		dst64[2] = dst64[0] + 2;
		dst64[3] = dst64[0] + 3;
		*dst64[1] = *src64[1];
		*dst64[2] = *src64[2];
		*dst64[3] = *src64[3];
		src64[0] += 4;
		dst64[0] += 4;

		*dst64[0] = *src64[0];
		src64[1] = src64[0] + 1;
		src64[2] = src64[0] + 2;
		src64[3] = src64[0] + 3;
		dst64[1] = dst64[0] + 1;
		dst64[2] = dst64[0] + 2;
		dst64[3] = dst64[0] + 3;
		*dst64[1] = *src64[1];
		*dst64[2] = *src64[2];
		*dst64[3] = *src64[3];
		src64[0] += 4;
		dst64[0] += 4;

		nqw -= 8;
	}
	if (nqw) {
		switch (nqw) {
		case 7:
			*(dst64[0])++ = *(src64[0])++;
		case 6:
			*(dst64[0])++ = *(src64[0])++;
		case 5:
			*(dst64[0])++ = *(src64[0])++;
		case 4:
			*(dst64[0])++ = *(src64[0])++;
		case 3:
			*(dst64[0])++ = *(src64[0])++;
		case 2:
			*(dst64[0])++ = *(src64[0])++;
		case 1:
			*(dst64[0])++ = *(src64[0])++;
		}
	}
}
