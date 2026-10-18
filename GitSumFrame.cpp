/////////////////////////////////////////////////////////////////////////////
// Name:        GitSumFrame.cpp
// Purpose:     Explorer-style main window for git-sum
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
#include "GitSumFrame.h"

#include "GitFetch.h"
#include "GitLog.h"
#include "GitStatus.h"

#include "git2.h"

#include "wx/artprov.h"
#include "wx/dir.h"
#include "wx/dirctrl.h"
#include "wx/file.h"
#include "wx/filename.h"
#include "wx/listctrl.h"
#include "wx/log.h"
#include "wx/menu.h"
#include "wx/msgdlg.h"
#include "wx/panel.h"
#include "wx/sizer.h"
#include "wx/splitter.h"
#include "wx/stattext.h"
#include "wx/statusbr.h"
#include "wx/stdpaths.h"
#include "wx/textdlg.h"
#include "wx/textctrl.h"
#include "wx/toolbar.h"
#include "wx/treectrl.h"

#include <algorithm>
#include <cstdio>

namespace
{

const int kGitIds[] =
{
    GitSumFrame::ID_GIT_STATUS,
    GitSumFrame::ID_GIT_ADD,
    GitSumFrame::ID_GIT_COMMIT,
    GitSumFrame::ID_GIT_DIFF,
    GitSumFrame::ID_GIT_LOG,
    GitSumFrame::ID_GIT_PUSH,
    GitSumFrame::ID_GIT_PULL,
    GitSumFrame::ID_GIT_FETCH
};

wxString NormalizePath(const wxString& path, bool asDir)
{
    wxFileName file;
    if ( asDir )
        file.AssignDir(path);
    else
        file.Assign(path);
    file.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE);
    wxString full = file.GetFullPath();
    const wxString sep = wxFileName::GetPathSeparator();
    if ( asDir && !full.empty() && !full.EndsWith(sep) )
        full += sep;
#ifdef __WXMSW__
    full.MakeLower();
#endif
    return full;
}

bool PathsEqual(const wxString& a, const wxString& b)
{
    const bool aDir = wxFileName::DirExists(a);
    const bool bDir = wxFileName::DirExists(b);
    return NormalizePath(a, aDir) == NormalizePath(b, bDir);
}

// True when child is parent or a path inside parent.
bool IsSameOrInside(const wxString& parentDir, const wxString& childPath)
{
    const wxString parent = NormalizePath(parentDir, true);
    const bool childIsDir = wxFileName::DirExists(childPath);
    const wxString child = NormalizePath(childPath, childIsDir);
    return child.StartsWith(parent);
}

bool IsVolumeRoot(const wxString& path)
{
    wxFileName dir;
    dir.AssignDir(path);
    dir.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS);
    return dir.GetDirCount() == 0;
}

bool IsGitControlledDirectory(const wxString& path)
{
    if ( !wxFileName::DirExists(path) )
        return false;

    wxFileName dir;
    dir.AssignDir(path);
    dir.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE);
    while ( true )
    {
        const wxString gitPath = wxFileName(dir.GetPath(), ".git").GetFullPath();
        if ( wxFileName::DirExists(gitPath) || wxFileName::FileExists(gitPath) )
            return true;
        if ( dir.GetDirCount() == 0 )
            return false;
        dir.RemoveLastDir();
    }
}

bool ValidNewName(const wxString& name, wxString& error)
{
    if ( name.empty() || name == "." || name == ".." )
    {
        error = "Enter a name.";
        return false;
    }
    if ( name.Find('/') != wxNOT_FOUND || name.Find('\\') != wxNOT_FOUND || name.Find(':') != wxNOT_FOUND )
    {
        error = "The name cannot contain path separators.";
        return false;
    }
    return true;
}

wxString JoinPath(const wxString& dir, const wxString& name)
{
    return wxFileName(dir, name).GetFullPath();
}

bool DirIsEmpty(const wxString& path)
{
    wxDir dir(path);
    if ( !dir.IsOpened() )
        return false;
    wxString name;
    return !dir.GetFirst(&name, wxString(), wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN);
}

bool RemoveTree(const wxString& path, wxString& error)
{
    wxLogNull quiet;
    if ( wxFileName::FileExists(path) )
    {
        // Git object files are read-only. Clearing that bit lets the delete succeed.
        wxChmod(path, 0666);
        if ( !wxRemoveFile(path) )
        {
            error = "Could not delete \"" + path + "\".";
            return false;
        }
        return true;
    }
    if ( !wxFileName::DirExists(path) )
        return true;

    wxArrayString names;
    {
        wxDir dir(path);
        if ( !dir.IsOpened() )
        {
            error = "Could not open \"" + path + "\".";
            return false;
        }
        wxString name;
        bool more = dir.GetFirst(&name, wxString(), wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN);
        while ( more )
        {
            if ( name != "." && name != ".." )
                names.Add(name);
            more = dir.GetNext(&name);
        }
    }
    for ( size_t i = 0; i < names.GetCount(); ++i )
    {
        if ( !RemoveTree(JoinPath(path, names[i]), error) )
            return false;
    }
    wxChmod(path, 0777);
    if ( !wxFileName::Rmdir(path) )
    {
        error = "Could not delete \"" + path + "\".";
        return false;
    }
    return true;
}

bool RemovePath(const wxString& path, wxString& error)
{
    return RemoveTree(path, error);
}

bool CopyDirectory(const wxString& src, const wxString& dest, wxString& error)
{
    wxLogNull quiet;
    if ( !wxFileName::Mkdir(dest, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) )
    {
        error = "Could not create \"" + dest + "\".";
        return false;
    }

    wxDir dir(src);
    if ( !dir.IsOpened() )
    {
        error = "Could not open \"" + src + "\".";
        return false;
    }

    wxString name;
    bool more = dir.GetFirst(&name, wxString(), wxDIR_FILES | wxDIR_HIDDEN);
    while ( more )
    {
        if ( !wxCopyFile(JoinPath(src, name), JoinPath(dest, name), true) )
        {
            error = "Could not copy \"" + name + "\".";
            return false;
        }
        more = dir.GetNext(&name);
    }

    more = dir.GetFirst(&name, wxString(), wxDIR_DIRS | wxDIR_HIDDEN);
    while ( more )
    {
        if ( name != "." && name != ".." )
        {
            if ( !CopyDirectory(JoinPath(src, name), JoinPath(dest, name), error) )
                return false;
        }
        more = dir.GetNext(&name);
    }
    return true;
}

bool CopyPath(const wxString& src, const wxString& dest, wxString& error)
{
    wxLogNull quiet;
    if ( wxFileName::DirExists(src) )
        return CopyDirectory(src, dest, error);
    if ( !wxCopyFile(src, dest, true) )
    {
        error = "Could not copy \"" + src + "\".";
        return false;
    }
    return true;
}

wxString ReadAll(const wxString& path)
{
    wxFile file(path);
    wxString text;
    if ( file.IsOpened() )
        file.ReadAll(&text);
    return text;
}

bool WriteAll(const wxString& path, const wxString& text)
{
    wxFile file(path, wxFile::write);
    if ( !file.IsOpened() )
        return false;
    return file.Write(text);
}

bool CommitFile(const wxString& repoPath, const char* relativePath, const char* message, wxString& error)
{
    git_repository* repo = nullptr;
    const wxScopedCharBuffer utf8 = repoPath.utf8_str();
    if ( git_repository_open(&repo, utf8.data()) != 0 || !repo )
    {
        error = "could not open the repository";
        return false;
    }

    git_signature* signature = nullptr;
    git_index* index = nullptr;
    git_tree* tree = nullptr;
    bool committed = false;
    if ( git_signature_now(&signature, "Ada", "ada@example.com") == 0 &&
         git_repository_index(&index, repo) == 0 &&
         git_index_add_bypath(index, relativePath) == 0 &&
         git_index_write(index) == 0 )
    {
        git_oid treeId;
        if ( git_index_write_tree(&treeId, index) == 0 &&
             git_tree_lookup(&tree, repo, &treeId) == 0 )
        {
            const git_commit* parents[1] = { nullptr };
            size_t parentCount = 0;
            git_commit* parentCommit = nullptr;
            bool parentReady = false;
            if ( git_repository_head_unborn(repo) == 1 )
                parentReady = true;
            else
            {
                git_oid parentId;
                parentReady = git_reference_name_to_id(&parentId, repo, "HEAD") == 0 &&
                              git_commit_lookup(&parentCommit, repo, &parentId) == 0;
                if ( parentReady )
                {
                    parents[0] = parentCommit;
                    parentCount = 1;
                }
            }
            if ( parentReady )
            {
                git_oid commitId;
                committed = git_commit_create(&commitId, repo, "HEAD", signature, signature,
                                              nullptr, message, tree, parentCount,
                                              parentCount ? parents : nullptr) == 0;
            }
            if ( parentCommit )
                git_commit_free(parentCommit);
        }
    }
    if ( !committed )
        error = "could not commit";

    if ( tree )
        git_tree_free(tree);
    if ( index )
        git_index_free(index);
    if ( signature )
        git_signature_free(signature);
    git_repository_free(repo);
    return committed;
}

bool Fail(const wxString& message)
{
    const wxScopedCharBuffer buf = ("git-sum self-test failed: " + message).utf8_str();
    std::fprintf(stderr, "%s\n", buf.data() ? buf.data() : "");
    return false;
}

} // namespace

