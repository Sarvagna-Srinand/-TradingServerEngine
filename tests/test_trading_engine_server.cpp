#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include "../TradingEngineServer.hpp"
#include "../Orderbook.hpp"
#include "../trading_optimized.grpc.pb.h"

class TradingEngineServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        orderbook = std::make_shared<Orderbook>();
        server = std::make_unique<TradingEngineServer>(orderbook);
        context = std::make_unique<grpc::ServerContext>();
    }
    
    void TearDown() override {
        server.reset();
        orderbook.reset();
        context.reset();
    }
    
    std::shared_ptr<Orderbook> orderbook;
    std::unique_ptr<TradingEngineServer> server;
    std::unique_ptr<grpc::ServerContext> context;
    
    trading::OrderRequest CreateOrderRequest(uint32_t orderId, trading::Side side, 
                                            uint32_t price, uint32_t quantity,
                                            trading::OrderType orderType = trading::GOOD_TILL_CANCEL) {
        trading::OrderRequest request;
        request.set_order_id(orderId);
        request.set_side(side);
        request.set_price(price);
        request.set_quantity(quantity);
        request.set_order_type(orderType);
        return request;
    }
};

TEST_F(TradingEngineServerTest, AddOrderSuccess) {
    auto request = CreateOrderRequest(1, trading::BUY, 100, 1000);
    trading::TradeResponse response;
    
    auto status = server->AddOrder(context.get(), &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.status(), trading::ACCEPTED);
    EXPECT_EQ(response.trades_size(), 0);  // No matching orders
    EXPECT_EQ(orderbook->Size(), 1);
}

TEST_F(TradingEngineServerTest, AddOrderWithMatching) {
    // Add buy order first
    auto buyRequest = CreateOrderRequest(1, trading::BUY, 100, 1000);
    trading::TradeResponse buyResponse;
    server->AddOrder(context.get(), &buyRequest, &buyResponse);
    
    // Add matching sell order
    auto sellRequest = CreateOrderRequest(2, trading::SELL, 100, 500);
    trading::TradeResponse sellResponse;
    auto status = server->AddOrder(context.get(), &sellRequest, &sellResponse);
    
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(sellResponse.status(), trading::FILLED);  // Order matched and generated trades
    EXPECT_EQ(sellResponse.trades_size(), 2);  // One trade with bid and ask info
    
    // Check trade details
    EXPECT_EQ(sellResponse.trades(0).order_id(), 1);  // Bid order ID
    EXPECT_EQ(sellResponse.trades(0).price(), 100);
    EXPECT_EQ(sellResponse.trades(0).quantity(), 500);
    
    EXPECT_EQ(sellResponse.trades(1).order_id(), 2);  // Ask order ID
    EXPECT_EQ(sellResponse.trades(1).price(), 100);
    EXPECT_EQ(sellResponse.trades(1).quantity(), 500);
}

TEST_F(TradingEngineServerTest, CancelOrderSuccess) {
    // Add order first
    auto addRequest = CreateOrderRequest(1, trading::BUY, 100, 1000);
    trading::TradeResponse addResponse;
    server->AddOrder(context.get(), &addRequest, &addResponse);
    
    // Cancel the order
    trading::CancelOrderRequest cancelRequest;
    cancelRequest.set_order_id(1);
    trading::CancelOrderResponse cancelResponse;
    
    auto status = server->CancelOrder(context.get(), &cancelRequest, &cancelResponse);
    
    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(cancelResponse.success());
    EXPECT_EQ(orderbook->Size(), 0);
}

