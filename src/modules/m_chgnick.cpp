/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2006 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

/** Handle /CHGNICK
 * */
class CommandChgNick : public Command
{
 public:
	CommandChgNick(Module* Creator) : Command(Creator,"CHGNICK", 2, 2)
	{
		allow_empty_last_param = false;
		flags_needed = 'o';
		syntax = "<oldnick> <newnick>";
		TRANSLATE2(TR_NICK, TR_TEXT);
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[0]);

		if ((!target) || (target->registered != REG_ALL))
		{
			user->WriteNotice("*** No such nickname: '" + parameters[0] + "'");
			return CMD_FAILURE;
		}

		/* Do local sanity checks and bails */
		if (IS_LOCAL(user))
		{
			if (!ServerInstance->IsNick(parameters[1]))
			{
				user->WriteNotice("*** Invalid nickname '" + parameters[1] + "'");
				return CMD_FAILURE;
			}
			if (parameters[1].empty())
			{
				user->WriteNotice("*** CHGNICK: nick must be specified");
				return CMD_FAILURE;
			}

			user->WriteNumeric(947, "%s :Nickname now changed.", parameters[1].c_str());
		}

		/* If we made it this far, extend the user */
		if (IS_LOCAL(target))
		{
			std::string oldnick = target->nick;
			if (target->ForceNickChange(parameters[1]))
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used CHGNICK to change "+oldnick+" to "+parameters[1]);
			else
			{
				std::string newnick = target->nick;
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used CHGNICK, but "+oldnick+" failed nick change to "+parameters[1]+" and was changed to "+newnick+" instead");
			}
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
	}
};

class ModuleChgNick : public Module
{
	CommandChgNick cmd;

public:
	ModuleChgNick() : cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for the CHGNAME command", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleChgNick)