GitSumFrame::GitSumFrame(const GitSumConfig& config, bool silentNotices)
    : wxFrame(nullptr, wxID_ANY, "git-sum", wxDefaultPosition, wxSize(1020, 680))
    , m_config(config)
    , m_dirCtrl(nullptr)
    , m_right(nullptr)
    , m_summary(nullptr)
    , m_commitMessage(nullptr)
    , m_list(nullptr)
    , m_address(nullptr)
    , m_pane(Pane::Files)
    , m_columns(Pane::Files)
    , m_statusSortColumn(0)
    , m_statusSortAscending(true)
    , m_settingPath(false)
    , m_silentNotices(silentNotices)
    , m_scriptedAnswer(0)
{
    wxString start = m_config.LastPath();
    if ( !wxFileName::DirExists(start) )
        start = wxGetHomeDir();
    m_currentDir = start;

    CreateStatusBar(3);
    const int widths[3] = { -1, 160, 120 };
    SetStatusWidths(3, widths);

    BuildMenus();
    BuildToolBar();
    BuildBody();

    m_settingPath = true;
    m_dirCtrl->ShowHidden(true);
    m_settingPath = false;
    BindDirEvents();

    Bind(wxEVT_CLOSE_WINDOW, &GitSumFrame::OnClose, this);
    ShowDirectory(m_currentDir, true);
    SetMinSize(wxSize(640, 400));
    Centre();
}

GitSumFrame::~GitSumFrame()
{
    // Ignore tree and list events while child windows are destroyed.
    m_settingPath = true;
    if ( m_dirCtrl )
    {
        if ( wxTreeCtrl* tree = m_dirCtrl->GetTreeCtrl() )
        {
            tree->Unbind(wxEVT_TREE_SEL_CHANGED, &GitSumFrame::OnDirSelected, this);
            tree->Unbind(wxEVT_TREE_ITEM_RIGHT_CLICK, &GitSumFrame::OnDirRightClick, this);
        }
    }
    if ( m_list )
    {
        m_list->Unbind(wxEVT_LIST_ITEM_ACTIVATED, &GitSumFrame::OnListActivated, this);
        m_list->Unbind(wxEVT_LIST_ITEM_SELECTED, &GitSumFrame::OnListSelection, this);
        m_list->Unbind(wxEVT_LIST_ITEM_DESELECTED, &GitSumFrame::OnListSelection, this);
        m_list->Unbind(wxEVT_KEY_DOWN, &GitSumFrame::OnListKey, this);
        m_list->Unbind(wxEVT_CONTEXT_MENU, &GitSumFrame::OnListContext, this);
        m_list->Unbind(wxEVT_LIST_COL_CLICK, &GitSumFrame::OnListColumn, this);
    }
}

void GitSumFrame::BuildMenus()
{
    wxMenu* file = new wxMenu;
    file->Append(ID_NEW_FOLDER, "New &folder");
    file->Append(ID_NEW_FILE, "New &file");
    file->AppendSeparator();
    file->Append(wxID_EXIT, "E&xit");

    wxMenu* edit = new wxMenu;
    edit->Append(ID_COPY_PATHS, "&Copy");
    edit->Append(ID_PASTE_PATHS, "&Paste");
    edit->Append(ID_DELETE_PATHS, "&Delete");

    wxMenu* git = new wxMenu;
    git->Append(ID_GIT_INIT, "&Init");
    git->AppendSeparator();
    git->AppendCheckItem(ID_GIT_STATUS, "&Status");
    git->Append(ID_GIT_ADD, "&Add");
    git->Append(ID_GIT_COMMIT, "&Commit");
    git->Append(ID_GIT_DIFF, "&Diff");
    git->AppendCheckItem(ID_GIT_LOG, "&Log");
    git->Append(ID_GIT_FETCH, "&Fetch");
    git->Append(ID_GIT_PUSH, "P&ush");
    git->Append(ID_GIT_PULL, "Pu&ll");

    wxMenu* help = new wxMenu;
    help->Append(wxID_ABOUT, "&About");

    wxMenuBar* bar = new wxMenuBar;
    bar->Append(file, "&File");
    bar->Append(edit, "&Edit");
    bar->Append(git, "&Git");
    bar->Append(help, "&Help");
    SetMenuBar(bar);

    Bind(wxEVT_MENU, &GitSumFrame::OnNewFolder, this, ID_NEW_FOLDER);
    Bind(wxEVT_MENU, &GitSumFrame::OnNewFile, this, ID_NEW_FILE);
    Bind(wxEVT_MENU, &GitSumFrame::OnCopy, this, ID_COPY_PATHS);
    Bind(wxEVT_MENU, &GitSumFrame::OnPaste, this, ID_PASTE_PATHS);
    Bind(wxEVT_MENU, &GitSumFrame::OnDelete, this, ID_DELETE_PATHS);
    Bind(wxEVT_MENU, &GitSumFrame::OnGitStatus, this, ID_GIT_STATUS);
    Bind(wxEVT_MENU, &GitSumFrame::OnGitInit, this, ID_GIT_INIT);
    Bind(wxEVT_MENU, &GitSumFrame::OnGitLog, this, ID_GIT_LOG);
    Bind(wxEVT_MENU, &GitSumFrame::OnGitStub, this, ID_GIT_ADD);
    Bind(wxEVT_MENU, &GitSumFrame::OnGitStub, this, ID_GIT_COMMIT);
    Bind(wxEVT_MENU, &GitSumFrame::OnGitStub, this, ID_GIT_DIFF);
    Bind(wxEVT_MENU, &GitSumFrame::OnGitFetch, this, ID_GIT_FETCH);
    Bind(wxEVT_MENU, &GitSumFrame::OnGitStub, this, ID_GIT_PUSH);
    Bind(wxEVT_MENU, &GitSumFrame::OnGitStub, this, ID_GIT_PULL);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(false); }, wxID_EXIT);
    Bind(wxEVT_MENU, &GitSumFrame::OnAbout, this, wxID_ABOUT);
}

void GitSumFrame::BuildToolBar()
{
    wxToolBar* bar = CreateToolBar(wxTB_FLAT | wxTB_TEXT | wxTB_HORZ_LAYOUT);
    bar->AddTool(ID_UP, "Up", wxArtProvider::GetBitmap(wxART_GO_UP, wxART_TOOLBAR), "Up one folder");
    bar->AddSeparator();
    bar->AddTool(ID_NEW_FOLDER, "New folder", wxArtProvider::GetBitmap(wxART_NEW_DIR, wxART_TOOLBAR), "New folder");
    bar->AddTool(ID_NEW_FILE, "New file", wxArtProvider::GetBitmap(wxART_NEW, wxART_TOOLBAR), "New file");
    bar->AddTool(ID_COPY_PATHS, "Copy", wxArtProvider::GetBitmap(wxART_COPY, wxART_TOOLBAR), "Copy");
    bar->AddTool(ID_PASTE_PATHS, "Paste", wxArtProvider::GetBitmap(wxART_PASTE, wxART_TOOLBAR), "Paste");
    bar->AddTool(ID_DELETE_PATHS, "Delete", wxArtProvider::GetBitmap(wxART_DELETE, wxART_TOOLBAR), "Delete");
    bar->AddSeparator();
    bar->AddTool(ID_GIT_INIT, "Init", wxArtProvider::GetBitmap(wxART_NEW_DIR, wxART_TOOLBAR), "Initialize a Git repository");
    bar->AddTool(ID_GIT_STATUS, "Status", wxArtProvider::GetBitmap(wxART_LIST_VIEW, wxART_TOOLBAR),
                 wxNullBitmap, wxITEM_CHECK, "Show or hide status");
    bar->AddTool(ID_GIT_ADD, "Add", wxArtProvider::GetBitmap(wxART_PLUS, wxART_TOOLBAR), "Add");
    bar->AddTool(ID_GIT_COMMIT, "Commit", wxArtProvider::GetBitmap(wxART_TICK_MARK, wxART_TOOLBAR), "Commit");
    bar->AddTool(ID_GIT_DIFF, "Diff", wxArtProvider::GetBitmap(wxART_FIND, wxART_TOOLBAR), "Diff");
    bar->AddTool(ID_GIT_LOG, "Log", wxArtProvider::GetBitmap(wxART_HELP_BOOK, wxART_TOOLBAR),
                 wxNullBitmap, wxITEM_CHECK, "Show or hide log");
    bar->AddTool(ID_GIT_FETCH, "Fetch", wxArtProvider::GetBitmap(wxART_REDO, wxART_TOOLBAR), "Fetch from remotes");
    bar->AddTool(ID_GIT_PUSH, "Push", wxArtProvider::GetBitmap(wxART_GO_UP, wxART_TOOLBAR), "Push");
    bar->AddTool(ID_GIT_PULL, "Pull", wxArtProvider::GetBitmap(wxART_GO_DOWN, wxART_TOOLBAR), "Pull");
    bar->Realize();

    Bind(wxEVT_TOOL, &GitSumFrame::OnUp, this, ID_UP);
    Bind(wxEVT_TOOL, &GitSumFrame::OnNewFolder, this, ID_NEW_FOLDER);
    Bind(wxEVT_TOOL, &GitSumFrame::OnNewFile, this, ID_NEW_FILE);
    Bind(wxEVT_TOOL, &GitSumFrame::OnCopy, this, ID_COPY_PATHS);
    Bind(wxEVT_TOOL, &GitSumFrame::OnPaste, this, ID_PASTE_PATHS);
    Bind(wxEVT_TOOL, &GitSumFrame::OnDelete, this, ID_DELETE_PATHS);
    Bind(wxEVT_TOOL, &GitSumFrame::OnGitStatus, this, ID_GIT_STATUS);
    Bind(wxEVT_TOOL, &GitSumFrame::OnGitInit, this, ID_GIT_INIT);
    Bind(wxEVT_TOOL, &GitSumFrame::OnGitLog, this, ID_GIT_LOG);
    Bind(wxEVT_TOOL, &GitSumFrame::OnGitStub, this, ID_GIT_ADD);
    Bind(wxEVT_TOOL, &GitSumFrame::OnGitStub, this, ID_GIT_COMMIT);
    Bind(wxEVT_TOOL, &GitSumFrame::OnGitStub, this, ID_GIT_DIFF);
    Bind(wxEVT_TOOL, &GitSumFrame::OnGitFetch, this, ID_GIT_FETCH);
    Bind(wxEVT_TOOL, &GitSumFrame::OnGitStub, this, ID_GIT_PUSH);
    Bind(wxEVT_TOOL, &GitSumFrame::OnGitStub, this, ID_GIT_PULL);
}

