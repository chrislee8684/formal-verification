// Markov Decision Process: nondeterminism (choices) + probability (randomness)
mdp

//////////////////////////////////////////////////////////////////////
// CONSTANTS: bound variables (finite state space)
//////////////////////////////////////////////////////////////////////

// Allowed range of mid price: [0, 200]
const int MID_MIN   = 90;
const int MID_MAX   = 110;

// Allowed range of quoted prices (bid, ask): [0, 200]
const int PRICE_MIN = 0;
const int PRICE_MAX = 200;

// Inv limit limits state space size, but max limit defines acceptable limit
// Allowed range of inventory: [-100, 100]
const int INV_MIN = -100;
const int INV_MAX =  100;

// Allowed risk limit of inventory: 1000 shares at most
const int MAX_LIMIT = 100;
const int MIN_LIMIT = -100; // don't wait to own shares before selling

//////////////////////////////////////////////////////////////////////
// MODULE: state variables and transitions
//////////////////////////////////////////////////////////////////////

module market

    ////////////////////////////////////////////////////////////////
    // STATE VARIABLES: variables that make up a state
    ////////////////////////////////////////////////////////////////

    // mid price
    mid : [MID_MIN..MID_MAX] init 100;

    // quotes
    bid : [PRICE_MIN..PRICE_MAX] init 99;
    ask : [PRICE_MIN..PRICE_MAX] init 101;

    // inventory (net position)
    inv : [INV_MIN..INV_MAX] init 0;

    // order state: tracks lifecycle of a quote (bid and ask seprately tracked)
    // 0 = NONE
    // 1 = ACTIVE
    // 2 = CANCEL_SENT
    // 3 = FILLED
    // 4 = CANCELED
    bid_state : [0..4] init 0;
    ask_state : [0..4] init 0;

    // signals
    market_data  : bool init false;
    bid_quote_posted : bool init false;
    ask_quote_posted : bool init false;

    // liveness obligations
    pending_bid_quote  : bool init false;
    pending_bid_cancel : bool init false;
    pending_ask_quote  : bool init false;
    pending_ask_cancel : bool init false;

    ////////////////////////////////////////////////////////////////
    // MARKET DATA ARRIVAL → CREATE QUOTE OBLIGATION
    ////////////////////////////////////////////////////////////////

    [md_update]
!market_data & !pending_bid_quote & !pending_ask_quote & bid_state=0 & ask_state=0 ->
  0.5 : (market_data' = true)
      & (mid' = min(MID_MAX, mid + 1))
      & (pending_bid_quote' = (inv < MAX_LIMIT))
      & (pending_ask_quote' = (inv > MIN_LIMIT))
      & (bid_quote_posted' = false)
      & (ask_quote_posted' = false)
+ 0.5 : (market_data' = true)
      & (mid' = max(MID_MIN, mid - 1))
      & (pending_bid_quote' = (inv < MAX_LIMIT))
      & (pending_ask_quote' = (inv > MIN_LIMIT))
      & (bid_quote_posted' = false)
      & (ask_quote_posted' = false);



    ////////////////////////////////////////////////////////////////
    // POST QUOTE → DISCHARGE QUOTE OBLIGATION
    ////////////////////////////////////////////////////////////////

   [post_bid]
market_data & pending_bid_quote & inv < MAX_LIMIT & bid_state = 0 ->
  1.0 : (bid' = max(PRICE_MIN, min(mid - 1, ask - 1)))
      & (bid_state' = 1)
      & (bid_quote_posted' = true)
      & (pending_bid_quote' = false);

[post_ask]
market_data & pending_ask_quote & inv > MIN_LIMIT & ask_state = 0 ->
  1.0 : (ask' = min(PRICE_MAX, max(mid + 1, bid + 1)))
      & (ask_state' = 1)
      & (ask_quote_posted' = true)
      & (pending_ask_quote' = false);


    [finish_quote] // necessary as market_data is a shared signal for bid and ask
    market_data & !pending_bid_quote & !pending_ask_quote ->
      1.0 : (market_data' = false)
          & (bid_quote_posted' = false)
          & (ask_quote_posted' = false);

    ////////////////////////////////////////////////////////////////
    // SEND CANCEL → CREATE CANCEL OBLIGATION
    ////////////////////////////////////////////////////////////////

    [send_cancel]
    bid_state = 1 ->
      1.0 : (bid_state'      = 2)
          & (pending_bid_cancel' = true);

    [send_cancel]
    ask_state = 1 ->
      1.0 : (ask_state'      = 2)
          & (pending_ask_cancel' = true);

    ////////////////////////////////////////////////////////////////
    // RESOLVE CANCEL (SAFE, TOTAL PROBABILITY = 1)
    // → DISCHARGE CANCEL OBLIGATION (either fill or cancel)
    ////////////////////////////////////////////////////////////////

    // Fill or cancel if inventory below max
    [resolve_cancel]
    bid_state = 2 & inv < MAX_LIMIT ->
      0.5 : (bid_state'      = 3)
          & (inv'            = inv + 1)
          & (pending_bid_cancel' = false)
    + 0.5 : (bid_state'      = 4)
          & (pending_bid_cancel' = false);
    
    [resolve_cancel]
    ask_state = 2 & inv > MIN_LIMIT ->
      0.5 : (ask_state'      = 3)
          & (inv'            = inv - 1)
          & (pending_ask_cancel' = false)
    + 0.5 : (ask_state'      = 4)
          & (pending_ask_cancel' = false);

    // Must cancel if inventory already at max
    [resolve_cancel]
    bid_state = 2 & inv = MAX_LIMIT ->
      1.0 : (bid_state'      = 4)
          & (pending_bid_cancel' = false);
    
    [resolve_cancel]
    ask_state = 2 & inv = MIN_LIMIT ->
      1.0 : (ask_state'      = 4)
          & (pending_ask_cancel' = false);
    
    ////////////////////////////////////////////////////////////////
    // RECYCLE QUOTES → RESET TO NONE IF FILLED OR CANCELLED
    ////////////////////////////////////////////////////////////////

    [recycle_bid]
    bid_state = 3 | bid_state = 4 ->
      1.0 : (bid_state' = 0);

    [recycle_ask]
    ask_state = 3 | ask_state = 4 ->
      1.0 : (ask_state' = 0);

endmodule
