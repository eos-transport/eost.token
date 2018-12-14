/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include <eost.token/eost.token.hpp>

namespace eost {

    void token::create( name   issuer,
                        asset  maximum_supply,
                        bool transfer_locked)
    {
        require_auth( _self );

        auto sym = maximum_supply.symbol;
        eosio_assert( sym.is_valid(), "invalid symbol name" );
        eosio_assert( maximum_supply.is_valid(), "invalid supply");
        eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

        stats statstable( _self, sym.code().raw() );
        auto existing = statstable.find( sym.code().raw() );
        eosio_assert( existing == statstable.end(), "token with symbol already exists" );

        statstable.emplace( _self, [&]( auto& s ) {
           s.supply.symbol = maximum_supply.symbol;
           s.max_supply    = maximum_supply;
           s.issuer        = issuer;
            s.transfer_locked = transfer_locked;
        });
    }


    void token::issue( name to, asset quantity, string memo, uint8_t is_lock, uint64_t lock_time )
    {
        auto sym = quantity.symbol;
        eosio_assert( sym.is_valid(), "invalid symbol name" );
        eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

        stats statstable( _self, sym.code().raw() );
        auto existing = statstable.find( sym.code().raw() );
        eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
        const auto& st = *existing;

        require_auth( st.issuer );
        eosio_assert( quantity.is_valid(), "invalid quantity" );
        eosio_assert( quantity.amount > 0, "must issue positive quantity" );

        eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
        eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

        statstable.modify( st, same_payer, [&]( auto& s ) {
           s.supply += quantity;
        });

        add_balance( st.issuer, quantity, st.issuer );

        if( to != st.issuer ) {
          SEND_INLINE_ACTION( *this, transfer, { {st.issuer, "active"_n} },
                              { st.issuer, to, quantity, memo }
          );
        }

        if(is_lock > 0){
            auto payer = has_auth( to ) ? to : st.issuer;
            lock(to,quantity,lock_time,payer);
        }
    }

    void token::retire( asset quantity, string memo )
    {
        auto sym = quantity.symbol;
        eosio_assert( sym.is_valid(), "invalid symbol name" );
        eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

        stats statstable( _self, sym.code().raw() );
        auto existing = statstable.find( sym.code().raw() );
        eosio_assert( existing != statstable.end(), "token with symbol does not exist" );
        const auto& st = *existing;

        require_auth( st.issuer );
        eosio_assert( quantity.is_valid(), "invalid quantity" );
        eosio_assert( quantity.amount > 0, "must retire positive quantity" );

        eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

        statstable.modify( st, same_payer, [&]( auto& s ) {
           s.supply -= quantity;
        });

        sub_balance( st.issuer, quantity );
    }

    void token::transfer( name    from,
                          name    to,
                          asset   quantity,
                          string  memo )
    {
        eosio_assert( from != to, "cannot transfer to self" );
        require_auth( from );
        eosio_assert(is_locked(from,quantity) == false,"must not lock");
        eosio_assert( is_account( to ), "to account does not exist");
        auto sym = quantity.symbol.code();
        stats statstable( _self, sym.raw() );
        const auto& st = statstable.get( sym.raw() );

        if(st.transfer_locked) {
            require_auth(st.issuer);
        }

        require_recipient( from );
        require_recipient( to );

        eosio_assert( quantity.is_valid(), "invalid quantity" );
        eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
        eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
        eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

        auto payer = has_auth( to ) ? to : from;

        sub_balance( from, quantity );
        add_balance( to, quantity, payer );
    }

    void token::sub_balance( name owner, asset value ) {
       accounts from_acnts( _self, owner.value );

       const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
       eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );

       from_acnts.modify( from, owner, [&]( auto& a ) {
             a.balance -= value;
          });
    }

    void token::add_balance( name owner, asset value, name ram_payer )
    {
       accounts to_acnts( _self, owner.value );
       auto to = to_acnts.find( value.symbol.code().raw() );
       if( to == to_acnts.end() ) {
          to_acnts.emplace( ram_payer, [&]( auto& a ){
            a.balance = value;
          });
       } else {
          to_acnts.modify( to, same_payer, [&]( auto& a ) {
            a.balance += value;
          });
       }
    }

    void token::open( name owner, const symbol& symbol, name ram_payer )
    {
       require_auth( ram_payer );

       auto sym_code_raw = symbol.code().raw();

       stats statstable( _self, sym_code_raw );
       const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
       eosio_assert( st.supply.symbol == symbol, "symbol precision mismatch" );

       accounts acnts( _self, owner.value );
       auto it = acnts.find( sym_code_raw );
       if( it == acnts.end() ) {
          acnts.emplace( ram_payer, [&]( auto& a ){
            a.balance = asset{0, symbol};
          });
       }
    }

    void token::close( name owner, const symbol& symbol )
    {
       require_auth( owner );
       accounts acnts( _self, owner.value );
       auto it = acnts.find( symbol.code().raw() );
       eosio_assert( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
       eosio_assert( it->balance.amount == 0, "Cannot close because the balance is not zero." );
       acnts.erase( it );
    }

    void token::cantransfer(asset quantity, bool is_transfer){

        auto sym = quantity.symbol;
        eosio_assert( sym.is_valid(), "invalid symbol name" );

        stats statstable( _self, sym.code().raw() );
        auto existing = statstable.find( sym.code().raw() );
        eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
        const auto& st = *existing;

        require_auth(st.issuer);

        statstable.modify(st, same_payer, [&](auto &s) {
            s.transfer_locked = is_transfer;
        });
    }

    void token::lock(name to, asset quantity, uint64_t lock_time,name ram_payer){
        auto sym = quantity.symbol;
        lockers to_lockers( _self, to.value );
        auto locker = to_lockers.find( quantity.symbol.code().raw() );

        eosio_assert( locker == to_lockers.end(), "locker must not exist" );
        time_point t {microseconds(current_time() + lock_time)};
        to_lockers.emplace( ram_payer, [&]( auto& a ){
            a.balance = quantity;
            a.unlock_time = t;
            a.lock_time = current_time_point();
        });

    }

    bool token::is_locked(name owner, asset   quantity){
        auto sym = quantity.symbol;
        lockers to_lockers( _self, owner.value );
        auto locker = to_lockers.find( quantity.symbol.code().raw() );
        if(locker == to_lockers.end()){
            return false;
        }
        const auto& lk = *locker;
        return (current_time_point() < lk.unlock_time);
    }

    time_point token::current_time_point() {
        const static time_point ct{ microseconds{ static_cast<int64_t>( current_time() ) } };
        return ct;
    }

} /// namespace eost

EOSIO_DISPATCH( eost::token, (create)(issue)(transfer)(open)(close)(retire)(cantransfer) )
