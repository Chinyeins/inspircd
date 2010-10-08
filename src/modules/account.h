/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __ACCOUNT_H__
#define __ACCOUNT_H__

class AccountEvent : public Event
{
 public:
	User* const user;
	const irc::string account;
	AccountEvent(Module* me, User* u, const irc::string& name)
		: Event(me, "account_login"), user(u), account(name)
	{
	}
};

/** AccountProvider: use dynamic_reference<AccountProvider> acct("account") to access. */
class AccountProvider : public DataProvider
{
 public:
	AccountProvider(Module* mod, const std::string& Name) : DataProvider(mod, Name) {}
	/** Is the user registered? */
	virtual bool IsRegistered(User* user) = 0;
	/**
	 * Get the account name that a user is using
	 * @param user The user to query
	 * @return The account name, or "" if not logged in
	 */
	virtual irc::string GetAccountName(User* user) = 0;
	/**
	 * Log the user in to an account.
	 *
	 * @param user The user to log in
	 * @param name The account name to log them in with. Empty to log out.
	 * @param tag A hidden tag on the account, for recording freshness or login method
	 */
	virtual void DoLogin(User* user, const irc::string& name, const std::string& tag = "") = 0;
};

class AccountDBEntry : public Extensible
{
 public:
	const irc::string name;
	const time_t ts;
	time_t hash_password_ts, connectclass_ts;
	std::string hash, password, connectclass;
	AccountDBEntry(const irc::string& nameref, time_t ourTS, std::string h = "", std::string p = "", time_t h_p_ts = 0, std::string cc = "", time_t cc_ts = 0) : Extensible(EXTENSIBLE_ACCOUNT), name(nameref), ts(ourTS), hash_password_ts(h_p_ts), connectclass_ts(cc_ts), hash(h), password(p), connectclass(cc)
	{
	}
	virtual CullResult cull() = 0;
	virtual ~AccountDBEntry() {}
};

typedef std::map<irc::string, AccountDBEntry*> AccountDB;

class AccountDBModifiedEvent : public Event
{
 public:
	const irc::string name;
	const AccountDBEntry* const entry;

	/**
	 * Create an event indicating that an account in the database has been modified
	 * The AccountDBProvider Send functions will automatically send this event
	 * @param me A pointer to the module creating the event
	 * @param acctname The name of the account that was modified
	 * @param ent A pointer to the account that was modified, or NULL if the account was deleted
	 */
	AccountDBModifiedEvent(Module* me, const irc::string& acctname, const AccountDBEntry* ent)
		: Event(me, "accountdb_modified"), name(acctname), entry(ent)
	{
	}
};

class GetAccountByAliasEvent : public Event
{
 public:
	const irc::string account;
	AccountDBEntry* entry;
	time_t alias_ts;
	void (*RemoveAliasImpl)(const irc::string&);
	GetAccountByAliasEvent(Module* me, const irc::string& name)
		: Event(me, "get_account_by_alias"), account(name), entry(NULL), alias_ts(0), RemoveAliasImpl(NULL)
	{
		Send();
	}
	inline void RemoveAlias()
	{
		RemoveAliasImpl(account);
	}
};

class AccountDBProvider : public DataProvider
{
 public:
	AccountDBProvider(Module* mod) : DataProvider(mod, "accountdb") {}

	/**
	 * Create an account and add it to the database
	 * @param send Whether or not to send the account immediately after adding it, if adding was successful
	 * @param nameref The name of the account to add
	 * @param ourTS The creation TS of the account to add
	 * @param h The hash type of the account to add
	 * @param p The password of the account to add
	 * @param h_p_ts The hash/password TS of the account to add
	 * @param cc The connect class of the account to add
	 * @param cc_ts The TS of the connect class of the account to add
	 * @return A pointer to the new account if it was successfully added, NULL if an account with the same name already existed
	 */
	virtual AccountDBEntry* AddAccount(bool send, const irc::string& nameref, time_t ourTS, std::string h = "", std::string p = "", time_t h_p_ts = 0, std::string cc = "", time_t cc_ts = 0) = 0;

	/**
	 * Get an account from the database
	 * @param name The name of the account
	 * @param alias Whether or not to check the given name as an alias
	 * @return A pointer to the account, or NULL if no account by the given name exists
	 */
	virtual AccountDBEntry* GetAccount(irc::string name, bool alias) const = 0;

