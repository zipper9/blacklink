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

#include "stdafx.h"
#include "WinFirewall.h"
#include "../client/Text.h"

#include <comdef.h>
#include <netfw.h>

WinFirewall::WinFirewall() : mgr(nullptr), policy(nullptr), profile(nullptr)
{
}

WinFirewall::~WinFirewall()
{
	shutdown();
}

bool WinFirewall::initialize(HRESULT* result)
{
	if (mgr)
	{
		if (result) *result = S_OK;
		return true;
	}

	HRESULT hr = CoCreateInstance(__uuidof(NetFwMgr), 0, CLSCTX_INPROC_SERVER, __uuidof(INetFwMgr), reinterpret_cast<void**>(&mgr));
	if (SUCCEEDED(hr) && mgr) hr = mgr->get_LocalPolicy(&policy);
	if (SUCCEEDED(hr) && policy) hr = policy->get_CurrentProfile(&profile);

	if (result) *result = hr;
	return SUCCEEDED(hr) && profile;
}

void WinFirewall::shutdown()
{
	if (profile)
	{
		profile->Release();
		profile = nullptr;
	}
	if (policy)
	{
		policy->Release();
		policy = nullptr;
	}
	if (mgr)
	{
		mgr->Release();
		mgr = nullptr;
	}
}

bool WinFirewall::enabled() const
{
	if (!profile) return false;
	VARIANT_BOOL fwEnabled = VARIANT_FALSE;
	profile->get_FirewallEnabled(&fwEnabled);
	return (fwEnabled != VARIANT_FALSE);
}

bool WinFirewall::queryAuthorized(const string& filename, bool& authorized) const
{
	return queryAuthorizedW(Text::utf8ToWide(filename), authorized);
}

bool WinFirewall::queryAuthorizedW(const wstring& filename, bool& authorized) const
{
	authorized = false;
	bool success = false;

	if (!profile) return false;

	_bstr_t bfilename = filename.c_str();

	INetFwAuthorizedApplications* apps = nullptr;
	HRESULT hr = profile->get_AuthorizedApplications(&apps);
	if (SUCCEEDED(hr) && apps)
	{
		INetFwAuthorizedApplication* app = nullptr;
		hr = apps->Item(bfilename, &app);
		if (SUCCEEDED(hr) && app)
		{
			VARIANT_BOOL fwEnabled = VARIANT_FALSE;
			hr = app->get_Enabled(&fwEnabled);
			app->Release();

			if (SUCCEEDED(hr))
			{
				success = true;
				authorized = fwEnabled != VARIANT_FALSE;
			}
		}
		else if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			// No entry in list of authorized apps
			success = true;
		}
		apps->Release();
	}

	return success;
}

bool WinFirewall::addApplication(const string& filename, const string& friendlyName, bool authorized, HRESULT* result)
{
	return addApplicationW(Text::utf8ToWide(filename), Text::utf8ToWide(friendlyName), authorized, result);
}

bool WinFirewall::addApplicationW(const wstring& filename, const wstring& friendlyName, bool authorized, HRESULT* result)
{
	if (!profile) return false;
	INetFwAuthorizedApplications* apps = nullptr;
	HRESULT hr = profile->get_AuthorizedApplications(&apps);
	if (SUCCEEDED(hr) && apps)
	{
		INetFwAuthorizedApplication* app = nullptr;
		hr = CoCreateInstance(__uuidof(NetFwAuthorizedApplication), 0, CLSCTX_INPROC_SERVER,
			__uuidof(INetFwAuthorizedApplication), reinterpret_cast<void**>(&app));
		if (SUCCEEDED(hr) && app)
		{
			_bstr_t bstr = filename.c_str();
			hr = app->put_ProcessImageFileName(bstr);
			bstr = friendlyName.c_str();
			if (SUCCEEDED(hr)) hr = app->put_Name(bstr);
			if (SUCCEEDED(hr)) hr = app->put_Enabled(authorized ? VARIANT_TRUE : VARIANT_FALSE);
			if (SUCCEEDED(hr)) hr = apps->Add(app);
			app->Release();
		}
		apps->Release();
	}
	if (result) *result = hr;
	return SUCCEEDED(hr);
}
