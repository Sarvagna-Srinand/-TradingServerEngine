#include <gtest/gtest.h>
#include "../Orderbook.hpp"
#include "../Order.hpp"
#include "../OrderModify.hpp"
#include <memory>

class OrderbookTest : public ::testing::Test {
protected:
    void SetUp() override {
        orderbook = std::make_shared<Orderbook>();
    }
    
    void TearDown() override {
        orderbook.reset();
    }
    
    std::shared_ptr<Orderbook> orderbook;
    
    OrderPointer CreateOrder(OrderId id, Side side, Price price, Quantity quantity, 
                           OrderType type = OrderType::GoodTillCancel) {
        return std::make_shared<Order>(type, id, side, price, quantity);
    }
};

TEST_F(OrderbookTest, EmptyOrderbookInitialization) {
    EXPECT_EQ(orderbook->Size(), 0);
    
    auto orderInfos = orderbook->GetOrderInfos();
    EXPECT_TRUE(orderInfos.GetBids().empty());
    EXPECT_TRUE(orderInfos.GetAsks().empty());
}

TEST_F(OrderbookTest, AddSingleBuyOrder) {
    auto order = CreateOrder(1, Side::Buy, 100, 1000);
    auto trades = orderbook->AddOrder(order);
    
    EXPECT_TRUE(trades.empty());  // No matching orders
    EXPECT_EQ(orderbook->Size(), 1);
    
    auto orderInfos = orderbook->GetOrderInfos();
    EXPECT_EQ(orderInfos.GetBids().size(), 1);
    EXPECT_TRUE(orderInfos.GetAsks().empty());
    
    EXPECT_EQ(orderInfos.GetBids()[0].price_, 100);
    EXPECT_EQ(orderInfos.GetBids()[0].quantity_, 1000);
}

TEST_F(OrderbookTest, AddSingleSellOrder) {
    auto order = CreateOrder(1, Side::Sell, 200, 500);
    auto trades = orderbook->AddOrder(order);
    
    EXPECT_TRUE(trades.empty());  // No matching orders
    EXPECT_EQ(orderbook->Size(), 1);
    
    auto orderInfos = orderbook->GetOrderInfos();
    EXPECT_TRUE(orderInfos.GetBids().empty());
    EXPECT_EQ(orderInfos.GetAsks().size(), 1);
    
    EXPECT_EQ(orderInfos.GetAsks()[0].price_, 200);
    EXPECT_EQ(orderInfos.GetAsks()[0].quantity_, 500);
}

TEST_F(OrderbookTest, SimpleMatching) {
    // Add buy order
    auto buyOrder = CreateOrder(1, Side::Buy, 100, 1000);
    auto trades1 = orderbook->AddOrder(buyOrder);
    EXPECT_TRUE(trades1.empty());
    
    // Add matching sell order
    auto sellOrder = CreateOrder(2, Side::Sell, 100, 500);
    auto trades2 = orderbook->AddOrder(sellOrder);
    
    EXPECT_EQ(trades2.size(), 1);
    EXPECT_EQ(trades2[0].GetBidTrade().orderId_, 1);
    EXPECT_EQ(trades2[0].GetAskTrade().orderId_, 2);
    EXPECT_EQ(trades2[0].GetBidTrade().quantity_, 500);
    EXPECT_EQ(trades2[0].GetAskTrade().quantity_, 500);
    EXPECT_EQ(trades2[0].GetBidTrade().price_, 100);
    
    // Check remaining orderbook state
    EXPECT_EQ(orderbook->Size(), 1);  // Only partial buy order remains
    
    auto orderInfos = orderbook->GetOrderInfos();
    EXPECT_EQ(orderInfos.GetBids().size(), 1);
    EXPECT_TRUE(orderInfos.GetAsks().empty());
    EXPECT_EQ(orderInfos.GetBids()[0].quantity_, 500);  // Remaining quantity
}

TEST_F(OrderbookTest, CompleteMatching) {
    // Add buy order
    auto buyOrder = CreateOrder(1, Side::Buy, 100, 1000);
    orderbook->AddOrder(buyOrder);
    
    // Add exactly matching sell order
    auto sellOrder = CreateOrder(2, Side::Sell, 100, 1000);
    auto trades = orderbook->AddOrder(sellOrder);
    
    EXPECT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].GetBidTrade().quantity_, 1000);
    EXPECT_EQ(trades[0].GetAskTrade().quantity_, 1000);
    
    // Both orders should be fully filled and removed
    EXPECT_EQ(orderbook->Size(), 0);
    
    auto orderInfos = orderbook->GetOrderInfos();
    EXPECT_TRUE(orderInfos.GetBids().empty());
    EXPECT_TRUE(orderInfos.GetAsks().empty());
}

TEST_F(OrderbookTest, MultipleOrdersAtSamePrice) {
    // Add multiple buy orders at same price
    orderbook->AddOrder(CreateOrder(1, Side::Buy, 100, 500));
    orderbook->AddOrder(CreateOrder(2, Side::Buy, 100, 300));
    orderbook->AddOrder(CreateOrder(3, Side::Buy, 100, 200));
    
    EXPECT_EQ(orderbook->Size(), 3);
    
    auto orderInfos = orderbook->GetOrderInfos();
    EXPECT_EQ(orderInfos.GetBids().size(), 1);  // One price level
    EXPECT_EQ(orderInfos.GetBids()[0].quantity_, 1000);  // Total quantity
}

