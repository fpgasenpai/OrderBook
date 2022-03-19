#pragma once
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <unordered_map>

namespace orderbook {

typedef uint64_t price_t;
typedef uint64_t order_size_t;
typedef uint64_t order_id_t;
enum class Side : uint8_t { Noside = 0, Buy = 1, Sell = 2 };

struct SingleOrder {
  price_t price;
  order_size_t size;
  order_id_t order_id;
  Side side;
  SingleOrder(price_t p, order_size_t s, order_id_t id, Side sd)
      : price(p), size(s), order_id(id), side(sd) {}
};

template <typename OrderT> class OrderBook {
public:
  OrderBook() {}

  ~OrderBook() { cleanUp(); }

  void cleanUp() {
    orders_.clear();
    for (auto pl = sell_book_.begin(); pl != sell_book_.end();) {
      pl->second.orders.clear();
      pl = sell_book_.erase(pl);
    }
    sell_book_.clear();
    for (auto pl = buy_book_.begin(); pl != buy_book_.end();) {
      pl->second.orders.clear();
      pl = buy_book_.erase(pl);
    }
    buy_book_.clear();
  }

  // Stream out sorted orders fast for downstream
  void dumpBook(const std::function<void(const OrderT &order)> &handler) {
    for (auto pl = sell_book_.begin(); pl != sell_book_.end(); ++pl) {
      for (auto od = pl->second.orders.begin(); od != pl->second.orders.end(); ++od) {
        handler(*od);
      }
    }
    for (auto pl = buy_book_.rbegin(); pl != buy_book_.rend(); ++pl) {
      for (auto od = pl->second.orders.begin(); od != pl->second.orders.end(); ++od) {
        handler(*od);
      }
    }
  }

  // Process limitOrder
  void newLimitOrder(OrderT &order) {
    auto &crossbook = (order.side == Side::Buy) ? sell_book_ : buy_book_;
    bool crosschecking = !crossbook.empty();
    while (crosschecking && order.size > 0) {
      crosschecking = false;
      auto &o1 = (order.side == Side::Buy) ? *crossbook.begin()->second.orders.begin()
                                           : *crossbook.rbegin()->second.orders.begin();
      if ((order.side == Side::Buy && order.price >= o1.price) ||
          (order.side == Side::Sell && order.price <= o1.price)) {
        std::cout << "NewLimit Match with " << o1.order_id << "\n";
        if (order.size >= o1.size) {
          order.size -= o1.size;
          removeOrder(o1.order_id);
          crosschecking = !crossbook.empty();
        } else {
          o1.size -= order.size;
          order.size = 0;
        }
      }
    }
    if (order.size > 0) {
      insertOrder(order);
    }
  }

  // void newLimitOrderIOC(OrderT order) {} /* no plan to do this*/

  // Cancel Order
  void removeOrder(order_id_t order_id) {
    auto ordi = orders_.find(order_id);
    if (ordi == orders_.end()) {
      std::cout << "Order not found: " << order_id << "\n";
      return;
    }
    auto &order = *ordi->second;
    auto &book = (order.side == Side::Buy) ? buy_book_ : sell_book_;
    auto pl = book.find(order.price); // Find Price level
    if (pl != book.end()) {
      pl->second.num_orders -= 1;
      pl->second.level_size -= order.size;
      pl->second.orders.erase(ordi->second);
      if (pl->second.num_orders == 0) {
        book.erase(pl);
      }
    }
    orders_.erase(ordi);
  }

  // As exchange book, remove size until fund deplete
  void newMarketOrder(Side takerside, price_t fund, order_id_t order_id) {
    auto &crossbook = (takerside == Side::Buy) ? sell_book_ : buy_book_;
    bool crosschecking = !crossbook.empty();
    while (crosschecking && fund > 0) {
      auto &o1 = (takerside == Side::Buy) ? *crossbook.begin()->second.orders.begin()
                                          : *crossbook.rbegin()->second.orders.begin();
      std::cout << "MktordMatch! at $" << o1.price << " with " << o1.order_id << "\n";
      if (fund >= o1.price * o1.size) {
        fund -= o1.price * o1.size;
        removeOrder(o1.order_id);
      } else {
        o1.size -= fund / o1.price;
        fund = 0;
      }
      crosschecking = !crossbook.empty();
    }
    if (fund != 0) {
      std::cout << "Market Order Partially filled\n";
    }
  }

  // As someone ngmi, only same side and move back
  void modifyOrder(OrderT &order) {
    auto ordi = orders_.find(order.order_id);
    if (ordi == orders_.end()) {
      std::cout << "Order Not Found: " << order.order_id << "\n";
      return;
    }
    // Check illegal mods
    auto &ob = *ordi->second;
    if ((order.side != ob.side) || (order.price == ob.price && order.size > ob.size)) {
      std::cout << "Mod reject 1\n";
      return;
    }
    if ((order.side == Side::Buy && order.price > ob.price) ||
        (order.side == Side::Sell && order.price < ob.price)) {
      std::cout << "Mod reject 2\n";
      return;
    }
    if (order.price == ob.price) {
      ob.size = order.size;
    } else {
      removeOrder(order.order_id);
      insertOrder(order);
    }
  }

  // As HFT book, remove size @ said price
  void tradeMessage(OrderT &msg) {
    auto pl = buy_book_.find(msg.price);
    Side side{Side::Buy};
    if (pl == buy_book_.end()) {
      pl = sell_book_.find(msg.price);
      side = Side::Sell;
      if (pl == sell_book_.end()) {
        std::cout << "Can not find trade level\n";
        return;
      }
    }
    // Del Levels crossed trade price
    if (side == Side::Buy) {
      for (auto cpl = buy_book_.rbegin(); cpl->first > msg.price; cpl = buy_book_.rbegin()) {
        for (auto od1 = cpl->second.orders.begin(); od1 != cpl->second.orders.end(); ++od1) {
          std::cout << "LvlDel Ord: " << od1->order_id << "\n";
          orders_.erase(od1->order_id);
        }
        cpl->second.orders.clear();
        buy_book_.erase(--cpl.base());
      }
    } else {
      for (auto cpl = sell_book_.begin(); cpl->first < msg.price;) {
        for (auto od1 = cpl->second.orders.begin(); od1 != cpl->second.orders.end(); ++od1) {
          std::cout << "LvlDel Ord: " << od1->order_id << "\n";
          orders_.erase(od1->order_id);
        }
        cpl->second.orders.clear();
        cpl = sell_book_.erase(cpl);
      }
    }
    // Match with orders in the price level
    for (auto od = pl->second.orders.begin(); od != pl->second.orders.end();) {
      if (od->size <= msg.size) {
        std::cout << "TradeFill Ord: " << od->order_id << "\n";
        pl->second.num_orders--;
        pl->second.level_size -= od->size;
        msg.size -= od->size;
        orders_.erase(od->order_id);
        od = pl->second.orders.erase(od);
      } else {
        std::cout << "TradePartialFill Ord: " << od->order_id << "\n";
        pl->second.level_size -= msg.size;
        od->size -= msg.size;
        return;
      }
    }
    if (pl->second.num_orders == 0) {
      auto &book = (side == Side::Buy) ? buy_book_ : sell_book_;
      book.erase(pl);
    }
  }

private:
  struct PriceLevel {
    uint64_t num_orders;
    order_size_t level_size;
    std::list<OrderT> orders;
  };

  typedef typename std::list<OrderT>::iterator ListNode;
  typedef std::map<price_t, PriceLevel> SideBook;
  typedef std::unordered_map<order_id_t, ListNode> OrderHash;

  SideBook buy_book_;
  SideBook sell_book_;
  OrderHash orders_;

  void insertOrder(OrderT &order) {
    if (orders_.find(order.order_id) != orders_.end()) {
      std::cout << "Duplicate order in add: " << order.order_id << "\n";
      return;
    }
    SideBook &book = (order.side == Side::Buy) ? buy_book_ : sell_book_;
    auto pl = book.find(order.price); // Find Price level
    if (pl != book.end()) {
      pl->second.orders.push_back(order);
      pl->second.num_orders += 1;
      pl->second.level_size += order.size;
      auto lastnode = pl->second.orders.end();
      orders_[order.order_id] = --lastnode;
    } else { // New Price Level
      auto dd = book.insert({order.price, PriceLevel()});
      dd.first->second.orders.push_back(order);
      dd.first->second.num_orders = 1;
      dd.first->second.level_size = order.size;
      orders_[order.order_id] = dd.first->second.orders.begin();
    }
  }
};

} // namespace orderbook
