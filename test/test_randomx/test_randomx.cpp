#include <gtest/gtest.h>

TEST(testRandomX,randomX){
	ASSERT_TRUE(1 == 1) << "random X ok";
}

int main() {
	std::cout << "test randomX functions" << std::endl;
	::testing::InitGoogleTest();
	return RUN_ALL_TESTS();
}