#include <iostream>
#include <gtest/gtest.h>

int main() {
	std::cout << "test core functions" << std::endl;
	::testing::InitGoogleTest();
	return RUN_ALL_TESTS();
}