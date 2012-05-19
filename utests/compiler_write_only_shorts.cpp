/* 
 * Copyright © 2012 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Benjamin Segovia <benjamin.segovia@intel.com>
 */

#include "utest_helper.hpp"

void compiler_write_only_shorts(void)
{
  const size_t n = 32;

  // Setup kernel and buffers
  OCL_CREATE_KERNEL("compiler_write_only_shorts");
  OCL_CREATE_BUFFER(buf[0], 0, n * sizeof(uint16_t), NULL);
  OCL_SET_ARG(0, sizeof(cl_mem), &buf[0]);

  // Run the kernel
  globals[0] = n;
  locals[0] = 16;
  OCL_NDRANGE(1);
  OCL_MAP_BUFFER(0);

  // Check results
  for (uint32_t i = 0; i < n; ++i)
    OCL_ASSERT(((uint16_t*)buf_data[0])[i] == 2);
}

MAKE_UTEST_FROM_FUNCTION(compiler_write_only_shorts);
