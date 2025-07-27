#include <iostream>
#include <gtest/gtest.h>

extern "C" {
#include "buddy.h"
};

TEST(BuddyTest, FooTest) {
    EXPECT_EQ(buddy_add(1,2), 3);
}