void GitSumFrame::BuildBody()
{
    wxBoxSizer* top = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(this, wxID_ANY, "Address"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_address = new wxTextCtrl(this, ID_ADDRESS, m_currentDir, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    row->Add(m_address, 1, wxEXPAND);
    top->Add(row, 0, wxEXPAND | wxALL, 6);

    wxSplitterWindow* splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    m_dirCtrl = new wxGenericDirCtrl(splitter, wxID_ANY, m_currentDir,
                                      wxDefaultPosition, wxDefaultSize,
                                      wxDIRCTRL_DIR_ONLY | wxDIRCTRL_3D_INTERNAL);
    m_right = new wxPanel(splitter, wxID_ANY);
    wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);
    m_summary = new wxStaticText(m_right, wxID_ANY, wxString());
    m_summary->Hide();
    rightSizer->Add(m_summary, 0, wxEXPAND | wxALL, 6);
    m_list = new wxListCtrl(m_right, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);
    m_list->AppendColumn("Name", wxLIST_FORMAT_LEFT, 280);
    m_list->AppendColumn("Size", wxLIST_FORMAT_RIGHT, 100);
    m_list->AppendColumn("Modified", wxLIST_FORMAT_LEFT, 160);
    rightSizer->Add(m_list, 3, wxEXPAND);
    m_commitMessage = new wxTextCtrl(m_right, wxID_ANY, wxString(), wxDefaultPosition, wxSize(-1, 140),
                                     wxTE_MULTILINE | wxTE_READONLY | wxTE_BESTWRAP);
    m_commitMessage->Hide();
    rightSizer->Add(m_commitMessage, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
    rightSizer->Hide(m_commitMessage);
    m_right->SetSizer(rightSizer);

    wxImageList* images = new wxImageList(16, 16, true);
    images->Add(wxArtProvider::GetBitmap(wxART_FOLDER, wxART_LIST, wxSize(16, 16)));
    images->Add(wxArtProvider::GetBitmap(wxART_NORMAL_FILE, wxART_LIST, wxSize(16, 16)));
    m_list->AssignImageList(images, wxIMAGE_LIST_SMALL);

    splitter->SplitVertically(m_dirCtrl, m_right, 280);
    splitter->SetMinimumPaneSize(160);
    top->Add(splitter, 1, wxEXPAND);
    SetSizer(top);

    m_address->Bind(wxEVT_TEXT_ENTER, &GitSumFrame::OnAddressEnter, this);
    m_list->Bind(wxEVT_LIST_ITEM_ACTIVATED, &GitSumFrame::OnListActivated, this);
    m_list->Bind(wxEVT_LIST_ITEM_SELECTED, &GitSumFrame::OnListSelection, this);
    m_list->Bind(wxEVT_LIST_ITEM_DESELECTED, &GitSumFrame::OnListSelection, this);
    m_list->Bind(wxEVT_KEY_DOWN, &GitSumFrame::OnListKey, this);
    m_list->Bind(wxEVT_CONTEXT_MENU, &GitSumFrame::OnListContext, this);
    m_list->Bind(wxEVT_LIST_COL_CLICK, &GitSumFrame::OnListColumn, this);
}

void GitSumFrame::BindDirEvents()
{
    wxTreeCtrl* tree = m_dirCtrl->GetTreeCtrl();
    tree->Bind(wxEVT_TREE_SEL_CHANGED, &GitSumFrame::OnDirSelected, this);
    tree->Bind(wxEVT_TREE_ITEM_RIGHT_CLICK, &GitSumFrame::OnDirRightClick, this);
}

void GitSumFrame::ShowDirectory(const wxString& path, bool updateTree)
{
    if ( !wxFileName::DirExists(path) )
    {
        Notify("Folder does not exist:\n" + path, wxICON_WARNING);
        if ( m_address )
            m_address->ChangeValue(m_currentDir);
        return;
    }

    wxFileName dir;
    dir.AssignDir(path);
    dir.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE);
    m_currentDir = dir.GetPath();
    SetTitle("git-sum - " + m_currentDir);
    if ( m_address )
        m_address->ChangeValue(m_currentDir);

    if ( updateTree && m_dirCtrl )
    {
        m_settingPath = true;
        m_dirCtrl->SetPath(m_currentDir);
        m_settingPath = false;
    }

    m_config.SetLastPath(m_currentDir);
    m_config.Save();
    if ( m_pane != Pane::Files && !IsGitControlledDirectory(m_currentDir) )
        m_pane = Pane::Files;
    SyncToggles();
    RefreshPane();
    UpdateCommandState();
}

void GitSumFrame::ShowPane(Pane pane)
{
    if ( pane != Pane::Files )
    {
        const wxString path = GitSelectionPath();
        if ( path.empty() || !IsGitControlledDirectory(path) )
        {
            Notify("Select a Git directory.", wxICON_INFORMATION);
            SyncToggles();
            return;
        }
        if ( !PathsEqual(path, m_currentDir) )
            ShowDirectory(path, true);
    }

    m_pane = pane;
    SyncToggles();
    RefreshPane();
    UpdateCommandState();
}

void GitSumFrame::RefreshPane()
{
    const bool setting = m_settingPath;
    m_settingPath = true;
    if ( m_pane != m_columns )
    {
        SetupColumns();
        m_columns = m_pane;
    }

    if ( m_summary )
        m_summary->Show(m_pane != Pane::Files);
    if ( m_commitMessage && m_right && m_right->GetSizer() )
        m_right->GetSizer()->Show(m_commitMessage, m_pane == Pane::Log);

    if ( m_pane == Pane::Files )
        FillList();
    else if ( m_pane == Pane::Status )
        FillStatus();
    else
        FillLog();

    if ( m_right )
        m_right->Layout();
    m_settingPath = setting;
}

void GitSumFrame::SyncToggles()
{
    const bool status = m_pane == Pane::Status;
    const bool log = m_pane == Pane::Log;
    if ( wxMenuBar* bar = GetMenuBar() )
    {
        bar->Check(ID_GIT_STATUS, status);
        bar->Check(ID_GIT_LOG, log);
    }
    if ( wxToolBar* tools = GetToolBar() )
    {
        tools->ToggleTool(ID_GIT_STATUS, status);
        tools->ToggleTool(ID_GIT_LOG, log);
    }
}

void GitSumFrame::SetupColumns()
{
    m_list->ClearAll();
    if ( m_pane == Pane::Status )
    {
        m_list->AppendColumn("Path", wxLIST_FORMAT_LEFT, 420);
        m_list->AppendColumn("Index", wxLIST_FORMAT_LEFT, 140);
        m_list->AppendColumn("Work tree", wxLIST_FORMAT_LEFT, 160);
    }
    else if ( m_pane == Pane::Log )
    {
        m_list->AppendColumn("Commit", wxLIST_FORMAT_LEFT, 90);
        m_list->AppendColumn("Author", wxLIST_FORMAT_LEFT, 200);
        m_list->AppendColumn("Date", wxLIST_FORMAT_LEFT, 140);
        m_list->AppendColumn("Message", wxLIST_FORMAT_LEFT, 360);
    }
    else
    {
        m_list->AppendColumn("Name", wxLIST_FORMAT_LEFT, 280);
        m_list->AppendColumn("Size", wxLIST_FORMAT_RIGHT, 100);
        m_list->AppendColumn("Modified", wxLIST_FORMAT_LEFT, 160);
    }
}

void GitSumFrame::SetSummary(const wxString& text)
{
    if ( !m_summary )
        return;
    m_summary->SetLabel(text);
    int width = m_right ? m_right->GetClientSize().GetWidth() - 16 : 480;
    if ( width < 160 )
        width = 480;
    m_summary->Wrap(width);
    m_summary->Show(m_pane != Pane::Files);
}

void GitSumFrame::FillList()
{
    m_entries.clear();
    m_list->Freeze();
    m_list->DeleteAllItems();

    wxDir dir(m_currentDir);
    if ( !dir.IsOpened() )
    {
        m_list->Thaw();
        SetStatusText("Cannot open " + m_currentDir, 0);
        SetStatusText(wxString(), 2);
        return;
    }

    wxString name;
    bool more = dir.GetFirst(&name, wxString(), wxDIR_DIRS | wxDIR_HIDDEN);
    while ( more )
    {
        if ( name != "." && name != ".." )
        {
            Entry entry;
            entry.name = name;
            entry.path = JoinPath(m_currentDir, name);
            entry.isDir = true;
            wxFileName info;
            info.AssignDir(entry.path);
            info.GetTimes(nullptr, &entry.modified, nullptr);
            m_entries.push_back(entry);
        }
        more = dir.GetNext(&name);
    }

    more = dir.GetFirst(&name, wxString(), wxDIR_FILES | wxDIR_HIDDEN);
    while ( more )
    {
        Entry entry;
        entry.name = name;
        entry.path = JoinPath(m_currentDir, name);
        entry.isDir = false;
        wxFileName info(entry.path);
        entry.size = info.GetSize();
        info.GetTimes(nullptr, &entry.modified, nullptr);
        m_entries.push_back(entry);
        more = dir.GetNext(&name);
    }

    std::sort(m_entries.begin(), m_entries.end(),
              [](const Entry& a, const Entry& b)
              {
                  if ( a.isDir != b.isDir )
                      return a.isDir;
                  return a.name.CmpNoCase(b.name) < 0;
              });

    for ( size_t i = 0; i < m_entries.size(); ++i )
    {
        const Entry& entry = m_entries[i];
        const long row = m_list->InsertItem(m_list->GetItemCount(), entry.name, entry.isDir ? 0 : 1);
        wxString sizeText;
        if ( !entry.isDir && entry.size != wxInvalidSize )
            sizeText = wxFileName::GetHumanReadableSize(entry.size);
        m_list->SetItem(row, 1, sizeText);
        m_list->SetItem(row, 2, entry.modified.IsValid() ? entry.modified.Format("%Y-%m-%d %H:%M") : wxString());
    }

    m_list->Thaw();
    SetStatusText(m_currentDir, 0);
    SetStatusText(wxString::Format("%u items", static_cast<unsigned>(m_entries.size())), 2);
}

void GitSumFrame::FillStatus()
{
    m_entries.clear();
    m_list->Freeze();
    m_list->DeleteAllItems();

    const GitStatusResult result = QueryGitStatus(m_currentDir);
    if ( !result.ok )
    {
        SetSummary(result.error);
        m_list->Thaw();
        SetStatusText(result.error, 0);
        SetStatusText(wxString(), 2);
        return;
    }

    wxString summary = result.branch;
    if ( !result.root.empty() )
        summary += "\n" + result.root;
    if ( !result.scope.empty() )
        summary += "\n" + result.scope;
    if ( result.rows.empty() )
        summary += "\nnothing to commit, working tree clean";
    else
        summary += wxString::Format("\n%u change(s)", static_cast<unsigned>(result.rows.size()));
    SetSummary(summary);

    for ( size_t i = 0; i < result.rows.size(); ++i )
    {
        const long row = m_list->InsertItem(m_list->GetItemCount(), result.rows[i].path);
        m_list->SetItem(row, 1, result.rows[i].index);
        m_list->SetItem(row, 2, result.rows[i].workTree);
    }

    m_list->Thaw();
    SortStatusList();
    SetStatusText(result.branch, 0);
    SetStatusText(result.rows.empty()
                      ? wxString("clean")
                      : wxString::Format("%u changes", static_cast<unsigned>(result.rows.size())),
                  2);
}

void GitSumFrame::SortStatusList()
{
    if ( m_pane != Pane::Status || !m_list )
        return;

    struct Item
    {
        wxString path;
        wxString index;
        wxString workTree;
    };
    std::vector<Item> items;
    const long count = m_list->GetItemCount();
    items.reserve(static_cast<size_t>(count));
    for ( long i = 0; i < count; ++i )
    {
        Item item;
        item.path = m_list->GetItemText(i, 0);
        item.index = m_list->GetItemText(i, 1);
        item.workTree = m_list->GetItemText(i, 2);
        items.push_back(item);
    }

    const int column = m_statusSortColumn;
    const bool ascending = m_statusSortAscending;
    std::sort(items.begin(), items.end(),
              [column, ascending](const Item& a, const Item& b)
              {
                  const wxString& left = column == 1 ? a.index : column == 2 ? a.workTree : a.path;
                  const wxString& right = column == 1 ? b.index : column == 2 ? b.workTree : b.path;
                  const int compared = left.CmpNoCase(right);
                  if ( compared == 0 )
                      return a.path.CmpNoCase(b.path) < 0;
                  return ascending ? compared < 0 : compared > 0;
              });

    const bool setting = m_settingPath;
    m_settingPath = true;
    m_list->Freeze();
    m_list->DeleteAllItems();
    for ( size_t i = 0; i < items.size(); ++i )
    {
        const long row = m_list->InsertItem(m_list->GetItemCount(), items[i].path);
        m_list->SetItem(row, 1, items[i].index);
        m_list->SetItem(row, 2, items[i].workTree);
    }
    m_list->Thaw();
    m_settingPath = setting;
}

void GitSumFrame::FillLog()
{
    m_entries.clear();
    m_list->Freeze();
    m_list->DeleteAllItems();

    const GitLogResult result = QueryGitLog(m_currentDir);
    if ( !result.ok )
    {
        m_logMessages.clear();
        ShowCommitMessage(-1);
        SetSummary(result.error);
        m_list->Thaw();
        SetStatusText(result.error, 0);
        SetStatusText(wxString(), 2);
        return;
    }

    wxString summary = result.branch;
    if ( !result.root.empty() )
        summary += "\n" + result.root;
    if ( !result.scope.empty() )
        summary += "\n" + result.scope;
    if ( result.rows.empty() )
        summary += "\nNo commits yet";
    else
        summary += wxString::Format("\n%u commit(s)", static_cast<unsigned>(result.rows.size()));
    if ( result.truncated )
        summary += "\nShowing the latest 500 commits";
    SetSummary(summary);

    m_logMessages.clear();
    m_logMessages.reserve(result.rows.size());
    for ( size_t i = 0; i < result.rows.size(); ++i )
    {
        const long row = m_list->InsertItem(m_list->GetItemCount(), result.rows[i].id);
        m_list->SetItem(row, 1, result.rows[i].author);
        m_list->SetItem(row, 2, result.rows[i].date);
        m_list->SetItem(row, 3, result.rows[i].summary);
        m_logMessages.push_back(result.rows[i].message);
    }
    if ( !m_logMessages.empty() )
        m_list->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    ShowCommitMessage(m_logMessages.empty() ? -1 : 0);

    m_list->Thaw();
    SetStatusText(result.branch, 0);
    SetStatusText(wxString::Format("%u commits", static_cast<unsigned>(result.rows.size())), 2);
}

void GitSumFrame::ShowCommitMessage(long index)
{
    if ( !m_commitMessage )
        return;
    if ( index < 0 || static_cast<size_t>(index) >= m_logMessages.size() )
        m_commitMessage->ChangeValue(wxString());
    else
        m_commitMessage->ChangeValue(m_logMessages[static_cast<size_t>(index)]);
}

void GitSumFrame::RefreshTree()
{
    if ( !m_dirCtrl )
        return;
    m_settingPath = true;
    m_dirCtrl->ReCreateTree();
    m_dirCtrl->SetPath(m_currentDir);
    m_settingPath = false;
}

void GitSumFrame::GoUp()
{
    if ( IsVolumeRoot(m_currentDir) )
        return;
    wxFileName dir;
    dir.AssignDir(m_currentDir);
    dir.RemoveLastDir();
    ShowDirectory(dir.GetPath(), true);
}

void GitSumFrame::UpdateCommandState()
{
    const bool haveSelection = !SelectedPaths().empty();
    const bool canPaste = !m_copiedPaths.empty();
    const wxString gitPath = GitSelectionPath();
    const bool git = !gitPath.empty() && IsGitControlledDirectory(gitPath);
    const bool canUp = !IsVolumeRoot(m_currentDir);

    GetMenuBar()->Enable(ID_COPY_PATHS, haveSelection);
    GetMenuBar()->Enable(ID_PASTE_PATHS, canPaste);
    GetMenuBar()->Enable(ID_DELETE_PATHS, haveSelection);
    for ( int id : kGitIds )
        GetMenuBar()->Enable(id, git);
    const bool canInit = !gitPath.empty() && wxFileName::DirExists(gitPath) && !git;
    GetMenuBar()->Enable(ID_GIT_INIT, canInit);

    wxToolBar* tools = GetToolBar();
    if ( tools )
    {
        tools->EnableTool(ID_UP, canUp);
        tools->EnableTool(ID_COPY_PATHS, haveSelection);
        tools->EnableTool(ID_PASTE_PATHS, canPaste);
        tools->EnableTool(ID_DELETE_PATHS, haveSelection);
        for ( int id : kGitIds )
            tools->EnableTool(id, git);
        tools->EnableTool(ID_GIT_INIT, canInit);
    }

    SetStatusText(git ? "Git repository" : wxString(), 1);
}

wxArrayString GitSumFrame::SelectedPaths() const
{
    wxArrayString paths;
    if ( !m_contextTreePath.empty() )
    {
        paths.Add(m_contextTreePath);
        return paths;
    }
    if ( m_pane != Pane::Files )
        return paths;

    long item = -1;
    while ( (item = m_list->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != -1 )
    {
        if ( item >= 0 && static_cast<size_t>(item) < m_entries.size() )
            paths.Add(m_entries[static_cast<size_t>(item)].path);
    }
    return paths;
}

wxString GitSumFrame::TargetDirectory() const
{
    if ( !m_contextTreePath.empty() )
        return m_contextTreePath;
    return m_currentDir;
}

wxString GitSumFrame::GitSelectionPath() const
{
    if ( !m_contextTreePath.empty() )
        return m_contextTreePath;

    if ( m_pane != Pane::Files )
        return m_currentDir;

    const int count = m_list->GetSelectedItemCount();
    if ( count == 0 )
        return m_currentDir;
    if ( count != 1 )
        return wxString();

    const long item = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if ( item < 0 || static_cast<size_t>(item) >= m_entries.size() )
        return wxString();
    if ( !m_entries[static_cast<size_t>(item)].isDir )
        return wxString();
    return m_entries[static_cast<size_t>(item)].path;
}

bool GitSumFrame::CommandEnabled(int id) const
{
    const bool menu = GetMenuBar() && GetMenuBar()->IsEnabled(id);
    const bool tool = GetToolBar() && GetToolBar()->GetToolEnabled(id);
    return menu && tool;
}

void GitSumFrame::Notify(const wxString& message, int icon)
{
    m_lastNotice = message;
    if ( m_silentNotices )
        return;
    wxMessageBox(message, "git-sum", wxOK | icon, this);
}

int GitSumFrame::Ask(const wxString& message, int style)
{
    if ( m_scriptedAnswer != 0 )
        return m_scriptedAnswer;
    return wxMessageBox(message, "git-sum", style, this);
}

void GitSumFrame::CopySelection()
{
    const wxArrayString paths = SelectedPaths();
    if ( paths.empty() )
    {
        Notify("Select one or more items.", wxICON_INFORMATION);
        return;
    }
    m_copiedPaths = paths;
    SetStatusText(wxString::Format("Copied %u item(s)", static_cast<unsigned>(paths.GetCount())), 0);
    UpdateCommandState();
}

void GitSumFrame::PasteClipboard()
{
    if ( m_copiedPaths.empty() )
    {
        Notify("Copy items before pasting.", wxICON_INFORMATION);
        return;
    }

    const wxString destDir = TargetDirectory();
    unsigned pasted = 0;
    for ( size_t i = 0; i < m_copiedPaths.GetCount(); ++i )
    {
        const wxString src = m_copiedPaths[i];
        const wxString name = wxFileName(src).GetFullName();
        if ( name.empty() )
            continue;
        const wxString dest = JoinPath(destDir, name);

        if ( PathsEqual(src, dest) )
        {
            Notify("\"" + name + "\" is already in this folder.");
            continue;
        }
        if ( wxFileName::DirExists(src) && IsSameOrInside(src, dest) )
        {
            Notify("Cannot paste a folder into itself.");
            break;
        }

        if ( wxFileName::Exists(dest) )
        {
            const int answer = Ask("\"" + name + "\" already exists. Overwrite it?",
                                   wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
            if ( answer != wxYES )
                continue;
            wxString error;
            if ( !RemovePath(dest, error) )
            {
                Notify(error, wxICON_ERROR);
                break;
            }
        }

        wxString error;
        if ( !CopyPath(src, dest, error) )
        {
            Notify(error, wxICON_ERROR);
            break;
        }
        ++pasted;
    }

    RefreshTree();
    RefreshPane();
    UpdateCommandState();
    if ( pasted > 0 )
        SetStatusText(wxString::Format("Pasted %u item(s)", pasted), 0);
}

bool GitSumFrame::CreateFolder(const wxString& name)
{
    wxString error;
    if ( !ValidNewName(name, error) )
    {
        Notify(error, wxICON_WARNING);
        return false;
    }
    const wxString path = JoinPath(TargetDirectory(), name);
    if ( wxFileName::Exists(path) )
    {
        Notify("\"" + name + "\" already exists.", wxICON_WARNING);
        return false;
    }
    if ( !wxFileName::Mkdir(path, wxS_DIR_DEFAULT, 0) )
    {
        Notify("Could not create \"" + path + "\".", wxICON_ERROR);
        return false;
    }
    RefreshTree();
    RefreshPane();
    UpdateCommandState();
    return true;
}

bool GitSumFrame::CreateFile(const wxString& name)
{
    wxString error;
    if ( !ValidNewName(name, error) )
    {
        Notify(error, wxICON_WARNING);
        return false;
    }
    const wxString path = JoinPath(TargetDirectory(), name);
    if ( wxFileName::Exists(path) )
    {
        Notify("\"" + name + "\" already exists.", wxICON_WARNING);
        return false;
    }
    wxFile file;
    if ( !file.Create(path, false) )
    {
        Notify("Could not create \"" + path + "\".", wxICON_ERROR);
        return false;
    }
    file.Close();
    RefreshPane();
    UpdateCommandState();
    return true;
}

bool GitSumFrame::DeleteSelection()
{
    const wxArrayString paths = SelectedPaths();
    if ( paths.empty() )
    {
        Notify("Select one or more items.", wxICON_INFORMATION);
        return false;
    }

    for ( size_t i = 0; i < paths.GetCount(); ++i )
    {
        if ( wxFileName::DirExists(paths[i]) && IsVolumeRoot(paths[i]) )
        {
            Notify("Cannot delete the root folder.", wxICON_WARNING);
            return false;
        }
    }

    wxString message;
    if ( paths.GetCount() == 1 )
        message = "Delete \"" + wxFileName(paths[0]).GetFullName() + "\"?";
    else
        message = wxString::Format("Delete %u items?", static_cast<unsigned>(paths.GetCount()));

    bool nonEmpty = false;
    for ( size_t i = 0; i < paths.GetCount(); ++i )
    {
        if ( wxFileName::DirExists(paths[i]) && !DirIsEmpty(paths[i]) )
            nonEmpty = true;
    }
    if ( nonEmpty )
        message += "\n\nFolders that are not empty will be removed with their contents.";

    if ( Ask(message, wxYES_NO | wxNO_DEFAULT | wxICON_WARNING) != wxYES )
    {
        return false;
    }

    wxString navigateAfter;
    for ( size_t i = 0; i < paths.GetCount(); ++i )
    {
        if ( PathsEqual(paths[i], m_currentDir) || IsSameOrInside(paths[i], m_currentDir) )
        {
            wxFileName parent;
            parent.AssignDir(paths[i]);
            if ( parent.GetDirCount() > 0 )
            {
                parent.RemoveLastDir();
                navigateAfter = parent.GetPath();
            }
        }
        wxString error;
        if ( !RemovePath(paths[i], error) )
        {
            Notify(error, wxICON_ERROR);
            break;
        }
    }

    if ( !navigateAfter.empty() )
        ShowDirectory(navigateAfter, false);
    RefreshTree();
    if ( navigateAfter.empty() )
        RefreshPane();
    UpdateCommandState();
    return true;
}

bool GitSumFrame::SelectName(const wxString& name)
{
    for ( size_t i = 0; i < m_entries.size(); ++i )
    {
        m_list->SetItemState(static_cast<long>(i), 0, wxLIST_STATE_SELECTED);
    }
    for ( size_t i = 0; i < m_entries.size(); ++i )
    {
        if ( m_entries[i].name == name )
        {
            m_list->SetItemState(static_cast<long>(i), wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
            UpdateCommandState();
            return true;
        }
    }
    UpdateCommandState();
    return false;
}

void GitSumFrame::ClearSelection()
{
    for ( size_t i = 0; i < m_entries.size(); ++i )
        m_list->SetItemState(static_cast<long>(i), 0, wxLIST_STATE_SELECTED);
    UpdateCommandState();
}

void GitSumFrame::PopupContextMenu()
{
    UpdateCommandState();
    wxMenu menu;
    menu.Append(ID_NEW_FOLDER, "New folder");
    menu.Append(ID_NEW_FILE, "New file");
    menu.AppendSeparator();
    menu.Append(ID_COPY_PATHS, "Copy");
    menu.Append(ID_PASTE_PATHS, "Paste");
    menu.Append(ID_DELETE_PATHS, "Delete");
    menu.AppendSeparator();
    menu.Append(ID_GIT_INIT, "Init");
    menu.AppendCheckItem(ID_GIT_STATUS, "Status");
    menu.Check(ID_GIT_STATUS, m_pane == Pane::Status);
    menu.Append(ID_GIT_ADD, "Add");
    menu.Append(ID_GIT_COMMIT, "Commit");
    menu.Append(ID_GIT_DIFF, "Diff");
    menu.AppendCheckItem(ID_GIT_LOG, "Log");
    menu.Check(ID_GIT_LOG, m_pane == Pane::Log);
    menu.Append(ID_GIT_FETCH, "Fetch");
    menu.Append(ID_GIT_PUSH, "Push");
    menu.Append(ID_GIT_PULL, "Pull");

    const int ids[] = {
        ID_NEW_FOLDER, ID_NEW_FILE, ID_COPY_PATHS, ID_PASTE_PATHS, ID_DELETE_PATHS,
        ID_GIT_INIT, ID_GIT_STATUS, ID_GIT_ADD, ID_GIT_COMMIT, ID_GIT_DIFF, ID_GIT_LOG,
        ID_GIT_FETCH, ID_GIT_PUSH, ID_GIT_PULL
    };
    for ( int id : ids )
        menu.Enable(id, GetMenuBar()->IsEnabled(id));
    PopupMenu(&menu);
}

void GitSumFrame::OnClose(wxCloseEvent& event)
{
    m_config.SetLastPath(m_currentDir);
    m_config.Save();
    event.Skip();
}

void GitSumFrame::OnAddressEnter(wxCommandEvent&)
{
    ShowDirectory(m_address->GetValue(), true);
}

void GitSumFrame::OnUp(wxCommandEvent&)
{
    GoUp();
}

void GitSumFrame::OnNewFolder(wxCommandEvent&)
{
    wxTextEntryDialog dialog(this, "Folder name:", "New folder");
    if ( dialog.ShowModal() != wxID_OK )
        return;
    CreateFolder(dialog.GetValue());
}

void GitSumFrame::OnNewFile(wxCommandEvent&)
{
    wxTextEntryDialog dialog(this, "File name:", "New file");
    if ( dialog.ShowModal() != wxID_OK )
        return;
    CreateFile(dialog.GetValue());
}

void GitSumFrame::OnCopy(wxCommandEvent&)
{
    CopySelection();
}

void GitSumFrame::OnPaste(wxCommandEvent&)
{
    PasteClipboard();
}

void GitSumFrame::OnDelete(wxCommandEvent&)
{
    DeleteSelection();
}

void GitSumFrame::OnGitStatus(wxCommandEvent& event)
{
    ShowPane(event.IsChecked() ? Pane::Status : Pane::Files);
}

void GitSumFrame::OnGitLog(wxCommandEvent& event)
{
    ShowPane(event.IsChecked() ? Pane::Log : Pane::Files);
}

void GitSumFrame::OnGitInit(wxCommandEvent&)
{
    const wxString path = GitSelectionPath();
    if ( path.empty() || !wxFileName::DirExists(path) )
    {
        Notify("Select a folder.", wxICON_INFORMATION);
        return;
    }
    if ( IsGitControlledDirectory(path) )
    {
        Notify("This folder is already a Git repository.", wxICON_INFORMATION);
        return;
    }

    git_libgit2_init();
    const wxScopedCharBuffer utf8 = path.utf8_str();
    git_repository* repo = nullptr;
    if ( git_repository_init(&repo, utf8.data(), 0) != 0 || !repo )
    {
        const git_error* error = git_error_last();
        const wxString message = error && error->message ? wxString::FromUTF8(error->message)
                                                          : wxString("Could not initialize the repository.");
        Notify(message, wxICON_ERROR);
        return;
    }
    git_repository_free(repo);
    RefreshTree();
    RefreshPane();
    UpdateCommandState();
}

void GitSumFrame::OnGitFetch(wxCommandEvent&)
{
    const wxString path = GitSelectionPath();
    if ( path.empty() || !IsGitControlledDirectory(path) )
    {
        Notify("Select a Git directory.", wxICON_INFORMATION);
        return;
    }

    const GitFetchResult result = FetchRepository(path, m_config.Credentials());
    if ( !result.ok )
    {
        Notify(result.error.empty() ? wxString("Could not fetch.") : result.error, wxICON_ERROR);
        return;
    }
    Notify(result.summary);
    if ( m_pane != Pane::Files )
        RefreshPane();
}

void GitSumFrame::OnGitStub(wxCommandEvent& event)
{
    wxString label = "Git command";
    if ( GetMenuBar() )
    {
        wxMenuItem* item = GetMenuBar()->FindItem(event.GetId());
        if ( item )
            label = item->GetItemLabelText();
    }
    Notify(label + " is not implemented.");
}

void GitSumFrame::OnAbout(wxCommandEvent&)
{
    Notify("git-sum\n\nFile browser with Git actions.\nInit creates a repository in a normal folder.\nStatus, Log, and Fetch use libgit2.\nThe other Git commands are not implemented yet.");
}

void GitSumFrame::OnDirSelected(wxTreeEvent& event)
{
    event.Skip();
    if ( m_settingPath || IsBeingDeleted() || !m_dirCtrl || !m_list )
        return;
    const wxString path = m_dirCtrl->GetPath();
    if ( path.empty() || PathsEqual(path, m_currentDir) )
        return;
    ShowDirectory(path, false);
}

void GitSumFrame::OnDirRightClick(wxTreeEvent& event)
{
    const wxString path = m_dirCtrl->GetPath(event.GetItem());
    if ( path.empty() )
        return;
    m_contextTreePath = path;
    PopupContextMenu();
    m_contextTreePath.clear();
    UpdateCommandState();
}

void GitSumFrame::OnListActivated(wxListEvent& event)
{
    if ( m_pane != Pane::Files )
        return;
    const long index = event.GetIndex();
    if ( index < 0 || static_cast<size_t>(index) >= m_entries.size() )
        return;
    if ( m_entries[static_cast<size_t>(index)].isDir )
        ShowDirectory(m_entries[static_cast<size_t>(index)].path, true);
}

void GitSumFrame::OnListSelection(wxListEvent&)
{
    if ( m_settingPath || IsBeingDeleted() )
        return;
    if ( m_pane == Pane::Log )
    {
        const long item = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        ShowCommitMessage(item);
    }
    UpdateCommandState();
}

void GitSumFrame::OnListKey(wxKeyEvent& event)
{
    const int code = event.GetKeyCode();
    if ( event.ControlDown() && !event.AltDown() && (code == 'C' || code == 'c') )
    {
        CopySelection();
        return;
    }
    if ( event.ControlDown() && !event.AltDown() && (code == 'V' || code == 'v') )
    {
        PasteClipboard();
        return;
    }
    if ( !event.ControlDown() && !event.AltDown() && code == WXK_DELETE )
    {
        DeleteSelection();
        return;
    }
    event.Skip();
}

void GitSumFrame::OnListColumn(wxListEvent& event)
{
    if ( m_pane != Pane::Status || m_settingPath || IsBeingDeleted() )
        return;
    const int column = event.GetColumn();
    if ( column < 0 || column > 2 )
        return;
    if ( column == m_statusSortColumn )
        m_statusSortAscending = !m_statusSortAscending;
    else
    {
        m_statusSortColumn = column;
        m_statusSortAscending = true;
    }
    SortStatusList();
}

void GitSumFrame::OnListContext(wxContextMenuEvent& event)
{
    const wxPoint screen = event.GetPosition() == wxDefaultPosition ? wxGetMousePosition() : event.GetPosition();
    const wxPoint client = m_list->ScreenToClient(screen);
    int flags = 0;
    const long index = m_list->HitTest(client, flags);
    if ( index >= 0 && m_list->GetItemState(index, wxLIST_STATE_SELECTED) == 0 )
    {
        ClearSelection();
        m_list->SetItemState(index, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }
    PopupContextMenu();
}

bool GitSumFrame::RunSelfTest()
{
    const wxString root = wxFileName::GetTempDir() + wxFileName::GetPathSeparator() + "git-sum-selftest";
    if ( wxFileName::DirExists(root) )
    {
        wxString removeError;
        if ( !RemoveTree(root, removeError) )
            return Fail(removeError.empty() ? wxString("could not remove old self-test directory") : removeError);
    }
    if ( !wxFileName::Mkdir(root, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL) )
        return Fail("could not create self-test directory");

    struct TempDir
    {
        wxString path;
        ~TempDir()
        {
            if ( !path.empty() )
                wxFileName::Rmdir(path, wxPATH_RMDIR_RECURSIVE);
        }
    } temp;
    temp.path = root;

    const wxString plain = JoinPath(root, "plain");
    const wxString repo = JoinPath(root, "repo");
    const wxString sub = JoinPath(repo, "sub");
    const wxString work = JoinPath(root, "work");
    const wxString note = JoinPath(plain, "note.txt");
    if ( !wxFileName::Mkdir(plain, wxS_DIR_DEFAULT, 0) ||
         !wxFileName::Mkdir(repo, wxS_DIR_DEFAULT, 0) ||
         !wxFileName::Mkdir(sub, wxS_DIR_DEFAULT, 0) ||
         !wxFileName::Mkdir(JoinPath(repo, ".git"), wxS_DIR_DEFAULT, 0) ||
         !wxFileName::Mkdir(work, wxS_DIR_DEFAULT, 0) ||
         !WriteAll(note, "hello") ||
         !WriteAll(JoinPath(work, ".git"), "gitdir: somewhere") )
        return Fail("could not build the sample tree");

#ifdef __WXMSW__
    if ( !IsVolumeRoot("C:\\") )
        return Fail("C:\\ should be a volume root");
#endif
    if ( IsVolumeRoot(plain) )
        return Fail("plain folder should not be a volume root");
    if ( IsGitControlledDirectory(plain) || IsGitControlledDirectory(note) || IsGitControlledDirectory(root) )
        return Fail("non-repo paths were treated as Git directories");
    if ( !IsGitControlledDirectory(repo) || !IsGitControlledDirectory(sub) || !IsGitControlledDirectory(work) )
        return Fail("repo, nested folder, or worktree was not detected");

    const wxString configPath = JoinPath(root, "git-sum.conf");
    {
        GitSumConfig saved(configPath);
        saved.SetLastPath(plain);
        saved.SetUserName("Ada");
        saved.SetUserEmail("ada@example.com");
        saved.SetCredentials("stored-later");
        if ( !saved.Save() )
            return Fail("config save failed");
        GitSumConfig loaded(configPath);
        if ( loaded.LastPath() != plain || loaded.UserName() != "Ada" ||
             loaded.UserEmail() != "ada@example.com" || loaded.Credentials() != "stored-later" )
            return Fail("config round-trip mismatch");
    }

    GitSumConfig config(configPath);
    config.SetLastPath(plain);
    config.SetUserName(wxString());
    config.SetUserEmail(wxString());
    config.SetCredentials(wxString());
    if ( !config.Save() )
        return Fail("could not reset config placeholders");

    struct FrameDeleter
    {
        GitSumFrame* frame;
        ~FrameDeleter() { delete frame; }
    } guard = {};
    guard.frame = new GitSumFrame(config, true);
    GitSumFrame* frame = guard.frame;

    if ( frame->CommandEnabled(ID_GIT_STATUS) || frame->m_currentDir.IsEmpty() )
        return Fail("Git commands should be disabled in a normal folder");
    if ( !frame->CommandEnabled(ID_GIT_INIT) )
        return Fail("Init should be enabled in a normal folder");
    for ( int id : kGitIds )
    {
        if ( frame->CommandEnabled(id) )
            return Fail("a Git command was enabled outside a repository");
    }

    if ( !frame->CreateFolder("alpha") || !wxFileName::DirExists(JoinPath(plain, "alpha")) )
        return Fail("new folder was not created");
    if ( frame->CreateFolder("alpha") || !frame->m_lastNotice.Contains("already exists") )
        return Fail("duplicate folder was accepted");
    if ( !frame->CreateFile("created.txt") || wxFileName::GetSize(JoinPath(plain, "created.txt")) != wxULongLong(0) )
        return Fail("new file was not created empty");
    if ( frame->CreateFile("a/b") || !frame->m_lastNotice.Contains("path separators") )
        return Fail("a path-like file name was accepted");
    if ( !frame->SelectName("alpha") || !frame->CommandEnabled(ID_GIT_INIT) || frame->CommandEnabled(ID_GIT_STATUS) )
        return Fail("Init should be enabled for a selected normal folder");
    frame->ClearSelection();

    if ( !frame->SelectName("note.txt") )
        return Fail("could not select note.txt");
    if ( frame->CommandEnabled(ID_GIT_STATUS) || frame->CommandEnabled(ID_GIT_INIT) )
        return Fail("Git commands should be disabled when a file is selected");
    if ( !frame->CommandEnabled(ID_COPY_PATHS) )
        return Fail("Copy should be enabled when an item is selected");

    frame->CopySelection();
    if ( !frame->CommandEnabled(ID_PASTE_PATHS) )
        return Fail("Paste should be enabled after Copy");

    frame->ShowDirectory(repo, true);
    frame->ClearSelection();
    if ( !frame->CommandEnabled(ID_GIT_STATUS) || !frame->CommandEnabled(ID_GIT_PULL) )
        return Fail("Git commands should be enabled in a repository");
    if ( frame->CommandEnabled(ID_GIT_INIT) )
        return Fail("Init should be disabled in a repository");

    frame->PasteClipboard();
    if ( ReadAll(JoinPath(repo, "note.txt")) != "hello" )
        return Fail("paste did not copy note.txt");

    if ( !WriteAll(JoinPath(repo, "note.txt"), "changed") )
        return Fail("could not modify pasted file");
    frame->m_scriptedAnswer = wxNO;
    frame->PasteClipboard();
    if ( ReadAll(JoinPath(repo, "note.txt")) != "changed" )
        return Fail("declining overwrite changed the file");
    frame->m_scriptedAnswer = wxYES;
    frame->PasteClipboard();
    if ( ReadAll(JoinPath(repo, "note.txt")) != "hello" )
        return Fail("overwrite did not restore note.txt");
    frame->m_scriptedAnswer = 0;

    if ( !frame->SelectName("note.txt") || frame->CommandEnabled(ID_GIT_COMMIT) )
        return Fail("selecting a file inside a repository should disable Git commands");
    frame->ClearSelection();
    if ( !frame->CommandEnabled(ID_GIT_LOG) )
        return Fail("clearing the selection should enable Git commands for the repository");
    if ( !frame->SelectName("sub") || !frame->CommandEnabled(ID_GIT_DIFF) )
        return Fail("a repository subfolder should enable Git commands");

    frame->ShowDirectory(sub, true);
    frame->ClearSelection();
    if ( !frame->CommandEnabled(ID_GIT_ADD) )
        return Fail("a nested repository folder should enable Git commands");

    frame->ShowDirectory(repo, true);
    if ( !frame->CreateFolder("box") )
        return Fail("could not create box");
    frame->ShowDirectory(JoinPath(repo, "box"), true);
    if ( !frame->CreateFile("inside.txt") )
        return Fail("could not create inside.txt");
    frame->ShowDirectory(repo, true);
    if ( !frame->SelectName("box") )
        return Fail("could not select box");
    frame->CopySelection();
    frame->ShowDirectory(JoinPath(repo, "box"), true);
    frame->m_lastNotice.clear();
    frame->PasteClipboard();
    if ( !frame->m_lastNotice.Contains("into itself") ||
         wxFileName::DirExists(JoinPath(JoinPath(repo, "box"), "box")) )
        return Fail("pasting a folder into itself was allowed");

    frame->ShowDirectory(repo, true);
    if ( !frame->SelectName("box") )
        return Fail("could not select box again");
    frame->CopySelection();
    frame->m_lastNotice.clear();
    frame->PasteClipboard();
    if ( !frame->m_lastNotice.Contains("already in this folder") )
        return Fail("pasting an item onto itself was not reported");

    if ( !frame->SelectName("note.txt") )
        return Fail("could not select note.txt for delete");
    frame->m_scriptedAnswer = wxYES;
    if ( !frame->DeleteSelection() || wxFileName::FileExists(JoinPath(repo, "note.txt")) )
        return Fail("delete did not remove note.txt");
    if ( !frame->SelectName("box") )
        return Fail("could not select box for delete");
    if ( !frame->DeleteSelection() || wxFileName::DirExists(JoinPath(repo, "box")) )
        return Fail("delete did not remove the non-empty folder");
    frame->m_scriptedAnswer = wxNO;
    if ( !frame->CreateFile("keep.txt") )
        return Fail("could not create keep.txt");
    if ( !frame->SelectName("keep.txt") )
        return Fail("could not select keep.txt");
    if ( frame->DeleteSelection() || !wxFileName::FileExists(JoinPath(repo, "keep.txt")) )
        return Fail("declining delete removed the file");
    frame->m_scriptedAnswer = 0;

    frame->ShowDirectory(work, true);
    frame->ClearSelection();
    if ( !frame->CommandEnabled(ID_GIT_STATUS) )
        return Fail("a worktree .git file should enable Git commands");
    frame->GoUp();
    if ( !PathsEqual(frame->m_currentDir, root) || frame->CommandEnabled(ID_GIT_PUSH) )
        return Fail("Up should leave the worktree and disable Git commands");

    wxCommandEvent stub(wxEVT_MENU, ID_GIT_ADD);
    frame->ShowDirectory(repo, true);
    frame->ClearSelection();
    frame->m_lastNotice.clear();
    frame->OnGitStub(stub);
    if ( !frame->m_lastNotice.Contains("not implemented") )
        return Fail("Git add did not report that it is not implemented");

    const wxString fresh = JoinPath(root, "fresh");
    if ( !wxFileName::Mkdir(fresh) )
        return Fail("could not create a folder to initialize");
    frame->ShowDirectory(fresh, true);
    frame->ClearSelection();
    if ( !frame->CommandEnabled(ID_GIT_INIT) || frame->CommandEnabled(ID_GIT_STATUS) )
        return Fail("Init should be the available Git command outside a repository");
    wxCommandEvent initEvent(wxEVT_MENU, ID_GIT_INIT);
    frame->OnGitInit(initEvent);
    if ( !wxFileName::DirExists(JoinPath(fresh, ".git")) || !IsGitControlledDirectory(fresh) )
        return Fail("Init did not create a repository");
    if ( frame->CommandEnabled(ID_GIT_INIT) || !frame->CommandEnabled(ID_GIT_STATUS) )
        return Fail("Init left the Git commands unchanged");
    const GitStatusResult created = QueryGitStatus(fresh);
    if ( !created.ok || !created.rows.empty() || !created.branch.Contains("No commits yet") )
        return Fail("Init did not create an empty repository");
    const GitFetchResult noRemotes = FetchRepository(fresh, wxString());
    if ( noRemotes.ok || !noRemotes.error.Contains("no remotes") )
        return Fail("fetch should report a repository with no remotes");

    const wxString fetchSource = JoinPath(root, "fetch-src");
    const wxString fetchDest = JoinPath(root, "fetch-dst");
    git_repository* sourceRepo = nullptr;
    git_repository* destRepo = nullptr;
    const wxScopedCharBuffer sourceUtf8 = fetchSource.utf8_str();
    const wxScopedCharBuffer destUtf8 = fetchDest.utf8_str();
    if ( git_repository_init(&sourceRepo, sourceUtf8.data(), 0) != 0 || !sourceRepo ||
         git_repository_init(&destRepo, destUtf8.data(), 0) != 0 || !destRepo )
    {
        if ( sourceRepo )
            git_repository_free(sourceRepo);
        if ( destRepo )
            git_repository_free(destRepo);
        return Fail("could not create repositories for fetch");
    }
    git_repository_free(sourceRepo);
    git_repository_free(destRepo);
    if ( !WriteAll(JoinPath(fetchSource, "hello.txt"), "hello") )
        return Fail("could not write the fetch source file");
    wxString fetchCommitError;
    if ( !CommitFile(fetchSource, "hello.txt", "Fetched commit", fetchCommitError) )
        return Fail(fetchCommitError);
    wxString remoteUrl = fetchSource;
    remoteUrl.Replace("\\", "/");
    const wxScopedCharBuffer remoteUtf8 = remoteUrl.utf8_str();
    git_repository* destOpen = nullptr;
    git_remote* origin = nullptr;
    if ( git_repository_open(&destOpen, destUtf8.data()) != 0 || !destOpen ||
         git_remote_create(&origin, destOpen, "origin", remoteUtf8.data()) != 0 || !origin )
    {
        if ( origin )
            git_remote_free(origin);
        if ( destOpen )
            git_repository_free(destOpen);
        return Fail("could not add a remote for fetch");
    }
    git_remote_free(origin);
    git_repository_free(destOpen);
    frame->ShowDirectory(fetchDest, true);
    frame->ClearSelection();
    if ( !frame->CommandEnabled(ID_GIT_FETCH) )
        return Fail("Fetch should be enabled in a repository");
    wxCommandEvent fetchEvent(wxEVT_MENU, ID_GIT_FETCH);
    frame->m_lastNotice.clear();
    frame->OnGitFetch(fetchEvent);
    if ( !frame->m_lastNotice.Contains("Fetched origin") )
        return Fail(frame->m_lastNotice.empty() ? wxString("fetch did not report success") : frame->m_lastNotice);
    git_repository* fetched = nullptr;
    git_reference* head = nullptr;
    git_reference* tracking = nullptr;
    wxString trackingRef = "refs/remotes/origin/master";
    if ( git_repository_open(&fetched, sourceUtf8.data()) == 0 && fetched &&
         git_repository_head(&head, fetched) == 0 && head && git_reference_shorthand(head) )
        trackingRef = "refs/remotes/origin/" + wxString::FromUTF8(git_reference_shorthand(head));
    if ( head )
        git_reference_free(head);
    if ( fetched )
        git_repository_free(fetched);
    fetched = nullptr;
    const wxScopedCharBuffer trackingUtf8 = trackingRef.utf8_str();
    if ( git_repository_open(&fetched, destUtf8.data()) != 0 || !fetched ||
         git_reference_lookup(&tracking, fetched, trackingUtf8.data()) != 0 || !tracking )
    {
        if ( tracking )
            git_reference_free(tracking);
        if ( fetched )
            git_repository_free(fetched);
        return Fail("fetch did not create the remote-tracking branch");
    }
    git_reference_free(tracking);
    git_repository_free(fetched);

    const wxString live = JoinPath(root, "live");
    git_libgit2_init();
    git_repository* liveRepo = nullptr;
    const wxScopedCharBuffer liveUtf8 = live.utf8_str();
    if ( git_repository_init(&liveRepo, liveUtf8.data(), 0) != 0 || !liveRepo )
        return Fail("could not create a repository for status");
    git_repository_free(liveRepo);
    if ( !WriteAll(JoinPath(live, "hello.txt"), "hello") )
        return Fail("could not write hello.txt");
    const GitStatusResult untracked = QueryGitStatus(live);
    if ( !untracked.ok )
        return Fail(untracked.error.empty() ? wxString("status failed on a new repository") : untracked.error);
    bool sawUntracked = false;
    for ( size_t i = 0; i < untracked.rows.size(); ++i )
    {
        if ( untracked.rows[i].path == "hello.txt" && untracked.rows[i].workTree.Contains("untracked") )
            sawUntracked = true;
    }
    if ( !sawUntracked )
        return Fail("status did not report hello.txt as untracked");
    if ( QueryGitStatus(plain).ok )
        return Fail("status succeeded outside a repository");
    if ( QueryGitLog(plain).ok )
        return Fail("log succeeded outside a repository");

    frame->ShowDirectory(live, true);
    frame->ClearSelection();
    wxCommandEvent statusOn(wxEVT_MENU, ID_GIT_STATUS);
    statusOn.SetInt(1);
    frame->OnGitStatus(statusOn);
    if ( frame->m_pane != Pane::Status || !frame->GetMenuBar()->IsChecked(ID_GIT_STATUS) ||
         !frame->GetToolBar()->GetToolState(ID_GIT_STATUS) )
        return Fail("status toggle did not show the status view");
    if ( frame->m_commitMessage->IsShown() )
        return Fail("the commit message should be hidden in the status view");
    if ( frame->GetMenuBar()->IsChecked(ID_GIT_LOG) )
        return Fail("log was checked while status is showing");
    sawUntracked = false;
    for ( long row = 0; row < frame->m_list->GetItemCount(); ++row )
    {
        if ( frame->m_list->GetItemText(row, 0) == "hello.txt" &&
             frame->m_list->GetItemText(row, 2).Contains("untracked") )
            sawUntracked = true;
    }
    if ( !sawUntracked )
        return Fail("status view did not list hello.txt as untracked");
    if ( !frame->m_summary->IsShown() || !frame->m_summary->GetLabel().Contains("No commits yet") )
        return Fail("status summary did not describe the new repository");

    if ( !WriteAll(JoinPath(live, "zebra.txt"), "z") )
        return Fail("could not write zebra.txt");
    frame->RefreshPane();
    if ( frame->m_list->GetItemCount() != 2 ||
         frame->m_list->GetItemText(0, 0) != "hello.txt" ||
         frame->m_list->GetItemText(1, 0) != "zebra.txt" )
        return Fail("status was not sorted by path");
    wxListEvent pathColumn(wxEVT_LIST_COL_CLICK);
    pathColumn.SetColumn(0);
    frame->OnListColumn(pathColumn);
    if ( frame->m_list->GetItemText(0, 0) != "zebra.txt" ||
         frame->m_list->GetItemText(1, 0) != "hello.txt" )
        return Fail("status path column did not sort descending");
    wxListEvent workTreeColumn(wxEVT_LIST_COL_CLICK);
    workTreeColumn.SetColumn(2);
    frame->OnListColumn(workTreeColumn);
    if ( frame->m_list->GetItemText(0, 0) != "hello.txt" ||
         frame->m_list->GetItemText(1, 0) != "zebra.txt" )
        return Fail("status work tree column did not sort");
    if ( !wxRemoveFile(JoinPath(live, "zebra.txt")) )
        return Fail("could not remove zebra.txt");
    frame->m_statusSortColumn = 0;
    frame->m_statusSortAscending = true;

    wxString commitError;
    if ( !CommitFile(live, "hello.txt", "Initial commit", commitError) )
        return Fail(commitError);
    frame->RefreshPane();
    if ( frame->m_list->GetItemCount() != 0 ||
         !frame->m_summary->GetLabel().Contains("nothing to commit") )
        return Fail("status view did not become clean after the commit");

    const wxString liveSub = JoinPath(live, "sub");
    if ( !wxFileName::Mkdir(liveSub) || !WriteAll(JoinPath(liveSub, "other.txt"), "other") )
        return Fail("could not write sub/other.txt");
    frame->ShowDirectory(liveSub, true);
    if ( frame->m_pane != Pane::Status )
        return Fail("status should stay on inside a repository folder");
    bool sawOther = false;
    bool sawHello = false;
    for ( long row = 0; row < frame->m_list->GetItemCount(); ++row )
    {
        const wxString path = frame->m_list->GetItemText(row, 0);
        if ( path.Contains("other.txt") )
            sawOther = true;
        if ( path.Contains("hello.txt") )
            sawHello = true;
    }
    if ( !sawOther || sawHello || !frame->m_summary->GetLabel().Contains("sub") )
        return Fail("status did not follow the selected folder");

    if ( !CommitFile(live, "sub/other.txt", "Nested file\n\nThe folder follows the selection.\n", commitError) )
        return Fail(commitError);

    wxCommandEvent logOn(wxEVT_MENU, ID_GIT_LOG);
    logOn.SetInt(1);
    frame->OnGitLog(logOn);
    if ( frame->m_pane != Pane::Log || !frame->GetMenuBar()->IsChecked(ID_GIT_LOG) ||
         frame->GetMenuBar()->IsChecked(ID_GIT_STATUS) || !frame->GetToolBar()->GetToolState(ID_GIT_LOG) )
        return Fail("log toggle did not replace the status view");
    if ( frame->m_list->GetItemCount() != 1 || frame->m_list->GetItemText(0, 3) != "Nested file" )
        return Fail("log did not follow the selected folder");
    if ( !frame->m_commitMessage->IsShown() ||
         !frame->m_commitMessage->GetValue().Contains("The folder follows the selection.") )
        return Fail("log did not show the commit message");

    frame->ShowDirectory(live, true);
    if ( frame->m_pane != Pane::Log )
        return Fail("log should stay on at the repository root");
    const GitLogResult history = QueryGitLog(live);
    if ( !history.ok || history.rows.size() != 2 || history.rows[0].summary != "Nested file" ||
         history.rows[1].summary != "Initial commit" || history.rows[0].id.length() != 7 ||
         !history.rows[0].author.Contains("Ada") || !history.scope.empty() )
        return Fail("log did not return the repository history");
    if ( frame->m_list->GetItemCount() != 2 ||
         frame->m_list->GetItemText(0, 3) != "Nested file" ||
         frame->m_list->GetItemText(1, 3) != "Initial commit" )
        return Fail("log view did not follow the repository root");
    if ( !frame->m_commitMessage->GetValue().Contains("The folder follows the selection.") )
        return Fail("the newest commit message was not shown");
    frame->m_list->SetItemState(0, 0, wxLIST_STATE_SELECTED);
    frame->m_list->SetItemState(1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    if ( !frame->m_commitMessage->GetValue().Contains("Initial commit") ||
         frame->m_commitMessage->GetValue().Contains("The folder follows the selection.") )
        return Fail("selecting a commit did not show its message");

    const GitLogResult nestedHistory = QueryGitLog(liveSub);
    if ( !nestedHistory.ok || nestedHistory.rows.size() != 1 ||
         nestedHistory.rows[0].summary != "Nested file" || nestedHistory.scope != "sub" )
        return Fail("log query did not follow the folder");

    wxCommandEvent logOff(wxEVT_MENU, ID_GIT_LOG);
    logOff.SetInt(0);
    frame->OnGitLog(logOff);
    if ( frame->m_pane != Pane::Files || frame->GetMenuBar()->IsChecked(ID_GIT_LOG) ||
         frame->m_list->GetColumnCount() != 3 || frame->m_commitMessage->IsShown() )
        return Fail("log toggle did not restore the file list");
    if ( !frame->SelectName("hello.txt") )
        return Fail("file list did not show hello.txt after log");

    frame->ClearSelection();
    frame->OnGitStatus(statusOn);
    frame->ShowDirectory(root, true);
    if ( frame->m_pane != Pane::Files || frame->GetMenuBar()->IsChecked(ID_GIT_STATUS) )
        return Fail("leaving a repository should restore the file list");
    frame->ShowDirectory(repo, true);

    GitSumConfig check(configPath);
    if ( !PathsEqual(check.LastPath(), repo) )
        return Fail("last path was not saved");
    if ( !check.UserName().empty() || !check.UserEmail().empty() || !check.Credentials().empty() )
        return Fail("placeholder credential keys were not kept");

    // Leave the sample tree before deleting it. The directory control and the
    // process working directory keep the folder open, and Windows will not
    // remove that tree until both have moved elsewhere.
    const wxString outside = wxPathOnly(wxStandardPaths::Get().GetExecutablePath());
    wxSetWorkingDirectory(outside);
    GitSumFrame* closing = guard.frame;
    guard.frame = nullptr;
    closing->Close(true);
    wxYield();
    wxString removeError;
    if ( !RemoveTree(temp.path, removeError) || wxFileName::DirExists(temp.path) )
        return Fail(removeError.empty() ? wxString("could not remove self-test directory") : removeError);
    temp.path.clear();

    std::printf("git-sum self-test passed\n");
    return true;
}
