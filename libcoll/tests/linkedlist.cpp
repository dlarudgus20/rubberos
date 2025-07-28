#include <iostream>
#include <gtest/gtest.h>

extern "C" {
#define restrict
#include "collections/linkedlist.h"
};

#include <stdlib.h>
#include <stdio.h>

TEST(linkedlist_test, trivial) {
    EXPECT_EQ(2, 2);
}
