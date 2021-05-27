#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/action.hpp>
#include <eosio.token/eosio.token.hpp>

#define BASIC_SYMBOL symbol("CFF", 4)

namespace eosio {

void token::create( const name& issuer, const asset&  maximum_supply ){
    require_auth( get_self() );

    auto sym = maximum_supply.symbol;
    check( sym.is_valid(), "Invalid symbol name" );
    check( maximum_supply.is_valid(), "Invalid supply");
    check( maximum_supply.amount > 0, "Max-Supply must be more then 0");

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing == statstable.end(), "This token already exist" );

    statstable.emplace( get_self(), [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}

void token::issue( const name& to, const asset& quantity, const string& memo ){
	getblacklist( to );
    auto sym = quantity.symbol;
    check( sym.is_valid(), "Invalid symbol name" );
    check( memo.size() <= 256, "Memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "This token dont exist." );
    const auto& st = *existing;
    check( to == st.issuer, "Token can only be issued TO issuer account" );
	require_recipient( to );
    require_auth( st.issuer );
    check( quantity.is_valid(), "Invalid quantity" );
    check( quantity.amount > 0, "Amount should be more then 0" );

    check( quantity.symbol == st.supply.symbol, "Symbol precision mismatch" );
    check( quantity.amount <= st.max_supply.amount - st.supply.amount, "Quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });
    add_balance( st.issuer, quantity, st.issuer );
}

void token::retire( const asset& quantity, const string& memo ){
    auto sym = quantity.symbol;
    check( sym.is_valid(), "Invalid symbol name" );
    check( memo.size() <= 256, "Memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "Token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "Invalid quantity" );
    check( quantity.amount > 0, "Amount should be more then 0" );

    check( quantity.symbol == st.supply.symbol, "Symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });
    sub_balance( st.issuer, quantity );
}

void token::transfer( name from, name to, asset quantity, string memo ){
    check( from != to, "Cannot transfer to self" );
    require_auth( from );
	getblacklist( from );
	getblacklist( to );
    check( is_account( to ), "TO ["+to.to_string()+"] account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

	check_quantity( quantity );
    check( quantity.symbol == st.supply.symbol, "Symbol precision mismatch" );
    check( memo.size() <= 256, "Memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : from;
	
	asset COMISS;
	COMISS.symbol = BASIC_SYMBOL;
	COMISS.amount = 10000;
	sub_balance( from, COMISS );
	statstable.modify( st, get_self(), [&]( auto& s ) {
	   s.supply.amount -= COMISS.amount;
	   s.max_supply.amount -= COMISS.amount;
	});
	
	get_staked_balance( from, quantity );
	
	sub_balance( from, quantity );
	add_balance( to, quantity, payer );
}


void token::burn( name sender, name from, asset quantity, string memo ){
    check( BASIC_SYMBOL == quantity.symbol, "Only CFF token allowed to burn on eosio.token. Burned on ["+from.to_string()+"], FROM ["+sender.to_string()+"]" );
	if( from != "swap.eos"_n && from != "swap.cht"_n && from != "swap.dice"_n && from != "swap.pgl"_n ){
		 check( false, "This from not register in trusted dapps!" );
	}
    require_auth( from );
    auto sym = quantity.symbol.code();
    stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );
	
    require_recipient( from );
    require_recipient( sender );

    check( quantity.is_valid(), "Invalid quantity" );
    check( quantity.amount > 0, "Amount less then 0 ["+std::to_string( quantity.amount )+"]" );
    check( quantity.symbol == st.supply.symbol, "Symbol precision mismatch" );
    check( memo.size() <= 256, "Memo has more than 256 bytes" );

	if( (st.max_supply.amount - quantity.amount ) < 0 ){
		check( false, "All CFF tokens burned!!!" );
	}
	statstable.modify( st, get_self(), [&]( auto& s ) {
	   s.supply.amount -= quantity.amount;
	   s.max_supply.amount -= quantity.amount;
	});
	sub_balance( sender, quantity );
}

void token::sub_balance( const name owner, const asset value ){
	accounts from_acnts( get_self(), owner.value );
	const auto& from = from_acnts.find( value.symbol.code().raw() );  
	if( from == from_acnts.end() ) {
		check( false, "FROM ["+owner.to_string()+"] dont have ["+value.symbol.code().to_string()+"] tokens" );
	}else{
		check( from->balance.amount >= value.amount, "Overdraw balance on token ["+value.symbol.code().to_string()+"] on ["+owner.to_string()+"]" );
		from_acnts.modify( from, owner, [&]( auto& a ) {
			a.balance -= value;
		});
	}
}

void token::add_balance( const name owner, const asset value, const name ram_payer ){
   accounts to_acnts( get_self(), owner.value );
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

void token::get_staked_balance( const name from, asset quantity ){
	accounts from_acnts( get_self(), from.value );
	auto balance = from_acnts.find( quantity.symbol.code().raw() );
	if( balance != from_acnts.end() ){
		stakeds staked( get_self(), get_self().value );
		auto stake = staked.find( from.value );
		if( stake != staked.end() ) {
			if( balance->balance.amount < ( quantity.amount + stake->staked.amount ) ){
				check( false, "FROM ["+from.to_string()+"] account have staked amount ["+std::to_string( stake->staked.amount )+"]. Balance is less then possible transfer." );
			}
		}
	}
}

void token::open( const name& owner, const symbol& symbol, const name& ram_payer ){
	require_auth( ram_payer );
	getblacklist( owner );
	getblacklist( ram_payer );

	check( is_account( owner ), "owner ["+owner.to_string()+"] account does not exist" );

	auto sym_code_raw = symbol.code().raw();
	stats statstable( get_self(), sym_code_raw );
	const auto& st = statstable.get( sym_code_raw, "Symbol does not exist" );
	check( st.supply.symbol == symbol, "Symbol precision mismatch" );

	accounts acnts( get_self(), owner.value );
	auto it = acnts.find( sym_code_raw );
	if( it == acnts.end() ) {
		acnts.emplace( ram_payer, [&]( auto& a ){
			a.balance = asset{0, symbol};
		});
	}
}

void token::close( const name& owner, const symbol& symbol ){
	require_auth( owner );
	getblacklist( owner );
	accounts acnts( get_self(), owner.value );
	auto it = acnts.find( symbol.code().raw() );
	check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
	check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
	acnts.erase( it );
}

void token::stake( const name from, const asset quantity ){
	require_auth( "coffe.hold"_n );
	check_quantity( quantity );
	accounts from_acnts( get_self(), from.value );
	auto balance = from_acnts.find( quantity.symbol.code().raw() );
	if( balance != from_acnts.end() ){
		if( balance->balance.amount < quantity.amount ){
			check( false, "FROM ["+from.to_string()+"] balance ["+std::to_string( balance->balance.amount )+"] is less then want stake ["+std::to_string( quantity.amount )+"]" );
		}
	}else{
		check( false, "FROM ["+from.to_string()+"] account dont have amount for stake" );
	}
	stakeds staked( get_self(), get_self().value );
	auto to = staked.find( from.value );
	if( to == staked.end() ) {
		staked.emplace( get_self(), [&]( auto& t ){
			t.account = from;
			t.staked = quantity;
		});
	}else{
		if( ( balance->balance.amount - to->staked.amount ) < quantity.amount ){
			check( false, "FROM ["+from.to_string()+"] account have staked amount" );
		}
		staked.modify( to, get_self(), [&]( auto& t ) {
			t.staked += quantity;
		});
	}
}

void token::unstake( const name from, const asset quantity )
{
	require_auth( "coffe.hold"_n );
	check_quantity( quantity );
	stakeds staked( get_self(), get_self().value );
	auto stake = staked.find( from.value );
	if( stake == staked.end() ){
		check( false, "FROM ["+from.to_string()+"] account does have stake" );
	}else{
		if( stake->staked.amount == quantity.amount ){
			staked.erase( stake );
		}else if( stake->staked.amount < quantity.amount ){
			check( false, "FROM ["+from.to_string()+"] account have less staked amount ["+std::to_string( stake->staked.amount )+"] then want unstake ["+std::to_string( quantity.amount )+"]" );
		}else{
			staked.modify( stake, get_self(), [&]( auto& a ) {
				a.staked -= quantity;
			});
		}
	}
}

void token::getblacklist( name account ){
	db_blacklist blacklist(get_self(), get_self().value);
	auto black = blacklist.find( account.value );
	if(black != blacklist.end()){
		check(false, "Account is on BLACKLIST");
	}
}
	
void token::blacklist( name account, bool a ){
	require_auth(get_self());
	require_recipient( account );
	if( account == get_self()){
		check(false, "SELF not should be added");
	}
	db_blacklist blacklist(get_self(), get_self().value);
	auto black = blacklist.find( account.value );
	if(black == blacklist.end()){
		if(!a){ check(false, "Account dont exist"); }
		blacklist.emplace(get_self(), [&](auto& t){
			t.account = account;
		});
	}else{
		if(a){
			check(false, "Account already exist");
		}else{
			blacklist.erase(black);
		}
	}
}

void token::check_quantity( asset quantity ){
	auto sym = quantity.symbol;
	check( quantity.is_valid(), "Invalid quantity" );
	check( sym.is_valid(), "Invalid symbol name" );
	check( quantity.amount > 0, "Quantity must be positive");
}
} /// namespace eosio
