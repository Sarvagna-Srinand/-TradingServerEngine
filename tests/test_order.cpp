#include <gtest/gtest.h>
#include "../Order.hpp"
#include "../Constants.hpp"

class OrderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup common test data
    }
    
    void TearDown() override {
        // Cleanup
    }
};

TEST_F(OrderTest, CreateValidOrder) {
    Order order(OrderType::GoodTillCancel, 123, Side::Buy, 100, 1000);
    
    EXPECT_EQ(order.GetOrderId(), 123);
    EXPECT_EQ(order.GetSide(), Side::Buy);
    EXPECT_EQ(order.GetPrice(), 100);
    EXPECT_EQ(order.GetInitialQuantity(), 1000);
    EXPECT_EQ(order.GetRemainingQuantity(), 1000);
    EXPECT_EQ(order.GetFilledQuantity(), 0);
    EXPECT_FALSE(order.IsFilled());
    EXPECT_EQ(order.GetOrderType(), OrderType::GoodTillCancel);
}

TEST_F(OrderTest, CreateMarketOrder) {
    Order order(456, Side::Sell, 500);
    
    EXPECT_EQ(order.GetOrderId(), 456);
    EXPECT_EQ(order.GetSide(), Side::Sell);
    EXPECT_EQ(order.GetPrice(), Constants::InvalidPrice);
    EXPECT_EQ(order.GetInitialQuantity(), 500);
    EXPECT_EQ(order.GetRemainingQuantity(), 500);
    EXPECT_EQ(order.GetOrderType(), OrderType::Market);
}

TEST_F(OrderTest, FillOrder) {
    Order order(OrderType::GoodTillCancel, 789, Side::Buy, 50, 1000);
    
    // Fill partially
    order.Fill(300);
    EXPECT_EQ(order.GetRemainingQuantity(), 700);
    EXPECT_EQ(order.GetFilledQuantity(), 300);
    EXPECT_FALSE(order.IsFilled());
    
    // Fill completely
    order.Fill(700);
    EXPECT_EQ(order.GetRemainingQuantity(), 0);
    EXPECT_EQ(order.GetFilledQuantity(), 1000);
    EXPECT_TRUE(order.IsFilled());
}

TEST_F(OrderTest, FillOrderThrowsOnOverfill) {
    Order order(OrderType::GoodTillCancel, 999, Side::Buy, 50, 100);
    
    EXPECT_THROW(order.Fill(150), std::logic_error);
}

TEST_F(OrderTest, ConvertToGoodTillCancel) {
    Order order(OrderType::Market, 111, Side::Buy, Constants::InvalidPrice, 200);
    
    order.ToGoodTillCancel(75);
    
    EXPECT_EQ(order.GetOrderType(), OrderType::GoodTillCancel);
    EXPECT_EQ(order.GetPrice(), 75);
}

TEST_F(OrderTest, OrderTypeValidation) {
    Order gtcOrder(OrderType::GoodTillCancel, 1, Side::Buy, 100, 1000);
    Order gfdOrder(OrderType::GoodForDay, 2, Side::Sell, 200, 500);
    Order fakOrder(OrderType::FillAndKill, 3, Side::Buy, 150, 250);
    Order fokOrder(OrderType::FillOrKill, 4, Side::Sell, 175, 750);
    
    EXPECT_EQ(gtcOrder.GetOrderType(), OrderType::GoodTillCancel);
    EXPECT_EQ(gfdOrder.GetOrderType(), OrderType::GoodForDay);
    EXPECT_EQ(fakOrder.GetOrderType(), OrderType::FillAndKill);
    EXPECT_EQ(fokOrder.GetOrderType(), OrderType::FillOrKill);
}

TEST_F(OrderTest, SideValidation) {
    Order buyOrder(OrderType::GoodTillCancel, 1, Side::Buy, 100, 1000);
    Order sellOrder(OrderType::GoodTillCancel, 2, Side::Sell, 200, 500);
    
    EXPECT_EQ(buyOrder.GetSide(), Side::Buy);
    EXPECT_EQ(sellOrder.GetSide(), Side::Sell);
}

TEST_F(OrderTest, QuantityBehavior) {
    Order order(OrderType::GoodTillCancel, 1, Side::Buy, 100, 1000);
    
    // Initial state
    EXPECT_EQ(order.GetInitialQuantity(), 1000);
    EXPECT_EQ(order.GetRemainingQuantity(), 1000);
    EXPECT_EQ(order.GetFilledQuantity(), 0);
    
    // After partial fill
    order.Fill(250);
    EXPECT_EQ(order.GetInitialQuantity(), 1000);  // Should remain unchanged
    EXPECT_EQ(order.GetRemainingQuantity(), 750);
    EXPECT_EQ(order.GetFilledQuantity(), 250);
    
    // After another fill
    order.Fill(100);
    EXPECT_EQ(order.GetInitialQuantity(), 1000);  // Should remain unchanged
    EXPECT_EQ(order.GetRemainingQuantity(), 650);
    EXPECT_EQ(order.GetFilledQuantity(), 350);
}
