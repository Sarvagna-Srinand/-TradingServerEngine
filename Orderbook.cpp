#include "Orderbook.hpp"
#include "Order.hpp"
#include "OrderModify.hpp"
#include "OrderType.hpp"
#include "OrderbookLevelInfos.hpp"
#include "Side.hpp"
#include "Usings.hpp"

#include <chrono>
#include <ctime>
#include <numeric>
#include <optional>

void Orderbook::PruneGoodForDayOrders() {
	using namespace std::chrono;
	const auto end = 16h;

	while (true) {
		const auto now = system_clock::now();
		const auto now_c = system_clock::to_time_t(now);
		std::tm now_parts;
		localtime_r(&now_c, &now_parts);

		if (now_parts.tm_hour >= end.count())
			now_parts.tm_mday += 1;

		now_parts.tm_hour = end.count();
		now_parts.tm_min = 0;
		now_parts.tm_sec = 0;

		auto next = system_clock::from_time_t(mktime(&now_parts));
		auto till = next - now + 100ms; // Buffer to ensure we don't miss the deadline

		{
			std::unique_lock ordersLock{ordersMutex_};

			if (shutdown_.load(std::memory_order_acquire) ||
				shutdownConditionVariable_.wait_for(ordersLock, till) == std::cv_status::no_timeout)
				return;
		}

		// {
		// 	std::scoped_lock ordersLock{ordersMutex_};

		// 	for (const auto &[_, entry] : orders_) {
		// 		const auto &[order, iterator] = entry;

		// 		if (order->GetOrderType() == OrderType::GoodForDay)
		// 			orderIds.push_back(order->GetOrderId());
		// 	}
		// }

		OrderIds orderIds = {goodForDayOrders_.begin(), goodForDayOrders_.end()};

		CancelOrders(orderIds);
		// goodForDayOrders_.clear(); // Just to make sure we don't keep them in memory, as they are cleared in CancelOrders() anyway
	}
}

void Orderbook::CancelOrders(OrderIds orderIds) {
	std::scoped_lock ordersLock{ordersMutex_};

	for (const auto &orderId : orderIds)
		CancelOrderInternal(orderId);
}

void Orderbook::CancelOrderInternal(OrderId orderId) {
	if (orders_.find(orderId) == orders_.end())
		return;

	const auto [order, iterator] = orders_.at(orderId);
	orders_.erase(orderId);

	if (order->GetOrderType() == OrderType::GoodForDay)
		goodForDayOrders_.erase(orderId);

	if (order->GetSide() == Side::Sell) {
		auto price = order->GetPrice();
		auto &orders = asks_.at(price);
		orders.erase(iterator);
		if (orders.empty())
			asks_.erase(price);
	} else {
		auto price = order->GetPrice();
		auto &orders = bids_.at(price);
		orders.erase(iterator);
		if (orders.empty())
			bids_.erase(price);
	}

	OnOrderCancelled(order);
}

void Orderbook::OnOrderCancelled(OrderPointer order) {
	UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Remove);
}

void Orderbook::OnOrderAdded(OrderPointer order) {
	UpdateLevelData(order->GetPrice(), order->GetInitialQuantity(), LevelData::Action::Add);
}

void Orderbook::OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled) {
	UpdateLevelData(price, quantity, isFullyFilled ? LevelData::Action::Remove : LevelData::Action::Match);
}

void Orderbook::UpdateLevelData(Price price, Quantity quantity, LevelData::Action action) {
	auto &data = data_[price];

	data.count_ += action == LevelData::Action::Remove ? -1 : action == LevelData::Action::Add ? 1
																							   : 0;
	if (action == LevelData::Action::Remove || action == LevelData::Action::Match) {
		data.quantity_ -= quantity;
	} else {
		data.quantity_ += quantity;
	}

	if (data.count_ == 0)
		data_.erase(price);
}

bool Orderbook::CanFullyFill(Side side, Price price, Quantity quantity) const {
	if (!CanMatch(side, price))
		return false;

	std::optional<Price> threshold;

	if (side == Side::Buy) {
		const auto [askPrice, _] = *asks_.begin();
		threshold = askPrice;
	} else {
		const auto [bidPrice, _] = *bids_.begin();
		threshold = bidPrice;
	}

	for (const auto &[levelPrice, levelData] : data_) {
		if (threshold.has_value() &&
			((side == Side::Buy && threshold.value() > levelPrice) ||
			 (side == Side::Sell && threshold.value() < levelPrice)))
			continue;

		if ((side == Side::Buy && levelPrice > price) ||
			(side == Side::Sell && levelPrice < price))
			continue;

		if (quantity <= levelData.quantity_)
			return true;

		quantity -= levelData.quantity_;
	}

	return false;
}

bool Orderbook::CanMatch(Side side, Price price) const {
	if (side == Side::Buy) {
		if (asks_.empty())
			return false;

		const auto &[bestAsk, _] = *asks_.begin();
		return price >= bestAsk;
	} else {
		if (bids_.empty())
			return false;

		const auto &[bestBid, _] = *bids_.begin();
		return price <= bestBid;
	}
}

