/////////////////////////////////////////////////////////////////////////////
// Name:        GitFetch.cpp
// Purpose:     Download from repository remotes through libgit2
// Copyright:   Kenneth S. Davis, 2026
// Licence:     GNU General Public License version 2
/////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Kenneth S. Davis
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
/////////////////////////////////////////////////////////////////////////////
#include "GitFetch.h"

#include "git2.h"

#include "wx/arrstr.h"

#include <string>

namespace
{

struct CredentialPayload
{
    std::string user;
    std::string password;
    int attempts = 0;
};

wxString FromUtf8(const char* text)
{
    if ( !text )
        return wxString();
    return wxString::FromUTF8(text);
}

wxString GitLastError(const wxString& fallback)
{
    const git_error* error = git_error_last();
    if ( error && error->message && error->message[0] )
        return FromUtf8(error->message);
    return fallback;
}

CredentialPayload ParseCredentials(const wxString& stored)
{
    CredentialPayload payload;
    const int colon = stored.Find(':');
    if ( colon == wxNOT_FOUND || colon <= 0 )
        return payload;
    const wxScopedCharBuffer user = stored.Left(static_cast<size_t>(colon)).utf8_str();
    const wxScopedCharBuffer password = stored.Mid(static_cast<size_t>(colon) + 1).utf8_str();
    if ( user.data() )
        payload.user = user.data();
    if ( password.data() )
        payload.password = password.data();
    return payload;
}

int AcquireCredentials(git_credential** out, const char* /*url*/, const char* usernameFromUrl,
                       unsigned int allowedTypes, void* payload)
{
    CredentialPayload* credentials = static_cast<CredentialPayload*>(payload);
    if ( !credentials || credentials->attempts > 0 || credentials->user.empty() )
        return GIT_PASSTHROUGH;
    if ( (allowedTypes & GIT_CREDENTIAL_USERPASS_PLAINTEXT) == 0 )
        return GIT_PASSTHROUGH;

    const char* user = (usernameFromUrl && usernameFromUrl[0]) ? usernameFromUrl : credentials->user.c_str();
    ++credentials->attempts;
    return git_credential_userpass_plaintext_new(out, user, credentials->password.c_str());
}

} // namespace

GitFetchResult FetchRepository(const wxString& directory, const wxString& credentials)
{
    GitFetchResult result;
    git_libgit2_init();

    const wxScopedCharBuffer utf8 = directory.utf8_str();
    git_buf discovered = GIT_BUF_INIT;
    if ( git_repository_discover(&discovered, utf8.data(), 0, nullptr) != 0 )
    {
        result.error = "Not a Git repository.";
        git_buf_dispose(&discovered);
        return result;
    }

    git_repository* repo = nullptr;
    const int opened = git_repository_open(&repo, discovered.ptr);
    git_buf_dispose(&discovered);
    if ( opened != 0 || !repo )
    {
        result.error = GitLastError("Could not open the repository.");
        return result;
    }

    git_strarray names = { nullptr, 0 };
    if ( git_remote_list(&names, repo) != 0 )
    {
        result.error = GitLastError("Could not list remotes.");
        git_repository_free(repo);
        return result;
    }
    if ( names.count == 0 )
    {
        result.error = "This repository has no remotes.";
        git_strarray_dispose(&names);
        git_repository_free(repo);
        return result;
    }

    CredentialPayload payload = ParseCredentials(credentials);
    git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
    options.callbacks.credentials = AcquireCredentials;
    options.callbacks.payload = &payload;

    wxArrayString fetched;
    for ( size_t i = 0; i < names.count; ++i )
    {
        const wxString name = FromUtf8(names.strings[i]);
        git_remote* remote = nullptr;
        if ( git_remote_lookup(&remote, repo, names.strings[i]) != 0 || !remote )
        {
            result.error = "Could not open remote " + name + ".\n" + GitLastError("Could not open the remote.");
            break;
        }

        payload.attempts = 0;
        const int fetchedRemote = git_remote_fetch(remote, nullptr, &options, "fetch");
        git_remote_free(remote);
        if ( fetchedRemote != 0 )
        {
            result.error = "Could not fetch " + name + ".\n" + GitLastError("Could not fetch from the remote.");
            break;
        }
        fetched.Add(name);
    }

    git_strarray_dispose(&names);
    git_repository_free(repo);
    if ( !result.error.empty() )
        return result;

    result.summary = "Fetched " + fetched[0];
    for ( size_t i = 1; i < fetched.GetCount(); ++i )
        result.summary += ", " + fetched[i];
    result.summary += ".";
    result.ok = true;
    return result;
}
