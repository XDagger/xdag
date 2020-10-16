#include <gtest/gtest.h>

TEST(testXfer,xfer){
	ASSERT_TRUE(1 == 1) << "test xfer ok";
}

int main() {
	std::cout << "test xfer functions" << std::endl;
	::testing::InitGoogleTest();
	return RUN_ALL_TESTS();
}