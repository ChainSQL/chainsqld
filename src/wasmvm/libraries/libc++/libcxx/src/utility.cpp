//===------------------------ utility.cpp ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "utility"

_LIBCPP_BEGIN_NAMESPACE_STD
#if defined(_LIBCPP_CXX03_LANG) || defined(_LIBCPP_BUILDING_LIBRARY)
const piecewise_construct_t piecewise_construct = {};
#endif
_LIBCPP_END_NAMESPACE_STD