Trades Orderbook::MatchOrders() {
	Trades trades;
	trades.reserve(orders_.size());

	while (true) {
		if (bids_.empty() || asks_.empty())
			break;

		auto &[bidPrice, bids] = *bids_.begin();
		auto &[askPrice, asks] = *asks_.begin();

		if (bidPrice < askPrice)
			break;

		while (!bids.empty() && !asks.empty()) {
			auto bid = bids.front();
			auto ask = asks.front();

			Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

			bid->Fill(quantity);
			ask->Fill(quantity);

			if (bid->IsFilled()) {
				bids.pop_front();
				orders_.erase(bid->GetOrderId());
			}

			if (ask->IsFilled()) {
				asks.pop_front();
				orders_.erase(ask->GetOrderId());
			}

			trades.push_back(Trade{
				TradeInfo{bid->GetOrderId(), bid->GetPrice(), quantity},
				TradeInfo{ask->GetOrderId(), ask->GetPrice(), quantity}});

			OnOrderMatched(bid->GetPrice(), quantity, bid->IsFilled());
			OnOrderMatched(ask->GetPrice(), quantity, ask->IsFilled());
		}

		if (bids.empty()) {
			bids_.erase(bidPrice);
			data_.erase(bidPrice);
		}

		if (asks.empty()) {
			asks_.erase(askPrice);
			data_.erase(askPrice);
		}
	}

	if (!bids_.empty()) {
		auto &[_, bids] = *bids_.begin();
		auto &order = bids.front();
		if (order->GetOrderType() == OrderType::FillAndKill)
			CancelOrderInternal(order->GetOrderId());  // Use internal method to avoid mutex deadlock
	}

	if (!asks_.empty()) {
		auto &[_, asks] = *asks_.begin();
		auto &order = asks.front();
		if (order->GetOrderType() == OrderType::FillAndKill)
			CancelOrderInternal(order->GetOrderId());  // Use internal method to avoid mutex deadlock
	}

	return trades;
}

Orderbook::Orderbook() : ordersPruneThread_{[this] { PruneGoodForDayOrders(); }} {}

Orderbook::~Orderbook() {
	shutdown_.store(true, std::memory_order_release);
	shutdownConditionVariable_.notify_one();
	ordersPruneThread_.join();
}

Trades Orderbook::AddOrder(OrderPointer order) {
	// Input validation
	if (!order || order->GetInitialQuantity() == 0) {
		return {};
	}
	
	if (order->GetPrice() < 0 || order->GetPrice() > 1000000) { // Reasonable price bounds
		return {};
	}
	
	std::scoped_lock ordersLock{ordersMutex_};

	// if (orders_.find(order->GetOrderId()) != orders_.end())
	// 	return {};

	auto [it, inserted] = orders_.insert({order->GetOrderId(), OrderEntry{order, OrderPointers::iterator()}});
	if (!inserted) // Saves one map lookup if the order already exists
		return {};

	if (order->GetOrderType() == OrderType::Market) {
		if (order->GetSide() == Side::Buy && !asks_.empty()) {
			const auto &[worstAsk, _] = *asks_.rbegin();
			order->ToGoodTillCancel(worstAsk);
		} else if (order->GetSide() == Side::Sell && !bids_.empty()) {
			const auto &[worstBid, _] = *bids_.rbegin();
			order->ToGoodTillCancel(worstBid);
		} else {
			orders_.erase(it);
			return {};
		}
	}
	if (order->GetOrderType() == OrderType::GoodForDay)
		goodForDayOrders_.insert(order->GetOrderId());

	if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice())) {
		orders_.erase(it);
		return {};
	}

	if (order->GetOrderType() == OrderType::FillOrKill && !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetInitialQuantity())) {
		orders_.erase(it);
		return {};
	}

	// OrderPointers::iterator iterator;

	// if (order->GetSide() == Side::Buy) {
	// 	auto &orders = bids_[order->GetPrice()];
	// 	orders.push_back(order);
	// 	iterator = std::prev(orders.end());
	// } else {
	// 	auto &orders = asks_[order->GetPrice()];
	// 	orders.push_back(order);
	// 	iterator = std::prev(orders.end());
	// }

	auto &ordersAtPrice = (order->GetSide() == Side::Buy ? bids_[order->GetPrice()] : asks_[order->GetPrice()]);
	ordersAtPrice.push_back(order);
	it->second.location_ = std::prev(ordersAtPrice.end());

	OnOrderAdded(order);

	return MatchOrders();
}

void Orderbook::CancelOrder(OrderId orderId) {
	std::scoped_lock ordersLock{ordersMutex_};

	CancelOrderInternal(orderId);
}

Trades Orderbook::ModifyOrder(OrderModify order) {
	OrderType orderType;

	{
		std::scoped_lock ordersLock{ordersMutex_};

		if (orders_.find(order.GetOrderId()) == orders_.end())
			return {};

		const auto &[existingOrder, _] = orders_.at(order.GetOrderId());
		orderType = existingOrder->GetOrderType();
	}

	CancelOrder(order.GetOrderId());
	return AddOrder(order.ToOrderPointer(orderType));
}

bool Orderbook::OrderExists(OrderId orderId) const {
	std::scoped_lock ordersLock{ordersMutex_};
	return orders_.find(orderId) != orders_.end();
}

std::size_t Orderbook::Size() const {
	std::scoped_lock ordersLock{ordersMutex_};
	return orders_.size();
}

OrderbookLevelInfos Orderbook::GetOrderInfos() const {
	LevelInfos bidInfos, askInfos;
	bidInfos.reserve(orders_.size());
	askInfos.reserve(orders_.size());

	auto CreateLevelInfos = [](Price price, const OrderPointers &orders) {
		return LevelInfo{price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
												[](Quantity runningSum, const OrderPointer &order) { return runningSum + order->GetRemainingQuantity(); })};
	};

	for (const auto &[price, orders] : bids_)
		bidInfos.push_back(CreateLevelInfos(price, orders));

	for (const auto &[price, orders] : asks_)
		askInfos.push_back(CreateLevelInfos(price, orders));

	return OrderbookLevelInfos{bidInfos, askInfos};
}