/////////////////////////////////////////////////////////////////////////////
// Name:        GitStatus.h
// Purpose:     Repository status through libgit2
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
#ifndef GITSTATUS_H
#define GITSTATUS_H

#include "wx/string.h"

#include <vector>

/**
 * @brief One path reported by git status.
 *
 * Index describes the staged change. WorkTree describes the unstaged change.
 * Either string may be empty.
 */
struct GitStatusRow
{
    wxString path;
    wxString index;
    wxString workTree;
};

/**
 * @brief Result of reading status for the repository that contains a directory.
 *
 * @c scope is the directory relative to the work tree. It is empty when @c directory
 * is the work tree itself.
 */
struct GitStatusResult
{
    bool ok = false;
    wxString error;
    wxString root;
    wxString branch;
    wxString scope;
    std::vector<GitStatusRow> rows;
};

/// Read status for the repository that contains @a directory, limited to that directory.
GitStatusResult QueryGitStatus(const wxString& directory);

#endif // GITSTATUS_H
