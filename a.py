import grpc
import trading_pb2
import trading_pb2_grpc
import time
import random
from concurrent.futures import ThreadPoolExecutor

def create_order_request(order_id, side, price, quantity, order_type):
    return trading_pb2.OrderRequest(
        order_id=order_id,
        client_id="TEST_CLIENT",
        side=side,
        price=price,
        quantity=quantity,
        order_type=order_type,
        instrument_id="AAPL"
    )

class TradingEngineClient:
    def __init__(self, address='localhost:5001'):
        self.channel = grpc.insecure_channel(address)
        self.stub = trading_pb2_grpc.TradingEngineStub(self.channel)
        
    def add_order(self, order_request):
        try:
            response = self.stub.AddOrder(order_request)
            print(f"Order {order_request.order_id} status: {trading_pb2.OrderStatus.Name(response.status)}")
            if response.trades:
                print(f"Trades executed: {len(response.trades)//2}")
                for i in range(0, len(response.trades), 2):
                    print(f"Trade: Price={response.trades[i].price}, Quantity={response.trades[i].quantity}")
            return response
        except grpc.RpcError as e:
            print(f"Error adding order: {e}")
            return None

    def modify_order(self, order_id, new_price, new_quantity, side):
        request = trading_pb2.ModifyOrderRequest(
            order_id=order_id,
            client_id="TEST_CLIENT",
            new_price=new_price,
            new_quantity=new_quantity,
            side=side
        )
        try:
            response = self.stub.ModifyOrder(request)
            print(f"Modified order {order_id}: {trading_pb2.OrderStatus.Name(response.status)}")
            return response
        except grpc.RpcError as e:
            print(f"Error modifying order: {e}")
            return None

    def cancel_order(self, order_id):
        request = trading_pb2.CancelOrderRequest(
            order_id=order_id,
            client_id="TEST_CLIENT"
        )
        try:
            response = self.stub.CancelOrder(request)
            print(f"Cancelled order {order_id}: {response.success}")
            return response
        except grpc.RpcError as e:
            print(f"Error cancelling order: {e}")
            return None

    def get_orderbook(self):
        request = trading_pb2.OrderbookRequest(
            instrument_id="AAPL",
            depth=10
        )
        try:
            response = self.stub.GetOrderbook(request)
            print("\nOrderbook:")
            print("Asks:")
            for ask in reversed(response.asks):
                print(f"  {ask.price}: {ask.quantity}")
            print("Bids:")
            for bid in response.bids:
                print(f"  {bid.price}: {bid.quantity}")
            return response
        except grpc.RpcError as e:
            print(f"Error getting orderbook: {e}")
            return None

def test_matching_scenario():
    client = TradingEngineClient()
    
    # Add buy order
    buy_order = create_order_request(
        order_id=1,
        side=trading_pb2.Side.BUY,
        price=100.0,
        quantity=1000,
        order_type=trading_pb2.OrderType.GOOD_TILL_CANCEL
    )
    client.add_order(buy_order)
    
    # Add matching sell order
    sell_order = create_order_request(
        order_id=2,
        side=trading_pb2.Side.SELL,
        price=100.0,
        quantity=500,
        order_type=trading_pb2.OrderType.GOOD_TILL_CANCEL
    )
    client.add_order(sell_order)
    
    # Check orderbook
    client.get_orderbook()
    
    # Modify remaining buy order
    client.modify_order(1, 99.0, 500, trading_pb2.Side.BUY)
    
    # Check orderbook again
    client.get_orderbook()
    
    # Cancel remaining order
    client.cancel_order(1)
    
    # Final orderbook check
    client.get_orderbook()

if __name__ == "__main__":
    print("Starting trading engine test client...")
    test_matching_scenario()