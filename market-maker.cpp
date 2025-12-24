#include <iostream>
#include <random>
#include <cmath>
#include <iomanip>
#include <cstdlib>

// ---------- Market state ----------

struct MarketState {
    double mid_price;
    double spread;
};

// ---------- Order state machine ----------

struct Order {
    enum class State {
        NONE,           // no order on this side
        ACTIVE,         // working at the exchange
        PENDING_CANCEL, // cancel sent, waiting for fill-before-cancel or cancel confirm
        CANCELED,       // cancel confirmed
        FILLED          // fully filled
    };

    State state = State::NONE;
    double price = 0.0;
};

// ---------- Market maker ----------

struct MarketMaker {
    double inventory = 0.0;        // number of shares
    double cash = 0.0;             // cash PnL
    double fair_value = 100.0;     // internal fair value estimate
    double base_spread = 0.2;      // base spread (dollars)
    double inventory_limit = 10000.; // max absolute inventory
    double gamma = 0.01;           // inventory aversion parameter (controls how strongly quotes are adjusted based on current inventory)

    Order bid_order;
    Order ask_order;

    // Compute desired quotes and (re)place ACTIVE orders
    void quote(const MarketState& mkt) {
        // Inventory penalty: if long, shift quotes down to encourage selling;
        // if short, shift quotes up to encourage buying.
        double inv_penalty = gamma * inventory;
        double effective_mid = mkt.mid_price - inv_penalty;
        double half_spread = base_spread / 2.0;

        double desired_bid = effective_mid - half_spread;
        double desired_ask = effective_mid + half_spread;

        // Optional safety: don't cross mid too aggressively
        if (desired_bid > mkt.mid_price)  desired_bid = mkt.mid_price - 0.01;
        if (desired_ask < mkt.mid_price)  desired_ask = mkt.mid_price + 0.01;

        // Hard inventory cuts: if too long, stop bidding; if too short, stop offering
        if (inventory >= inventory_limit) {
            // No bid if we are too long
            if (bid_order.state == Order::State::ACTIVE)
                request_cancel(bid_order);
        } else {
            // Place or refresh bid if we don't have an active one
            if (bid_order.state == Order::State::NONE ||
                bid_order.state == Order::State::CANCELED ||
                bid_order.state == Order::State::FILLED)
            {
                bid_order.state = Order::State::ACTIVE;
                bid_order.price = desired_bid;
            }
        }

        if (inventory <= -inventory_limit) {
            // No ask if we are too short
            if (ask_order.state == Order::State::ACTIVE)
                request_cancel(ask_order);
        } else {
            // Place or refresh ask if we don't have an active one
            if (ask_order.state == Order::State::NONE ||
                ask_order.state == Order::State::CANCELED ||
                ask_order.state == Order::State::FILLED)
            {
                ask_order.state = Order::State::ACTIVE;
                ask_order.price = desired_ask;
            }
        }
    }

    // Request cancel for an ACTIVE order
    void request_cancel(Order& o) {
        if (o.state == Order::State::ACTIVE) {
            o.state = Order::State::PENDING_CANCEL;
        }
    }

    // Update inventory and cash on a fill
    // quantity > 0 : we buy quantity (hit our bid)
    // quantity < 0 : we sell |quantity| (lift our ask)
    void handle_fill(double price, int quantity) {
        inventory += quantity;
        cash -= price * quantity; // buying reduces cash, selling increases
    }
};

// ---------- Simple midprice dynamics ----------

void update_midprice(MarketState& mkt, std::mt19937_64& rng) {
    std::normal_distribution<double> d_price(0.0, 0.05); // small noise
    double shock = d_price(rng);
    mkt.mid_price = std::max(0.01, mkt.mid_price + shock);
}

// ---------- Fill simulation for ACTIVE orders ----------

bool maybe_fill_active_order(const Order& o,
                             const MarketState& mkt,
                             std::mt19937_64& rng,
                             bool is_bid_side)
{
    if (o.state != Order::State::ACTIVE)
        return false;

    double dist = is_bid_side
        ? (mkt.mid_price - o.price)   // bid: smaller => more aggressive
        : (o.price - mkt.mid_price);  // ask: smaller => more aggressive

    if (dist < 0.0) {
        // Crossed quotes shouldn't really happen in this toy model,
        // but just avoid nonsense.
        dist = 0.0;
    }

    double intensity = std::exp(-dist * 10.0); // more aggressive => higher intensity
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    double u = uni(rng);
    return u < intensity;
}

// ---------- Cancel resolution: fill-before-cancel OR cancel-confirm ----------

