#include <iostream>
#include <gtest/gtest.h>

extern "C" {
#include "slab_alloc.h"
};

TEST(BuddyTest, FooTest) {
    EXPECT_EQ(slab_mul(1,2), 2);
}
