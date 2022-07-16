/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WINFIREWALL_H_
#define WINFIREWALL_H_

#include "../client/w.h"
#include "../client/typedefs.h"

struct INetFwMgr;
struct INetFwPolicy;
struct INetFwProfile;

class WinFirewall
{
public:
	WinFirewall();
	~WinFirewall();

	WinFirewall(const WinFirewall&) = delete;
	WinFirewall& operator= (const WinFirewall&) = delete;

	bool initialize(HRESULT *result);
	void shutdown();

	bool enabled() const;
	bool queryAuthorized(const string& filename, bool& authorized) const;
	bool queryAuthorizedW(const wstring& filename, bool& authorized) const;

	bool addApplication(const string& filename, const string& friendlyName, bool authorized, HRESULT* result);
	bool addApplicationW(const wstring& filename, const wstring& friendlyName, bool authorized, HRESULT* result);

private:
	INetFwMgr* mgr;
	INetFwPolicy* policy;
	INetFwProfile* profile;
};

#endif // WINFIREWALL_H_
