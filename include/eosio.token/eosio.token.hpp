#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

#include <string>

namespace eosiosystem {
	class system_contract;
}

namespace eosio {

	using std::string;

	class [[eosio::contract("eosio.token")]] token : public contract {
		public:
			using contract::contract;

		[[eosio::action]]
		void create( const name& issuer, const asset& maximum_supply);
		
		[[eosio::action]]
		void issue( const name& to, const asset& quantity, const string& memo );

		[[eosio::action]]
		void retire( const asset& quantity, const string& memo );

		[[eosio::action]]
		void transfer( const name from, const name to, const asset quantity, const string memo );

		[[eosio::action]]
		void open( const name& owner, const symbol& symbol, const name& ram_payer );

		[[eosio::action]]
		void close( const name& owner, const symbol& symbol );
		
		[[eosio::action]]
		void burn( const name sender, const name from, const asset quantity, const string memo );
		
		[[eosio::action]]
		void blacklist( name account, bool a );
		
		[[eosio::action]]
		void stake( const name from, const asset quantity );
		
		[[eosio::action]]
		void unstake( const name from, const asset quantity );
		
		static asset get_supply( const name& token_contract_account, const symbol_code& sym_code )
		{
			stats statstable( token_contract_account, sym_code.raw() );
			const auto& st = statstable.get( sym_code.raw() );
			return st.supply;
		}

		static asset get_balance( const name& token_contract_account, const name& owner, const symbol_code& sym_code )
		{
			accounts accountstable( token_contract_account, owner.value );
			const auto& ac = accountstable.get( sym_code.raw() );
			return ac.balance;
		}

		using create_action = eosio::action_wrapper<"create"_n, &token::create>;
		using issue_action = eosio::action_wrapper<"issue"_n, &token::issue>;
		using retire_action = eosio::action_wrapper<"retire"_n, &token::retire>;
		using transfer_action = eosio::action_wrapper<"transfer"_n, &token::transfer>;
		using open_action = eosio::action_wrapper<"open"_n, &token::open>;
		using close_action = eosio::action_wrapper<"close"_n, &token::close>;
		using burn_action = eosio::action_wrapper<"burn"_n, &token::burn>;
		using stake_action = eosio::action_wrapper<"stake"_n, &token::stake>;
		using unstake_action = eosio::action_wrapper<"unstake"_n, &token::unstake>;
		
		private:
		
		struct [[eosio::table]] account {
			asset	balance;
			uint64_t primary_key()const { return balance.symbol.code().raw(); }
		};

		struct [[eosio::table]] currency_stats {
			asset	supply;
			asset	max_supply;
			name	issuer;
			uint64_t primary_key()const { return supply.symbol.code().raw(); }
		};
		
		struct [[eosio::table]] currency_stake {
			name account;
			asset staked;
			uint64_t primary_key()const { return account.value; }
		};
		
		struct [[eosio::table]] blacklists {
			name account;
			uint64_t primary_key() const { return account.value; }
		};

		typedef eosio::multi_index< "accounts"_n, account > accounts;
		typedef eosio::multi_index< "stat"_n, currency_stats > stats;
		typedef eosio::multi_index< "stake"_n, currency_stake > stakeds;
		typedef eosio::multi_index< "blacklist"_n, blacklists > db_blacklist;

		void sub_balance( const name owner, const asset value );
		void add_balance( const name owner, const asset value, const name ram_payer );
		void getblacklist( name account );
		void get_staked_balance( const name from, asset quantity );
		void check_quantity( asset quantity );
	};
}