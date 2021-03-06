/**
 *  @file
 *  @copyright defined in EOTS/LICENSE.txt
 */

#include <eotio.token/eotio.token.hpp>

namespace eotio {

void token::create( account_name issuer,
                    asset        maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eotio_assert( sym.is_valid(), "invalid symbol name" );
    eotio_assert( maximum_supply.is_valid(), "invalid supply");
    eotio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    eotio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::issue( account_name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eotio_assert( sym.is_valid(), "invalid symbol name" );
    eotio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    eotio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eotio_assert( quantity.is_valid(), "invalid quantity" );
    eotio_assert( quantity.amount > 0, "must issue positive quantity" );

    eotio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eotio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st, st.issuer );

    if( to != st.issuer ) {
       SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
    }
}

void token::transfer( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
    eotio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eotio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

    require_recipient( from );
    require_recipient( to );

    eotio_assert( quantity.is_valid(), "invalid quantity" );
    eotio_assert( quantity.amount > 0, "must transfer positive quantity" );
    eotio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eotio_assert( memo.size() <= 256, "memo has more than 256 bytes" );


    sub_balance( from, quantity, st );
    add_balance( to, quantity, st, from );
}

void token::sub_balance( account_name owner, asset value, const currency_stats& st ) {
   accounts from_acnts( _self, owner );

   const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );
   eotio_assert( from.balance.amount >= value.amount, "overdrawn balance" );


   if( from.balance.amount == value.amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance -= value;
      });
   }
}

void token::add_balance( account_name owner, asset value, const currency_stats& st, account_name ram_payer )
{
   accounts to_acnts( _self, owner );
   auto to = to_acnts.find( value.symbol.name() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, 0, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

} /// namespace eotio

eotIO_ABI( eotio::token, (create)(issue)(transfer) )
