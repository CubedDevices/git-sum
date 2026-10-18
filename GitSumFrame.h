/////////////////////////////////////////////////////////////////////////////
// Name:        GitSumFrame.h
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
#ifndef GITSUMFRAME_H
#define GITSUMFRAME_H

#include "GitSumConfig.h"

#include "wx/arrstr.h"
#include "wx/datetime.h"
#include "wx/frame.h"
#include "wx/longlong.h"

#include <vector>

class wxGenericDirCtrl;
class wxListCtrl;
class wxPanel;
class wxStaticText;
class wxTextCtrl;
class wxTreeEvent;
class wxListEvent;
class wxKeyEvent;
class wxCommandEvent;
class wxCloseEvent;
class wxContextMenuEvent;

/**
 * @brief Main window: directory tree, file list, and Git views.
 */
class GitSumFrame : public wxFrame
{
public:
    /**
     * @param config Configuration to copy. The frame saves the last path into it.
     * @param silentNotices When true, message boxes are recorded and not shown.
     */
    explicit GitSumFrame(const GitSumConfig& config, bool silentNotices = false);
    ~GitSumFrame() override;

    /// Exercise navigation, file operations, config keys, and Git enablement.
    static bool RunSelfTest();

    enum
    {
        ID_ADDRESS = wxID_HIGHEST + 1,
        ID_UP,
        ID_NEW_FOLDER,
        ID_NEW_FILE,
        ID_COPY_PATHS,
        ID_PASTE_PATHS,
        ID_DELETE_PATHS,
        ID_GIT_STATUS,
        ID_GIT_ADD,
        ID_GIT_COMMIT,
        ID_GIT_DIFF,
        ID_GIT_LOG,
        ID_GIT_PUSH,
        ID_GIT_PULL,
        ID_GIT_FETCH,
        ID_GIT_INIT
    };

private:
    struct Entry
    {
        wxString name;
        wxString path;
        bool isDir;
        wxULongLong size;
        wxDateTime modified;
    };

    void BuildMenus();
    void BuildToolBar();
    void BuildBody();
    void BindDirEvents();

    enum class Pane
    {
        Files,
        Status,
        Log
    };

    void ShowDirectory(const wxString& path, bool updateTree);
    void ShowDrives();
    void ShowPane(Pane pane);
    void RefreshPane();
    void SyncToggles();
    void SetupColumns();
    void SetSummary(const wxString& text);
    void FillList();
    void FillStatus();
    void SortStatusList();
    void FillLog();
    void ShowCommitMessage(long index);
    void ShowCommitFiles(long index);
    void RefreshTree();
    void GoUp();
    void UpdateCommandState();

    void CopySelection();
    void PasteClipboard();
    bool CreateFolder(const wxString& name);
    bool CreateFile(const wxString& name);
    bool DeleteSelection();

    bool SelectName(const wxString& name);
    void ClearSelection();

    wxArrayString SelectedPaths() const;
    long ListIndexAt(const wxPoint& screen) const;
    wxString TargetDirectory() const;
    wxString GitSelectionPath() const;
    wxString GitLogPath() const;
    bool CommandEnabled(int id) const;

    void Notify(const wxString& message, int icon = wxICON_INFORMATION);
    int Ask(const wxString& message, int style);

    void PopupContextMenu();

    void OnClose(wxCloseEvent& event);
    void OnAddressEnter(wxCommandEvent& event);
    void OnUp(wxCommandEvent& event);
    void OnNewFolder(wxCommandEvent& event);
    void OnNewFile(wxCommandEvent& event);
    void OnCopy(wxCommandEvent& event);
    void OnPaste(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void OnGitStatus(wxCommandEvent& event);
    void OnGitLog(wxCommandEvent& event);
    void OnGitInit(wxCommandEvent& event);
    void OnGitFetch(wxCommandEvent& event);
    void OnGitStub(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnDirSelected(wxTreeEvent& event);
    void OnDirRightClick(wxTreeEvent& event);
    void OnListActivated(wxListEvent& event);
    void OnListSelection(wxListEvent& event);
    void OnListKey(wxKeyEvent& event);
    void OnListContext(wxContextMenuEvent& event);
    void OnListColumn(wxListEvent& event);

    GitSumConfig m_config;
    wxGenericDirCtrl* m_dirCtrl;
    wxPanel* m_right;
    wxStaticText* m_summary;
    wxListCtrl* m_commitFiles;
    wxTextCtrl* m_commitMessage;
    wxListCtrl* m_list;
    wxTextCtrl* m_address;
    Pane m_pane;
    Pane m_columns;
    int m_statusSortColumn;
    bool m_statusSortAscending;
    wxString m_currentDir;
    bool m_drivesView;
    wxArrayString m_copiedPaths;
    std::vector<Entry> m_entries;
    std::vector<wxString> m_logMessages;
    std::vector<wxString> m_logIds;
    wxString m_logTarget;
    wxString m_contextTreePath;
    bool m_settingPath;
    bool m_silentNotices;
    int m_scriptedAnswer;
    wxString m_lastNotice;
};

#endif // GITSUMFRAME_H
