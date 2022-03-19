#include "OrderBook.h"

int main() {
  using namespace orderbook;
  OrderBook<SingleOrder> book{};
  auto dumpfn = [](const SingleOrder &order) {
    std::cout << "oid: " << order.order_id;
    if (order.side == Side::Buy) {
      std::cout << " Buy  px: " << order.price;
    } else {
      std::cout << " Sell px: " << order.price;
    }
    std::cout << " size: " << order.size << "\n";
  };

  // Test 1: Insert, Mod, Del
  auto od = SingleOrder(500, 8, 12345, Side::Buy);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      book.newLimitOrder(od);
      od.order_id++;
      od.size++;
    }
    od.price--;
  }
  od.side = Side::Sell;
  od.price = 501;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      book.newLimitOrder(od);
      od.order_id++;
      od.size++;
    }
    od.price++;
  }
  book.removeOrder(12222);
  book.removeOrder(12359);
  book.removeOrder(12345);
  od = SingleOrder(501, 8, 12356, Side::Sell);
  book.modifyOrder(od);
  std::cout << "---------Test 1 ------\n";
  book.dumpBook(dumpfn);
  // Limit Order cross
  od = SingleOrder(502, 20, 13000, Side::Buy);
  book.newLimitOrder(od);
  std::cout << "---------Test 2 ------\n";
  book.dumpBook(dumpfn);
  od = SingleOrder(499, 35, 13001, Side::Sell);
  book.newLimitOrder(od);
  book.dumpBook(dumpfn);
  std::cout << "---------Test 3 ------\n";
  book.newMarketOrder(Side::Sell, 25000, 19996);
  book.dumpBook(dumpfn);
  std::cout << "---------Test 4 ------\n";
  od = SingleOrder(502, 35, 13006, Side::Noside);
  book.tradeMessage(od);
  book.dumpBook(dumpfn);
  book.cleanUp();
  std::cout << "---------Test 5 ------\n";
  od = SingleOrder(500, 8, 12345, Side::Buy);
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      book.newLimitOrder(od);
      od.order_id++;
      od.size++;
    }
    od.price--;
  }
  od.side = Side::Sell;
  od.price = 501;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      book.newLimitOrder(od);
      od.order_id++;
      od.size++;
    }
    od.price++;
  }
  od = SingleOrder(498, 35, 13006, Side::Noside);
  book.tradeMessage(od);
  book.dumpBook(dumpfn);
  // book.dumpBook([] (const SingleOrder &order) {
  //   std::cout <<"prc: " << order.price << " size: " << order.size << "\n";
  // });
}
