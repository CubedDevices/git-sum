/////////////////////////////////////////////////////////////////////////////
// Name:        GitStatus.cpp
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
#include "GitStatus.h"

#include "git2.h"

#include "wx/filename.h"

#include <cstring>

namespace
{

wxString FromUtf8(const char* text)
{
    if ( !text )
        return wxString();
    return wxString::FromUTF8(text);
}

wxString ScopePath(const wxString& directory, const wxString& workdir)
{
    wxFileName dir;
    dir.AssignDir(directory);
    dir.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE);
    wxFileName root;
    root.AssignDir(workdir);
    root.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE);

    const wxString dirPath = dir.GetPath();
    const wxString rootPath = root.GetPath();
    if ( dirPath.CmpNoCase(rootPath) == 0 || dirPath.length() <= rootPath.length() )
        return wxString();
    if ( dirPath.Left(rootPath.length()).CmpNoCase(rootPath) != 0 )
        return wxString();
    const wxChar separator = dirPath[rootPath.length()];
    if ( separator != wxFileName::GetPathSeparator() && separator != '/' && separator != '\\' )
        return wxString();

    wxString relative = dirPath.Mid(rootPath.length() + 1);
    relative.Replace("\\", "/");
    return relative;
}

wxString GitLastError(const wxString& fallback)
{
    const git_error* error = git_error_last();
    if ( error && error->message && error->message[0] )
        return FromUtf8(error->message);
    return fallback;
}

wxString DeltaPath(const git_diff_delta* delta)
{
    if ( !delta )
        return wxString();
    if ( delta->new_file.path && delta->new_file.path[0] )
        return FromUtf8(delta->new_file.path);
    return FromUtf8(delta->old_file.path);
}

void AddFlag(wxString& text, const wxString& flag)
{
    if ( !text.empty() )
        text += ", ";
    text += flag;
}

wxString SideText(unsigned status, bool indexSide, const git_diff_delta* delta)
{
    wxString text;
    if ( indexSide )
    {
        if ( status & GIT_STATUS_INDEX_NEW )
            AddFlag(text, "added");
        if ( status & GIT_STATUS_INDEX_MODIFIED )
            AddFlag(text, "modified");
        if ( status & GIT_STATUS_INDEX_DELETED )
            AddFlag(text, "deleted");
        if ( status & GIT_STATUS_INDEX_RENAMED )
            AddFlag(text, "renamed");
        if ( status & GIT_STATUS_INDEX_TYPECHANGE )
            AddFlag(text, "typechange");
    }
    else
    {
        if ( status & GIT_STATUS_WT_NEW )
            AddFlag(text, "untracked");
        if ( status & GIT_STATUS_WT_MODIFIED )
            AddFlag(text, "modified");
        if ( status & GIT_STATUS_WT_DELETED )
            AddFlag(text, "deleted");
        if ( status & GIT_STATUS_WT_TYPECHANGE )
            AddFlag(text, "typechange");
        if ( status & GIT_STATUS_WT_RENAMED )
            AddFlag(text, "renamed");
        if ( status & GIT_STATUS_WT_UNREADABLE )
            AddFlag(text, "unreadable");
        if ( status & GIT_STATUS_CONFLICTED )
            AddFlag(text, "conflict");
    }

    if ( (indexSide && (status & GIT_STATUS_INDEX_RENAMED)) ||
         (!indexSide && (status & GIT_STATUS_WT_RENAMED)) )
    {
        if ( delta && delta->old_file.path && delta->new_file.path &&
             std::strcmp(delta->old_file.path, delta->new_file.path) != 0 )
            AddFlag(text, "from " + FromUtf8(delta->old_file.path));
    }
    return text;
}

wxString BranchText(git_repository* repo)
{
    git_reference* head = nullptr;
    const int rc = git_repository_head(&head, repo);
    if ( rc == GIT_EUNBORNBRANCH )
    {
        git_reference* symbolic = nullptr;
        wxString name = "master";
        if ( git_reference_lookup(&symbolic, repo, "HEAD") == 0 )
        {
            const char* target = git_reference_symbolic_target(symbolic);
            if ( target )
            {
                const wxString full = FromUtf8(target);
                const wxString prefix = "refs/heads/";
                if ( full.StartsWith(prefix) )
                    name = full.Mid(prefix.length());
            }
            git_reference_free(symbolic);
        }
        return "No commits yet on " + name;
    }
    if ( rc != 0 )
        return "Unknown branch";

    wxString text;
    if ( git_repository_head_detached(repo) )
    {
        const git_oid* oid = git_reference_target(head);
        char shortId[16] = { 0 };
        if ( oid )
            git_oid_tostr(shortId, 8, oid);
        text = wxString("HEAD detached") + (shortId[0] ? " at " + wxString::FromUTF8(shortId) : wxString());
    }
    else
    {
        const char* name = git_reference_shorthand(head);
        text = "On branch " + (name ? FromUtf8(name) : wxString("HEAD"));
    }
    git_reference_free(head);
    return text;
}

} // namespace

GitStatusResult QueryGitStatus(const wxString& directory)
{
    GitStatusResult result;
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

    if ( const char* workdir = git_repository_workdir(repo) )
    {
        wxFileName root;
        root.AssignDir(FromUtf8(workdir));
        root.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
        result.root = root.GetPath();
    }
    result.branch = BranchText(repo);
    result.scope = ScopePath(directory, result.root);

    git_status_options options = GIT_STATUS_OPTIONS_INIT;
    options.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
    options.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
                    GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
                    GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
                    GIT_STATUS_OPT_RENAMES_INDEX_TO_WORKDIR |
                    GIT_STATUS_OPT_SORT_CASE_SENSITIVELY;

    const wxScopedCharBuffer scopeUtf8 = result.scope.utf8_str();
    char* scopeSpec = const_cast<char*>(scopeUtf8.data());
    if ( scopeSpec && scopeSpec[0] )
    {
        options.pathspec.count = 1;
        options.pathspec.strings = &scopeSpec;
    }

    git_status_list* list = nullptr;
    if ( git_status_list_new(&list, repo, &options) != 0 )
    {
        result.error = GitLastError("Could not read status.");
        git_repository_free(repo);
        return result;
    }

    const size_t count = git_status_list_entrycount(list);
    result.rows.reserve(count);
    for ( size_t i = 0; i < count; ++i )
    {
        const git_status_entry* entry = git_status_byindex(list, i);
        if ( !entry )
            continue;
        GitStatusRow row;
        row.path = DeltaPath(entry->index_to_workdir);
        if ( row.path.empty() )
            row.path = DeltaPath(entry->head_to_index);
        row.index = SideText(entry->status, true, entry->head_to_index);
        row.workTree = SideText(entry->status, false, entry->index_to_workdir);
        if ( !row.path.empty() )
            result.rows.push_back(row);
    }

    git_status_list_free(list);
    git_repository_free(repo);
    result.ok = true;
    return result;
}