	/**
	 * Remove an account from the database and delete it
	 * This frees the memory associated with the account and invalidates any pointers to it
	 * @param send Whether or not to send the removal immediately after removing it
	 * @param entry A pointer to the account to remove
	 */
	virtual void RemoveAccount(bool send, AccountDBEntry* entry) = 0;

	/**
	 * Get the internal database used to store accounts
	 * @return A const reference to the database
	 */
	virtual const AccountDB& GetDB() const = 0;

	/**
	 * Send an entire account
	 * @param entry A pointer to the account to send
	 */
	virtual void SendAccount(const AccountDBEntry* entry) const = 0;

	/** Send an update to an account
	 * @param entry A pointer to the account to send an update for
	 * @param field The name of the field to send an update for
	 */
	virtual void SendUpdate(const AccountDBEntry* entry, std::string field) const = 0;

	/** Send a removal for an account
	 * @param name The name of the account to remove
	 * @param ts The creation TS of the account to remove
	 */
	virtual void SendRemoval(irc::string name, time_t ts) const = 0;
};

// Some generic extension items for use with accounts

class TSExtItem : public SimpleExtItem<time_t>
{
 public:
	TSExtItem(const std::string& Key, Module* parent) : SimpleExtItem<time_t>(EXTENSIBLE_ACCOUNT, Key, parent) {}
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
	{
		time_t* ts = static_cast<time_t*>(item);
		if(!ts) /* If we don't have a TS, not if the TS is zero */
			return "";
		return ConvToStr(*ts);
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		time_t* ours = get(container);
		time_t theirs = atol(value.c_str());
		if(!ours || theirs > *ours)
			set(container, theirs);
	}
};

class TSBoolExtItem : public SimpleExtItem<std::pair<time_t, bool> >
{
 public:
	TSBoolExtItem(const std::string& Key, Module* parent) : SimpleExtItem<std::pair<time_t, bool> >(EXTENSIBLE_ACCOUNT, Key, parent) {}
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
	{
		std::pair<time_t, bool>* p = static_cast<std::pair<time_t, bool>*>(item);
		if(!p)
			return "";
		return ConvToStr(p->first) + (format == FORMAT_NETWORK ? " :" : " ") + (p->second ? '1' : '0');
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		time_t ts;
		bool item;
		std::string::size_type delim = value.find_first_of(' ');
		ts = atol(value.substr(0, delim).c_str());
		if(delim == std::string::npos)
			item = false;
		else
			item = (value.substr(delim + 1)[0] == '1');
		std::pair<time_t, bool>* p = get(container);
		if(!p || ts > p->first)
			set(container, std::make_pair(ts, item));
	}
};

class TSIntExtItem : public SimpleExtItem<std::pair<time_t, signed int> >
{
	const signed int default_value;
 public:
	TSIntExtItem(const std::string& Key, Module* parent, signed int def_value) : SimpleExtItem<std::pair<time_t, signed int> >(EXTENSIBLE_ACCOUNT, Key, parent), default_value(def_value) {}
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
	{
		std::pair<time_t, signed int>* p = static_cast<std::pair<time_t, signed int>*>(item);
		if(!p)
			return "";
		return ConvToStr(p->first) + (format == FORMAT_NETWORK ? " :" : " ") + ConvToStr(p->second);
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		time_t ts;
		signed int item;
		std::string::size_type delim = value.find_first_of(' ');
		ts = atol(value.substr(0, delim).c_str());
		if(delim == std::string::npos)
			item = default_value;
		else
			item = atoi(value.substr(delim + 1).c_str());
		std::pair<time_t, signed int>* p = get(container);
		if(!p || ts > p->first)
			set(container, std::make_pair(ts, item));
	}
};

class TSStringExtItem : public SimpleExtItem<std::pair<time_t, std::string> >
{
 public:
	TSStringExtItem(const std::string& Key, Module* parent) : SimpleExtItem<std::pair<time_t, std::string> >(EXTENSIBLE_ACCOUNT, Key, parent) {}
	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
	{
		std::pair<time_t, std::string>* p = static_cast<std::pair<time_t, std::string>*>(item);
		if(!p)
			return "";
		return ConvToStr(p->first) + (format == FORMAT_NETWORK ? " :" : " ") + p->second;
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		time_t ts;
		std::string item;
		std::string::size_type delim = value.find_first_of(' ');
		ts = atol(value.substr(0, delim).c_str());
		if(delim == std::string::npos)
			item = "";
		else
			item = value.substr(delim + 1);
		std::pair<time_t, std::string>* p = get(container);
		if(!p || ts > p->first)
			set(container, std::make_pair(ts, item));
	}
};

#endif
