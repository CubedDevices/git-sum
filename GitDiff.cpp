/////////////////////////////////////////////////////////////////////////////
// Name:        GitDiff.cpp
// Purpose:     Diff a committed file against the working tree
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
#include "GitDiff.h"

#include "git2.h"

#include "wx/ffile.h"
#include "wx/filename.h"
#include "wx/log.h"

#include <vector>

namespace
{

const size_t kMaxBytes = 2 * 1024 * 1024;
const size_t kMaxLines = 4000;

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

wxString LineText(const char* content, size_t length)
{
    wxString text;
    if ( content && length > 0 )
    {
        text = wxString::FromUTF8(content, length);
        if ( text.empty() )
            text = wxString::From8BitData(content, length);
    }
    while ( !text.empty() && (text.Last() == '\n' || text.Last() == '\r') )
        text.RemoveLast();
    text.Replace("\t", "    ");
    return text;
}

struct Collect
{
    std::vector<GitDiffLine>* lines = nullptr;
    bool* binary = nullptr;
    bool* truncated = nullptr;
};

int FileCallback(const git_diff_delta* delta, float, void* payload)
{
    Collect* collect = static_cast<Collect*>(payload);
    if ( delta && (delta->flags & GIT_DIFF_FLAG_BINARY) && collect->binary )
        *collect->binary = true;
    return 0;
}

int HunkCallback(const git_diff_delta*, const git_diff_hunk* hunk, void* payload)
{
    Collect* collect = static_cast<Collect*>(payload);
    if ( !collect->lines || !hunk )
        return 0;
    if ( collect->lines->size() >= kMaxLines )
    {
        if ( collect->truncated )
            *collect->truncated = true;
        return 0;
    }
    GitDiffLine line;
    line.kind = GitDiffLine::Kind::Hunk;
    line.text = LineText(hunk->header, hunk->header_len);
    collect->lines->push_back(line);
    return 0;
}

int LineCallback(const git_diff_delta*, const git_diff_hunk*, const git_diff_line* row, void* payload)
{
    Collect* collect = static_cast<Collect*>(payload);
    if ( !collect->lines || !row )
        return 0;
    if ( collect->lines->size() >= kMaxLines )
    {
        if ( collect->truncated )
            *collect->truncated = true;
        return 0;
    }
    GitDiffLine line;
    if ( row->origin == GIT_DIFF_LINE_ADDITION )
        line.kind = GitDiffLine::Kind::Addition;
    else if ( row->origin == GIT_DIFF_LINE_DELETION )
        line.kind = GitDiffLine::Kind::Deletion;
    else if ( row->origin == GIT_DIFF_LINE_CONTEXT )
        line.kind = GitDiffLine::Kind::Context;
    else
        return 0;
    line.oldLine = row->old_lineno;
    line.newLine = row->new_lineno;
    line.text = LineText(row->content, row->content_len);
    collect->lines->push_back(line);
    return 0;
}

bool ReadFileBytes(const wxString& path, std::vector<char>& bytes, wxString& error)
{
    if ( !wxFileName::FileExists(path) )
        return true;
    wxLogNull quiet;
    wxFFile file(path, "rb");
    if ( !file.IsOpened() )
    {
        error = "Could not read the working tree file.";
        return false;
    }
    const wxFileOffset length = file.Length();
    if ( length < 0 )
    {
        error = "Could not read the working tree file.";
        return false;
    }
    if ( static_cast<size_t>(length) > kMaxBytes )
    {
        error = "The working tree file is too large to diff.";
        return false;
    }
    bytes.resize(static_cast<size_t>(length));
    if ( length > 0 && file.Read(bytes.data(), static_cast<size_t>(length)) != static_cast<size_t>(length) )
    {
        error = "Could not read the working tree file.";
        return false;
    }
    return true;
}

} // namespace

GitWorktreeDiff QueryWorktreeDiff(const wxString& directory,
                                   const wxString& fullId,
                                   const wxString& relativePath)
{
    GitWorktreeDiff result;
    result.path = relativePath;
    git_libgit2_init();

    const wxScopedCharBuffer dirUtf8 = directory.utf8_str();
    git_buf discovered = GIT_BUF_INIT;
    if ( git_repository_discover(&discovered, dirUtf8.data(), 0, nullptr) != 0 )
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

    const char* workdir = git_repository_workdir(repo);
    if ( !workdir )
    {
        result.error = "This repository has no working tree.";
        git_repository_free(repo);
        return result;
    }

    git_oid oid;
    const wxScopedCharBuffer idUtf8 = fullId.utf8_str();
    git_commit* commit = nullptr;
    git_tree* tree = nullptr;
    git_tree_entry* entry = nullptr;
    git_blob* blob = nullptr;
    if ( git_oid_fromstr(&oid, idUtf8.data()) != 0 ||
         git_commit_lookup(&commit, repo, &oid) != 0 || !commit ||
         git_commit_tree(&tree, commit) != 0 || !tree )
    {
        result.error = GitLastError("Could not read the commit.");
        if ( tree )
            git_tree_free(tree);
        if ( commit )
            git_commit_free(commit);
        git_repository_free(repo);
        return result;
    }

    const wxScopedCharBuffer pathUtf8 = relativePath.utf8_str();
    if ( git_tree_entry_bypath(&entry, tree, pathUtf8.data()) == 0 && entry )
    {
        if ( git_blob_lookup(&blob, repo, git_tree_entry_id(entry)) != 0 )
            blob = nullptr;
        else if ( git_blob_rawsize(blob) > kMaxBytes )
        {
            result.error = "The committed file is too large to diff.";
            git_blob_free(blob);
            git_tree_entry_free(entry);
            git_tree_free(tree);
            git_commit_free(commit);
            git_repository_free(repo);
            return result;
        }
    }

    wxString relative = relativePath;
    relative.Replace("/", wxString(wxFileName::GetPathSeparator()));
    wxString workPath = wxString::FromUTF8(workdir);
    workPath.Replace("/", wxString(wxFileName::GetPathSeparator()));
    if ( !workPath.empty() && !workPath.EndsWith(wxFileName::GetPathSeparator()) )
        workPath += wxFileName::GetPathSeparator();
    workPath += relative;
    std::vector<char> workBytes;
    if ( !ReadFileBytes(workPath, workBytes, result.error) )
    {
        if ( blob )
            git_blob_free(blob);
        if ( entry )
            git_tree_entry_free(entry);
        git_tree_free(tree);
        git_commit_free(commit);
        git_repository_free(repo);
        return result;
    }

    bool binary = false;
    bool truncated = false;
    Collect collect;
    collect.lines = &result.lines;
    collect.binary = &binary;
    collect.truncated = &truncated;
    git_diff_options options = GIT_DIFF_OPTIONS_INIT;
    options.context_lines = 3;
    const int compared = git_diff_blob_to_buffer(
        blob, pathUtf8.data(),
        workBytes.empty() ? nullptr : workBytes.data(), workBytes.size(), pathUtf8.data(),
        &options, FileCallback, nullptr, HunkCallback, LineCallback, &collect);

    if ( blob )
        git_blob_free(blob);
    if ( entry )
        git_tree_entry_free(entry);
    git_tree_free(tree);
    git_commit_free(commit);
    git_repository_free(repo);

    if ( compared != 0 )
    {
        result.error = GitLastError("Could not diff the file.");
        result.lines.clear();
        return result;
    }

    result.binary = binary;
    result.truncated = truncated;
    result.identical = !binary && result.lines.empty();
    result.ok = true;
    return result;
}