void resolve_pending_cancel(Order& o,
                            MarketMaker& mm,
                            const MarketState& mkt,
                            std::mt19937_64& rng,
                            bool is_bid_side,
                            int qty_per_fill)
{
    if (o.state != Order::State::PENDING_CANCEL)
        return;

    // Model: once cancel is pending, either:
    //  - The order gets filled before the cancel is processed, OR
    //  - The exchange confirms the cancel.
    //
    // We ensure we ALWAYS transition to FILLED or CANCELED in this function,
    // never leaving the state as PENDING_CANCEL at the end of the step.

    std::bernoulli_distribution fill_before_cancel(0.3); // 30% chance fill happens first
    if (fill_before_cancel(rng)) {
        // Filled before cancel completes
        int qty = is_bid_side ? +qty_per_fill : -qty_per_fill;
        mm.handle_fill(o.price, qty);
        o.state = Order::State::FILLED;
    } else {
        // Cancel confirmation, no trade
        o.state = Order::State::CANCELED;
    }
}

// Runtime check that models the property:
// "If a cancel request is sent out, either the order should get filled before
// cancellation or receive a cancel confirmation."
void assert_no_lost_cancels(const Order& o) {
    if (o.state == Order::State::PENDING_CANCEL) {
        std::cerr << "ERROR: Lost cancel! Order is still PENDING_CANCEL "
                     "at end of step.\n";
        std::exit(1);
    }
}

int main() {
    // RNG
    std::mt19937_64 rng(42); // fixed seed for reproducibility

    // Initial market state
    MarketState mkt;
    mkt.mid_price = 100.0;
    mkt.spread = 0.2;

    // Market maker
    MarketMaker mm;

    const int n_steps = 10000;
    const int qty_per_fill = 1;

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "step, mid, bid_px, bid_state, ask_px, ask_state, inv, cash, pnl\n";

    for (int t = 0; t < n_steps; ++t) {
        // 1) Update midprice
        update_midprice(mkt, rng);

        // 2) Compute desired quotes / request cancels based on inventory
        mm.quote(mkt);

        // 3) Simulate fills for ACTIVE orders
        //    (before we resolve pending cancels)
        if (maybe_fill_active_order(mm.bid_order, mkt, rng, true)) {
            mm.handle_fill(mm.bid_order.price, +qty_per_fill);
            mm.bid_order.state = Order::State::FILLED;
        }
        if (maybe_fill_active_order(mm.ask_order, mkt, rng, false)) {
            mm.handle_fill(mm.ask_order.price, -qty_per_fill);
            mm.ask_order.state = Order::State::FILLED;
        }

        // 4) Resolve any PENDING_CANCEL orders:
        //    either filled-before-cancel or cancel-confirmation.
        resolve_pending_cancel(mm.bid_order, mm, mkt, rng, true,  qty_per_fill);
        resolve_pending_cancel(mm.ask_order, mm, mkt, rng, false, qty_per_fill);

        // 5) Check the "no lost cancel" property:
        //    no order may remain in PENDING_CANCEL at the end of a step.
        assert_no_lost_cancels(mm.bid_order);
        assert_no_lost_cancels(mm.ask_order);

        // 6) Compute mark-to-market PnL
        double pnl = mm.cash + mm.inventory * mkt.mid_price;

        auto state_to_char = [](Order::State s) {
            switch (s) {
                case Order::State::NONE:           return 'N';
                case Order::State::ACTIVE:         return 'A';
                case Order::State::PENDING_CANCEL: return 'P'; // should never appear in print
                case Order::State::CANCELED:       return 'C';
                case Order::State::FILLED:         return 'F';
            }
            return '?';
        };

        if (t % 50 == 0 || t == n_steps - 1) {
            std::cout << t << ", "
                      << mkt.mid_price << ", "
                      << mm.bid_order.price << ", "
                      << state_to_char(mm.bid_order.state) << ", "
                      << mm.ask_order.price << ", "
                      << state_to_char(mm.ask_order.state) << ", "
                      << mm.inventory << ", "
                      << mm.cash << ", "
                      << pnl << "\n";
        }

        // After logging, recycle FILLED/CANCELED orders into NONE
        if (mm.bid_order.state == Order::State::FILLED ||
            mm.bid_order.state == Order::State::CANCELED)
        {
            mm.bid_order.state = Order::State::NONE;
        }
        if (mm.ask_order.state == Order::State::FILLED ||
            mm.ask_order.state == Order::State::CANCELED)
        {
            mm.ask_order.state = Order::State::NONE;
        }
    }

    std::cout << "\nFinal state:\n";
    std::cout << "Inventory: " << mm.inventory << " shares\n";
    std::cout << "Cash:      " << mm.cash << "\n";
    std::cout << "Midprice:  " << mkt.mid_price << "\n";
    std::cout << "PnL:       " << mm.cash + mm.inventory * mkt.mid_price << "\n";

    return 0;
}
