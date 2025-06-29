#pragma once

#include "Orderbook.hpp"
#include "trading_optimized.grpc.pb.h"

class TradingEngineServer final : public trading::TradingEngine::Service {
  private:
	std::shared_ptr<Orderbook> orderbook_;

	OrderType ParseOrderType(trading::OrderType type);
	Side ParseSide(::trading::Side side);

  public:
	TradingEngineServer(std::shared_ptr<Orderbook> orderbook) : orderbook_(std::move(orderbook)) {}

	grpc::Status AddOrder(grpc::ServerContext *context, const trading::OrderRequest *request,
						  trading::TradeResponse *response) override;

	grpc::Status CancelOrder(grpc::ServerContext *context, const trading::CancelOrderRequest *request,
							 trading::CancelOrderResponse *response) override;

	grpc::Status ModifyOrder(grpc::ServerContext *context, const trading::ModifyOrderRequest *request,
							 trading::TradeResponse *response) override;

	grpc::Status GetOrderbook(grpc::ServerContext *context, const trading::OrderbookRequest *request,
							  trading::OrderbookResponse *response) override;
};