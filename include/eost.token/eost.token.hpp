/**
 *  @file
 *  @copyright defined in LICENSE.txt
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/time.hpp>

#include <string>

namespace eosiosystem {
   class system_contract;
}

using namespace eosio;

namespace eost {

   using std::string;

   class [[eosio::contract("eost.token")]] token : public contract {
      public:
         using contract::contract;

         [[eosio::action]]
         void create( name   issuer,
                      asset  maximum_supply,
                      bool transfer_locked);

         [[eosio::action]]
         void issue( name to, asset quantity, string memo);

         [[eosio::action]]
         void issuelock( name to, asset quantity, string memo,uint64_t lock_time );


         [[eosio::action]]
         void retire( asset quantity, string memo );

         [[eosio::action]]
         void transfer( name    from,
                        name    to,
                        asset   quantity,
                        string  memo );

         [[eosio::action]]
         void open( name owner, const symbol& symbol, name ram_payer );

         [[eosio::action]]
         void cantransfer(asset quantity, bool is_transfer);

         [[eosio::action]]
         void close( name owner, const symbol& symbol );

         [[eosio::action]]
         void burn(name from, asset quantity);

         static asset get_supply( name token_contract_account, symbol_code sym_code )
         {
            stats statstable( token_contract_account, sym_code.raw() );
            const auto& st = statstable.get( sym_code.raw() );
            return st.supply;
         }

         static asset get_balance( name token_contract_account, name owner, symbol_code sym_code )
         {
            accounts accountstable( token_contract_account, owner.value );
            const auto& ac = accountstable.get( sym_code.raw() );
            return ac.balance;
         }

      private:
         struct [[eosio::table]] account {
            asset    balance;

            uint64_t primary_key()const { return balance.symbol.code().raw(); }
         };

         struct [[eosio::table]] currency_stats {
             asset    supply;
             asset    max_supply;
             name     issuer;
             bool transfer_locked = false;
             uint64_t primary_key()const { return supply.symbol.code().raw(); }
         };

         struct [[eosio::table]] locker {
             asset balance;
             time_point unlock_time;
             time_point lock_time;
             uint64_t primary_key() const {return balance.symbol.code().raw();}
         };

         typedef eosio::multi_index< "accounts"_n, account > accounts;
         typedef eosio::multi_index< "stat"_n, currency_stats > stats;
         typedef eosio::multi_index<"locker"_n, locker > lockers;

         void sub_balance( name owner, asset value );
         void add_balance( name owner, asset value, name ram_payer );
         bool is_locked(name owner,asset   quantity);
         void lock(name to, asset quantity, uint64_t lock_time,name ram_payer);


       static time_point current_time_point();
   };

} /// namespace eosio
