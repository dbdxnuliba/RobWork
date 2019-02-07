/******************************************************************************
 * Copyright 2019 The Robotics Group, The Maersk Mc-Kinney Moller Institute,
 * Faculty of Engineering, University of Southern Denmark
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#include <gtest/gtest.h>

#include <rw/common/ConcatVectorIterator.hpp>
#include <rw/common/VectorIterator.hpp>

using namespace rw::common;

TEST(ConcatVectorIterator, compile) {
    std::vector<char*> curr;
    std::vector<char*> next;

    ConcatVectorIterator<char> begin(&curr, curr.begin(), &next);

    ConstConcatVectorIterator<char> const_begin(begin);

    ConstConcatVectorIterator<char> f = begin;
    ++f;

    const_begin++;

    char& x = *begin;

    const char& c = *const_begin;
    EXPECT_TRUE(c == x);
}

TEST(VectorIterator, compile) {
    std::vector<char*> vals;
    VectorIterator<char>(vals.begin());
}