TEST_F(OrderbookTest, PricePriority) {
    // Add buy orders at different prices
    orderbook->AddOrder(CreateOrder(1, Side::Buy, 100, 1000));
    orderbook->AddOrder(CreateOrder(2, Side::Buy, 110, 500));  // Higher price
    orderbook->AddOrder(CreateOrder(3, Side::Buy, 90, 800));   // Lower price
    
    // Add sell order that should match with highest bid
    auto sellOrder = CreateOrder(4, Side::Sell, 100, 200);
    auto trades = orderbook->AddOrder(sellOrder);
    
    EXPECT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].GetBidTrade().orderId_, 2);  // Should match order 2 (highest price)
    EXPECT_EQ(trades[0].GetBidTrade().price_, 110);
}

TEST_F(OrderbookTest, CancelOrder) {
    auto order = CreateOrder(1, Side::Buy, 100, 1000);
    orderbook->AddOrder(order);
    EXPECT_EQ(orderbook->Size(), 1);
    
    orderbook->CancelOrder(1);
    EXPECT_EQ(orderbook->Size(), 0);
    
    auto orderInfos = orderbook->GetOrderInfos();
    EXPECT_TRUE(orderInfos.GetBids().empty());
}

TEST_F(OrderbookTest, CancelNonExistentOrder) {
    // Should not crash or affect anything
    orderbook->CancelOrder(999);
    EXPECT_EQ(orderbook->Size(), 0);
}

TEST_F(OrderbookTest, ModifyOrder) {
    auto order = CreateOrder(1, Side::Buy, 100, 1000);
    orderbook->AddOrder(order);
    
    OrderModify modify(1, Side::Buy, 110, 500);
    auto trades = orderbook->ModifyOrder(modify);
    
    EXPECT_TRUE(trades.empty());  // No matching for modified order
    EXPECT_EQ(orderbook->Size(), 1);
    
    auto orderInfos = orderbook->GetOrderInfos();
    EXPECT_EQ(orderInfos.GetBids()[0].price_, 110);
    EXPECT_EQ(orderInfos.GetBids()[0].quantity_, 500);
}

TEST_F(OrderbookTest, ModifyNonExistentOrder) {
    OrderModify modify(999, Side::Buy, 100, 1000);
    auto trades = orderbook->ModifyOrder(modify);
    
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(orderbook->Size(), 0);
}

TEST_F(OrderbookTest, MarketOrderBuy) {
    // Add sell orders first
    orderbook->AddOrder(CreateOrder(1, Side::Sell, 200, 500));
    orderbook->AddOrder(CreateOrder(2, Side::Sell, 210, 300));
    
    // Add market buy order
    auto marketOrder = CreateOrder(3, Side::Buy, Constants::InvalidPrice, 400, OrderType::Market);
    auto trades = orderbook->AddOrder(marketOrder);
    
    EXPECT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].GetAskTrade().orderId_, 1);  // Should match best ask
    EXPECT_EQ(trades[0].GetBidTrade().quantity_, 400);
}

TEST_F(OrderbookTest, FillAndKillOrder) {
    // Add sell order
    orderbook->AddOrder(CreateOrder(1, Side::Sell, 200, 300));
    
    // Add FAK buy order for more than available
    auto fakOrder = CreateOrder(2, Side::Buy, 200, 500, OrderType::FillAndKill);
    auto trades = orderbook->AddOrder(fakOrder);
    
    EXPECT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0].GetBidTrade().quantity_, 300);  // Only what was available
    
    // FAK order should be removed (can't be fully filled)
    EXPECT_EQ(orderbook->Size(), 0);  // Both orders should be gone
}

TEST_F(OrderbookTest, FillOrKillOrderSuccess) {
    // Add enough sell orders
    orderbook->AddOrder(CreateOrder(1, Side::Sell, 200, 500));
    orderbook->AddOrder(CreateOrder(2, Side::Sell, 200, 500));
    
    // Add FOK buy order that can be fully filled
    auto fokOrder = CreateOrder(3, Side::Buy, 200, 800, OrderType::FillOrKill);
    auto trades = orderbook->AddOrder(fokOrder);
    
    EXPECT_EQ(trades.size(), 2);  // Should match both sell orders
    EXPECT_EQ(orderbook->Size(), 1);  // One sell order partially remaining
}

TEST_F(OrderbookTest, FillOrKillOrderFailure) {
    // Add insufficient sell orders
    orderbook->AddOrder(CreateOrder(1, Side::Sell, 200, 300));
    
    // Add FOK buy order that cannot be fully filled
    auto fokOrder = CreateOrder(2, Side::Buy, 200, 500, OrderType::FillOrKill);
    auto trades = orderbook->AddOrder(fokOrder);
    
    EXPECT_TRUE(trades.empty());  // No trades should occur
    EXPECT_EQ(orderbook->Size(), 1);  // Original sell order should remain
}

TEST_F(OrderbookTest, InvalidOrderRejection) {
    // Test null order
    auto trades1 = orderbook->AddOrder(nullptr);
    EXPECT_TRUE(trades1.empty());
    EXPECT_EQ(orderbook->Size(), 0);
    
    // Test zero quantity order
    auto zeroOrder = CreateOrder(1, Side::Buy, 100, 0);
    auto trades2 = orderbook->AddOrder(zeroOrder);
    EXPECT_TRUE(trades2.empty());
    EXPECT_EQ(orderbook->Size(), 0);
}

TEST_F(OrderbookTest, DuplicateOrderId) {
    auto order1 = CreateOrder(1, Side::Buy, 100, 1000);
    auto order2 = CreateOrder(1, Side::Sell, 200, 500);  // Same ID
    
    orderbook->AddOrder(order1);
    auto trades = orderbook->AddOrder(order2);  // Should be rejected
    
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(orderbook->Size(), 1);  // Only first order should remain
}
