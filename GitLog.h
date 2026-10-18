/////////////////////////////////////////////////////////////////////////////
// Name:        GitLog.h
// Purpose:     Repository history through libgit2
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
#ifndef GITLOG_H
#define GITLOG_H

#include "wx/string.h"

#include <vector>

/**
 * @brief One commit in the history, newest first.
 */
struct GitLogRow
{
    wxString id;
    wxString fullId;
    wxString author;
    wxString date;
    wxString summary;
    wxString message;
};

/**
 * @brief One file changed by a commit.
 */
struct GitCommitFile
{
    wxString status;
    wxString path;
};

/**
 * @brief Files changed by one commit, compared with its first parent.
 */
struct GitCommitFilesResult
{
    bool ok = false;
    wxString error;
    std::vector<GitCommitFile> files;
};

/**
 * @brief Result of reading history for the repository that contains a directory.
 *
 * At most 500 commits are returned. @c truncated is true when older commits exist.
 * @c scope is the file or directory relative to the work tree. It is empty when
 * @c directory is the work tree itself, and history is then the whole repository.
 * Otherwise only commits that touch that file or directory are included.
 */
struct GitLogResult
{
    bool ok = false;
    bool truncated = false;
    wxString error;
    wxString root;
    wxString branch;
    wxString scope;
    std::vector<GitLogRow> rows;
};

/// Read history for the repository that contains @a path, limited to that file or directory.
GitLogResult QueryGitLog(const wxString& path);

/// Files changed by the commit @a fullId in the repository that contains @a directory.
GitCommitFilesResult QueryCommitFiles(const wxString& directory, const wxString& fullId);

#endif // GITLOG_H
