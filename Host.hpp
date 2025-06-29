#pragma once

#include "TradingEngineServer.hpp"
#include <memory>

class Host {
	/*
	 * Host is a class that manages the TradingEngineServer.
	 * It provides methods to start and stop the server.
	 * The server is expected to be passed as a shared pointer, allowing for easy management of its lifetime.
	 * If the server is not initialized, it throws an exception when trying to start or stop it.
	 */
  public:
	Host(std::shared_ptr<TradingEngineServer> tradingEngineServer)
		: m_tradingEngineServer(tradingEngineServer) {}

	void start() {
		if (m_tradingEngineServer) {
			m_tradingEngineServer->start();
		} else {
			throw std::runtime_error("TradingEngineServer is not initialized.");
		}
	}
	void stop() {
		if (m_tradingEngineServer) {
			m_tradingEngineServer->stop();
		} else {
			throw std::runtime_error("TradingEngineServer is not initialized.");
		}
	}

  private:
	std::shared_ptr<TradingEngineServer> m_tradingEngineServer;
};