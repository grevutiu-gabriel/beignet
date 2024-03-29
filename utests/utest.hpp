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

/**
 * \file utest.hpp
 * \author Benjamin Segovia <benjamin.segovia@intel.com>
 *
 * Provides all unit test capabilites. It is rather rudimentary but it should
 * do the job
 */
#ifndef __UTEST_UTEST_HPP__
#define __UTEST_UTEST_HPP__

#include "utest_exception.hpp"
#include <vector>
#include <iostream>

/*! Quick and dirty unit test system with registration */
struct UTest
{
  /*! A unit test function to run */
  typedef void (*Function) (void);
  /*! Empty test */
  UTest(void);
  /*! Build a new unit test and append it to the unit test list */
  UTest(Function fn, const char *name);
  /*! Function to execute */
  Function fn;
  /*! Name of the test */
  const char *name;
  /*! The tests that are registered */
  static std::vector<UTest> *utestList;
  /*! Run the test with the given name */
  static void run(const char *name);
  /*! Run all the tests */
  static void runAll(void);
};

/*! Register a new unit test */
#define UTEST_REGISTER(FN) static const UTest __##FN##__(FN, #FN);

/*! Turn a function into a unit test */
#define MAKE_UTEST_FROM_FUNCTION(FN) \
  static void __ANON__##FN##__(void) { UTEST_EXPECT_SUCCESS(FN()); } \
  static const UTest __##FN##__(__ANON__##FN##__, #FN);

/*! No assert is expected */
#define UTEST_EXPECT_SUCCESS(EXPR) \
 do { \
    try { \
      EXPR; \
      std::cout << "  " << #EXPR << "    [SUCCESS]" << std::endl; \
    } \
    catch (Exception e) { \
      std::cout << "  " << #EXPR << "    [FAILED]" << std::endl; \
      std::cout << "    " << e.what() << std::endl; \
    } \
  } while (0)

#define UTEST_EXPECT_FAILED(EXPR) \
 do { \
    try { \
      EXPR; \
      std::cout << "  " << #EXPR << "    [FAILED]" <<  std::endl; \
    } \
    catch (gbe::Exception e) { \
      std::cout << "  " << #EXPR << "    [SUCCESS]" << std::endl; \
    } \
  } while (0)

#endif /* __UTEST_UTEST_HPP__ */

