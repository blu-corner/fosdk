/*
    Copyright 2018 Neueda Ltd.
*/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    testing::InitGoogleMock(&argc, argv);
    testing::InitGoogleTest(&argc, argv);
    testing::FLAGS_gtest_death_test_style = "fast";
    return RUN_ALL_TESTS();
}
