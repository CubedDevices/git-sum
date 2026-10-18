/////////////////////////////////////////////////////////////////////////////
// Name:        GitLog.cpp
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
#include "GitLog.h"

#include "git2.h"

#include "wx/datetime.h"
#include "wx/filename.h"

namespace
{

const size_t kMaxCommits = 500;

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

bool CommitTouches(git_repository* repo, git_commit* commit, const wxString& scope)
{
    if ( scope.empty() )
        return true;

    const wxScopedCharBuffer scopeUtf8 = scope.utf8_str();
    char* scopeSpec = const_cast<char*>(scopeUtf8.data());
    if ( !scopeSpec || !scopeSpec[0] )
        return true;

    git_tree* tree = nullptr;
    git_tree* parentTree = nullptr;
    git_commit* parent = nullptr;
    git_diff* diff = nullptr;
    bool touches = false;
    if ( git_commit_tree(&tree, commit) == 0 )
    {
        if ( git_commit_parentcount(commit) > 0 && git_commit_parent(&parent, commit, 0) == 0 )
            git_commit_tree(&parentTree, parent);

        git_diff_options options = GIT_DIFF_OPTIONS_INIT;
        options.pathspec.count = 1;
        options.pathspec.strings = &scopeSpec;
        if ( git_diff_tree_to_tree(&diff, repo, parentTree, tree, &options) == 0 &&
             diff && git_diff_num_deltas(diff) > 0 )
            touches = true;
    }

    if ( diff )
        git_diff_free(diff);
    if ( parentTree )
        git_tree_free(parentTree);
    if ( tree )
        git_tree_free(tree);
    if ( parent )
        git_commit_free(parent);
    return touches;
}

wxString GitLastError(const wxString& fallback)
{
    const git_error* error = git_error_last();
    if ( error && error->message && error->message[0] )
        return FromUtf8(error->message);
    return fallback;
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

wxString AuthorText(const git_signature* signature)
{
    if ( !signature )
        return wxString();
    wxString author = FromUtf8(signature->name);
    if ( signature->email && signature->email[0] )
    {
        if ( !author.empty() )
            author += " ";
        author += "<" + FromUtf8(signature->email) + ">";
    }
    return author;
}

} // namespace

GitLogResult QueryGitLog(const wxString& directory)
{
    GitLogResult result;
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

    git_revwalk* walk = nullptr;
    if ( git_revwalk_new(&walk, repo) != 0 )
    {
        result.error = GitLastError("Could not read history.");
        git_repository_free(repo);
        return result;
    }
    git_revwalk_sorting(walk, GIT_SORT_TIME);

    const int pushed = git_revwalk_push_head(walk);
    if ( pushed == GIT_EUNBORNBRANCH )
    {
        git_revwalk_free(walk);
        git_repository_free(repo);
        result.ok = true;
        return result;
    }
    if ( pushed != 0 )
    {
        result.error = GitLastError("Could not read history.");
        git_revwalk_free(walk);
        git_repository_free(repo);
        return result;
    }

    result.rows.reserve(32);
    size_t examined = 0;
    bool hitExamineLimit = false;
    while ( result.rows.size() < kMaxCommits )
    {
        git_oid oid;
        const int next = git_revwalk_next(&oid, walk);
        if ( next == GIT_ITEROVER )
            break;
        if ( next != 0 )
        {
            result.error = GitLastError("Could not read history.");
            git_revwalk_free(walk);
            git_repository_free(repo);
            return result;
        }

        git_commit* commit = nullptr;
        if ( git_commit_lookup(&commit, repo, &oid) != 0 )
        {
            result.error = GitLastError("Could not read a commit.");
            git_revwalk_free(walk);
            git_repository_free(repo);
            return result;
        }
        ++examined;
        if ( !CommitTouches(repo, commit, result.scope) )
        {
            git_commit_free(commit);
            if ( examined >= 2000 )
            {
                hitExamineLimit = true;
                break;
            }
            continue;
        }

        GitLogRow row;
        char shortId[8] = { 0 };
        git_oid_tostr(shortId, sizeof(shortId), &oid);
        row.id = wxString::FromUTF8(shortId);
        row.author = AuthorText(git_commit_author(commit));
        const wxDateTime when(static_cast<time_t>(git_commit_time(commit)));
        if ( when.IsValid() )
            row.date = when.Format("%Y-%m-%d %H:%M");
        row.summary = FromUtf8(git_commit_summary(commit));
        row.message = FromUtf8(git_commit_message(commit));
        result.rows.push_back(row);
        git_commit_free(commit);
    }

    if ( hitExamineLimit || result.rows.size() == kMaxCommits )
    {
        git_oid extra;
        result.truncated = git_revwalk_next(&extra, walk) == 0;
    }

    git_revwalk_free(walk);
    git_repository_free(repo);
    result.ok = true;
    return result;
}
