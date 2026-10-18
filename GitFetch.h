/////////////////////////////////////////////////////////////////////////////
// Name:        GitFetch.h
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
#ifndef GITFETCH_H
#define GITFETCH_H

#include "wx/string.h"

/**
 * @brief Result of fetching every remote in the repository that contains a directory.
 */
struct GitFetchResult
{
    bool ok = false;
    wxString error;
    wxString summary;
};

/**
 * @brief Fetch every configured remote.
 *
 * @a credentials may be empty. When it contains @c username:password, that pair is
 * offered to a remote that asks for a username and password. The password may contain
 * further colons.
 */
GitFetchResult FetchRepository(const wxString& directory, const wxString& credentials);

#endif // GITFETCH_H