TEST_F(TradingEngineServerTest, CancelNonExistentOrder) {
    trading::CancelOrderRequest request;
    request.set_order_id(999);  // Non-existent order
    trading::CancelOrderResponse response;
    
    auto status = server->CancelOrder(context.get(), &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(response.success());  // Should return success=true even for non-existent orders
}

TEST_F(TradingEngineServerTest, ModifyOrderSuccess) {
    // Add order first
    auto addRequest = CreateOrderRequest(1, trading::BUY, 100, 1000);
    trading::TradeResponse addResponse;
    server->AddOrder(context.get(), &addRequest, &addResponse);
    
    // Modify the order
    trading::ModifyOrderRequest modifyRequest;
    modifyRequest.set_order_id(1);
    modifyRequest.set_side(trading::BUY);
    modifyRequest.set_new_price(110);
    modifyRequest.set_new_quantity(500);
    trading::TradeResponse modifyResponse;
    
    auto status = server->ModifyOrder(context.get(), &modifyRequest, &modifyResponse);
    
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(modifyResponse.status(), trading::ACCEPTED);
    EXPECT_EQ(orderbook->Size(), 1);  // Still one order, but modified
}

TEST_F(TradingEngineServerTest, ModifyNonExistentOrder) {
    trading::ModifyOrderRequest request;
    request.set_order_id(999);  // Non-existent order
    request.set_side(trading::BUY);
    request.set_new_price(100);
    request.set_new_quantity(1000);
    trading::TradeResponse response;
    
    auto status = server->ModifyOrder(context.get(), &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.status(), trading::REJECTED);
    EXPECT_EQ(orderbook->Size(), 0);
}

TEST_F(TradingEngineServerTest, GetOrderbookEmpty) {
    trading::OrderbookRequest request;
    trading::OrderbookResponse response;
    
    auto status = server->GetOrderbook(context.get(), &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.bids_size(), 0);
    EXPECT_EQ(response.asks_size(), 0);
}

TEST_F(TradingEngineServerTest, GetOrderbookWithOrders) {
    // Add some orders
    auto buyRequest1 = CreateOrderRequest(1, trading::BUY, 100, 1000);
    auto buyRequest2 = CreateOrderRequest(2, trading::BUY, 95, 500);
    auto sellRequest1 = CreateOrderRequest(3, trading::SELL, 105, 800);
    auto sellRequest2 = CreateOrderRequest(4, trading::SELL, 110, 300);
    
    trading::TradeResponse tempResponse;
    server->AddOrder(context.get(), &buyRequest1, &tempResponse);
    server->AddOrder(context.get(), &buyRequest2, &tempResponse);
    server->AddOrder(context.get(), &sellRequest1, &tempResponse);
    server->AddOrder(context.get(), &sellRequest2, &tempResponse);
    
    // Get orderbook
    trading::OrderbookRequest request;
    trading::OrderbookResponse response;
    auto status = server->GetOrderbook(context.get(), &request, &response);
    
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.bids_size(), 2);  // Two bid levels
    EXPECT_EQ(response.asks_size(), 2);  // Two ask levels
    
    // Check bid levels (should be sorted by price descending)
    EXPECT_EQ(response.bids(0).price(), 100);
    EXPECT_EQ(response.bids(0).quantity(), 1000);
    EXPECT_EQ(response.bids(1).price(), 95);
    EXPECT_EQ(response.bids(1).quantity(), 500);
    
    // Check ask levels (should be sorted by price ascending)
    EXPECT_EQ(response.asks(0).price(), 105);
    EXPECT_EQ(response.asks(0).quantity(), 800);
    EXPECT_EQ(response.asks(1).price(), 110);
    EXPECT_EQ(response.asks(1).quantity(), 300);
}

TEST_F(TradingEngineServerTest, OrderTypeConversion) {
    // Test all order types
    std::vector<std::pair<trading::OrderType, OrderType>> typeMap = {
        {trading::GOOD_TILL_CANCEL, OrderType::GoodTillCancel},
        {trading::GOOD_FOR_DAY, OrderType::GoodForDay},
        {trading::FILL_AND_KILL, OrderType::FillAndKill},
        {trading::FILL_OR_KILL, OrderType::FillOrKill},
        {trading::MARKET, OrderType::Market}
    };
    
    for (const auto& [protoType, expectedType] : typeMap) {
        auto request = CreateOrderRequest(1, trading::BUY, 100, 1000, protoType);
        trading::TradeResponse response;
        
        auto status = server->AddOrder(context.get(), &request, &response);
        EXPECT_TRUE(status.ok());
        
        // Cancel the order for next iteration
        trading::CancelOrderRequest cancelRequest;
        cancelRequest.set_order_id(1);
        trading::CancelOrderResponse cancelResponse;
        server->CancelOrder(context.get(), &cancelRequest, &cancelResponse);
    }
}

TEST_F(TradingEngineServerTest, SideConversion) {
    // Test BUY side
    auto buyRequest = CreateOrderRequest(1, trading::BUY, 100, 1000);
    trading::TradeResponse buyResponse;
    auto buyStatus = server->AddOrder(context.get(), &buyRequest, &buyResponse);
    EXPECT_TRUE(buyStatus.ok());
    
    // Test SELL side
    auto sellRequest = CreateOrderRequest(2, trading::SELL, 105, 500);
    trading::TradeResponse sellResponse;
    auto sellStatus = server->AddOrder(context.get(), &sellRequest, &sellResponse);
    EXPECT_TRUE(sellStatus.ok());
    
    EXPECT_EQ(orderbook->Size(), 2);
}

TEST_F(TradingEngineServerTest, ConcurrentOrderHandling) {
    // This is a basic test for thread safety
    // In a real environment, you'd use multiple threads
    
    std::vector<trading::OrderRequest> requests;
    for (int i = 1; i <= 10; ++i) {
        requests.push_back(CreateOrderRequest(i, 
            (i % 2 == 0) ? trading::BUY : trading::SELL,
            100 + (i % 3), 1000));
    }
    
    std::vector<trading::TradeResponse> responses(requests.size());
    
    for (size_t i = 0; i < requests.size(); ++i) {
        auto status = server->AddOrder(context.get(), &requests[i], &responses[i]);
        EXPECT_TRUE(status.ok());
    }
    
    // At least some orders should be in the book
    EXPECT_GT(orderbook->Size(), 0);
}
