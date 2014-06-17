/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2004, 2008-2009 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007 Robin Burchell <robin+git@viroteck.net>
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

/* $LinkerFlags: -lPocoNet -lPocoFoundation -lPocoNetSSL -lpugixml */
/* $ModDesc: Provides user and channel +Z mode */

#include "inspircd.h"
#include <iostream>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPBasicCredentials.h>
#include <Poco/URI.h>
#include <pugixml.hpp>

using Poco::Net::HTTPSClientSession;
using Poco::Net::HTTPRequest;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPMessage;
using Poco::URI;

/** Handles usermode +Z
 */
class TranslateUser : public SimpleUserModeHandler
{
 public:
	TranslateUser(Module* Creator) : SimpleUserModeHandler(Creator, "u_translate", 'Z') { }
};

/** Handles channel mode +Z
 */
class TranslateChannel : public SimpleChannelModeHandler
{
 public:
	TranslateChannel(Module* Creator) : SimpleChannelModeHandler(Creator, "translate", 'Z') { }
};

class ModuleTranslate : public Module
{
	TranslateUser tu;
	TranslateChannel tc;

 public:
	ModuleTranslate() : tu(this), tc(this) { }
	void init()
	{
	ServerInstance->Modules->AddService(tu);
	ServerInstance->Modules->AddService(tc);
	Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice };
	ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ~ModuleTranslate()
	{
	}

	std::string char2hex( char dec )
	{
		char dig1 = (dec&0xF0)>>4;
		char dig2 = (dec&0x0F);
		if ( 0<= dig1 && dig1<= 9) dig1+=48;	// 0,48inascii
		if (10<= dig1 && dig1<=15) dig1+=97-10;	// a,97inascii
		if ( 0<= dig2 && dig2<= 9) dig2+=48;
		if (10<= dig2 && dig2<=15) dig2+=97-10;

		std::string r;
		r.append( &dig1, 1);
		r.append( &dig2, 1);
		return r;
	}
	// 
	std::string encode(const std::string &msg)
	{
		std::string escapedmsg="";
		int max = msg.length();
		for(int i=0; i<max; i++)
		{
			if (	(48 <= msg[i] && msg[i] <= 57) ||	// 0-9
				(65 <= msg[i] && msg[i] <= 90) ||	// a-z
				(97 <= msg[i] && msg[i] <= 122) ||	// A-Z
				(msg[i]=='~' || msg[i]=='!' ||
				msg[i]=='*' || msg[i]=='(' ||
				msg[i]==')' || msg[i]=='\''))
				{ escapedmsg.append( &msg[i], 1); }
			else
			{	
			escapedmsg.append("%");
			escapedmsg.append( char2hex(msg[i]) );	// converts char 255 to string "ff"
			}
		}
		return escapedmsg;
	}

	std::string translate(const std::string& msg) {
		std::string escaped = encode(msg);
		URI uri("https://api.datamarket.azure.com");
		std::string path = "/Bing/MicrosoftTranslator/v1/Translate?Text='";
		// language to translate to (en, fr, es, etc)
		std::string to = "'&To='en'";
		std::string finalpath = path + escaped + to;
		std::string username = "foo";
		// password is your azure datamarket api key you get after registering at datamarket.azure.com and ordering the translation service
		std::string password = "api string";
		std::string translation;

		Poco::Net::Context::Ptr context = new Poco::Net::Context(Poco::Net::Context::CLIENT_USE, "", "", "", Poco::Net::Context::VERIFY_NONE, 9, false, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
		HTTPSClientSession session(uri.getHost(), uri.getPort(), context);
		Poco::Net::HTTPRequest request(HTTPRequest::HTTP_GET, finalpath, HTTPMessage::HTTP_1_1);
		Poco::Net::HTTPBasicCredentials credentials(username, password);
		credentials.authenticate(request);
		session.sendRequest(request);
		HTTPResponse response;
		std::istream& rs = session.receiveResponse(response);
		
		pugi::xml_document doc;
		doc.load(rs);
		translation = doc.child("feed").child("entry").child("content").child("m:properties").child_value("d:Text");
		return translation;
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		bool active = false;

		if (target_type == TYPE_USER)
			active = ((User*)dest)->IsModeSet('Z');
		else if (target_type == TYPE_CHANNEL)
		{
			active = ((Channel*)dest)->IsModeSet('Z');
			Channel* c = (Channel*)dest;
			ModResult res = ServerInstance->OnCheckExemption(user,c,"translate");

			if (res == MOD_RES_ALLOW)
				return MOD_RES_PASSTHRU;
		}

		if (!active)
			return MOD_RES_PASSTHRU;

		text = text + " (" + translate(text) + ")";
		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user,dest,target_type,text,status,exempt_list);
	}

	virtual Version GetVersion()
	{
		return Version("Provides user and channel +Z mode",VF_VENDOR);
	}

};

MODULE_INIT(ModuleTranslate)
