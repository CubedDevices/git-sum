/////////////////////////////////////////////////////////////////////////////
// Name:        GitDiff.h
// Purpose:     Diff a file against its version before a commit
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
#ifndef GITDIFF_H
#define GITDIFF_H

#include "wx/string.h"

#include <vector>

/**
 * @brief One displayed row of a parent-to-commit diff.
 *
 * Additions are lines on the new side. Deletions are lines that existed
 * only on the old side.
 */
struct GitDiffLine
{
    enum class Kind
    {
        Context,
        Addition,
        Deletion,
        Hunk
    };

    Kind kind = Kind::Context;
    int oldLine = -1;
    int newLine = -1;
    wxString text;
};

/**
 * @brief Text diff of one file.
 */
struct GitCommitDiff
{
    bool ok = false;
    bool binary = false;
    bool identical = false;
    bool truncated = false;
    wxString error;
    wxString path;
    std::vector<GitDiffLine> lines;
};

/// Diff @a oldPath in the first parent of @a fullId against @a newPath in that commit.
GitCommitDiff QueryCommitDiff(const wxString& directory,
                              const wxString& fullId,
                              const wxString& oldPath,
                              const wxString& newPath);

/// Diff @a oldPath at HEAD against the working-tree file @a newPath.
GitCommitDiff QueryHeadDiff(const wxString& directory,
                            const wxString& oldPath,
                            const wxString& newPath);

#endif // GITDIFF_H
