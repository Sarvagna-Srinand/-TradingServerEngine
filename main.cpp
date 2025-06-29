#include "Orderbook.hpp"
#include "TradingEngineServer.hpp"
#include <grpcpp/grpcpp.h>

int main() {
	std::string server_address("0.0.0.0:5001");
	std::shared_ptr<Orderbook> orderbook = std::make_shared<Orderbook>();

	TradingEngineServer service(orderbook);

	grpc::ServerBuilder builder;
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	builder.RegisterService(&service);

	std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
	if (server) {
		std::cout << "Server listening on " << server_address << std::endl;
		server->Wait();
	} else {
		std::cerr << "Failed to start server." << std::endl;
		return 1;
	}

	return 0;
}
