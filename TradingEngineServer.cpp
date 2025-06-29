#include "TradingEngineServer.hpp"

grpc::Status TradingEngineServer::AddOrder(grpc::ServerContext * /*context*/, const trading::OrderRequest *request,
										   trading::TradeResponse *response) {

	OrderPointer order = std::make_shared<Order>(
		ParseOrderType(request->order_type()),
		request->order_id(),
		ParseSide(request->side()),
		request->price(),
		request->quantity());

	auto trades = orderbook_->AddOrder(order);
	
	// Set status based on whether order was filled or just placed
	if (trades.empty()) {
		// Order was added to orderbook without matches
		response->set_status(::trading::OrderStatus::ACCEPTED);
	} else {
		// Order was matched and generated trades
		response->set_status(::trading::OrderStatus::FILLED);
		for (const auto &trade : trades) {
			auto *bidTradeInfo = response->add_trades();
			bidTradeInfo->set_order_id(trade.GetBidTrade().orderId_);
			bidTradeInfo->set_price(trade.GetBidTrade().price_);
			bidTradeInfo->set_quantity(trade.GetBidTrade().quantity_);

			auto *askTradeInfo = response->add_trades();
			askTradeInfo->set_order_id(trade.GetAskTrade().orderId_);
			askTradeInfo->set_price(trade.GetAskTrade().price_);
			askTradeInfo->set_quantity(trade.GetAskTrade().quantity_);
		}
	}

	return grpc::Status::OK;
}

grpc::Status TradingEngineServer::CancelOrder(grpc::ServerContext * /*context*/, const trading::CancelOrderRequest *request,
											  trading::CancelOrderResponse *response) {
	orderbook_->CancelOrder(request->order_id());
	response->set_success(true);

	return grpc::Status::OK;
}

grpc::Status TradingEngineServer::ModifyOrder(grpc::ServerContext * /*context*/, const trading::ModifyOrderRequest *request,
											  trading::TradeResponse *response) {
	// Check if order exists first
	if (!orderbook_->OrderExists(request->order_id())) {
		response->set_status(::trading::OrderStatus::REJECTED);
		return grpc::Status::OK;
	}

	OrderModify order(
		request->order_id(),
		ParseSide(request->side()),
		request->new_price(),
		request->new_quantity());

	Trades trades = orderbook_->ModifyOrder(order);
	
	// Set status based on whether order modification resulted in trades
	if (trades.empty()) {
		// Order was modified successfully without matches
		response->set_status(::trading::OrderStatus::ACCEPTED);
	} else {
		// Order modification resulted in trades
		response->set_status(::trading::OrderStatus::FILLED);
		for (const auto &trade : trades) {
			auto *bidTradeInfo = response->add_trades();
			bidTradeInfo->set_order_id(trade.GetBidTrade().orderId_);
			bidTradeInfo->set_price(trade.GetBidTrade().price_);
			bidTradeInfo->set_quantity(trade.GetBidTrade().quantity_);

			auto *askTradeInfo = response->add_trades();
			askTradeInfo->set_order_id(trade.GetAskTrade().orderId_);
			askTradeInfo->set_price(trade.GetAskTrade().price_);
			askTradeInfo->set_quantity(trade.GetAskTrade().quantity_);
		}
	}

	return grpc::Status::OK;
}

grpc::Status TradingEngineServer::GetOrderbook(grpc::ServerContext * /*context*/, const trading::OrderbookRequest * /*request*/,
											   trading::OrderbookResponse *response) {
	OrderbookLevelInfos orderInfos = orderbook_->GetOrderInfos();

	const auto &bids = orderInfos.GetBids();
	const auto &asks = orderInfos.GetAsks();

	for (const auto &bid : bids) {
		auto *bidInfo = response->add_bids();
		bidInfo->set_price(bid.price_);
		bidInfo->set_quantity(bid.quantity_);
	}
	for (const auto &ask : asks) {
		auto *askInfo = response->add_asks();
		askInfo->set_price(ask.price_);
		askInfo->set_quantity(ask.quantity_);
	}
	return grpc::Status::OK;
}

OrderType TradingEngineServer::ParseOrderType(trading::OrderType type) {
	switch (type) {
	case trading::OrderType::MARKET:
		return OrderType::Market;
	case trading::OrderType::GOOD_FOR_DAY:
		return OrderType::GoodForDay;
	case trading::OrderType::FILL_OR_KILL:
		return OrderType::FillOrKill;
	case trading::OrderType::FILL_AND_KILL:
		return OrderType::FillAndKill;
	case trading::OrderType::GOOD_TILL_CANCEL:
		return OrderType::GoodTillCancel;
	default:
		throw std::invalid_argument("Unknown order type");
	}
}

Side TradingEngineServer::ParseSide(::trading::Side side) {
	switch (side) {
	case trading::Side::BUY:
		return Side::Buy;
	case trading::Side::SELL:
		return Side::Sell;
	default:
		throw std::invalid_argument("Unknown side");
	}
}