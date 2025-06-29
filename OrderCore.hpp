#pragma once
#include <stdexcept>
#include <string>

class IOrderCore {
  public:
	virtual ~IOrderCore() = default;

	virtual long getOrderId() const = 0;
	virtual int getSecurityId() const = 0;
	virtual const std::string &getUsername() const = 0;
};

class OrderCore : public IOrderCore {
  public:
	OrderCore(long orderId, int securityId, const std::string &username)
		: orderId(orderId), securityId(securityId), username(username) {}

	long getOrderId() const override {
		return orderId;
	}
	int getSecurityId() const override {
		return securityId;
	}
	const std::string &getUsername() const override {
		return username;
	}

  private:
	long orderId;
	int securityId;
	std::string username;
};

class Order : public IOrderCore {
  public:
	Order(OrderCore &&core, long price, uint quantity, bool isBuy)
		: core(std::move(core)),
		  price(price),
		  initialQuantity(quantity),
		  currentQuantity(quantity),
		  isBuy(isBuy) {
		orderId = core.getOrderId();
	}

	long getOrderId() const override {
		return core.getOrderId();
	}
	int getSecurityId() const override {
		return core.getSecurityId();
	}
	const std::string &getUsername() const override {
		return core.getUsername();
	}
	long getPrice() const {
		return price;
	}
	uint getInitialQuantity() const {
		return initialQuantity;
	}
	uint getCurrentQuantity() const {
		return currentQuantity;
	}
	bool isBuyOrder() const {
		return isBuy;
	}
	void increaseQuantity(uint quantityDelta) {
		currentQuantity += quantityDelta;
	}
	void decreaseQuantity(uint quantityDelta) {
		if (quantityDelta > currentQuantity) {
			throw std::out_of_range("Cannot decrease quantity below zero");
		}
		currentQuantity -= quantityDelta;
	}

  private:
	OrderCore core;
	long price;
	uint initialQuantity;
	uint currentQuantity;
	bool isBuy;
};