/////////////////////////////////////////////////////////////////////////////
// Name:        GitSumConfig.h
// Purpose:     Local configuration file for git-sum
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
#ifndef GITSUMCONFIG_H
#define GITSUMCONFIG_H

#include "wx/string.h"

/**
 * @brief Local git-sum.conf stored in the user configuration directory.
 *
 * Last browsed path is used by the file browser. The Git keys are placeholders
 * for the later libgit2 work and are kept empty until then.
 */
class GitSumConfig
{
public:
    /// @param filePath Absolute path of the config file. Empty uses the default.
    explicit GitSumConfig(const wxString& filePath = wxString());

    /// Read keys from disk. Missing keys stay empty.
    void Load();

    /// Write every key, including empty placeholders.
    bool Save() const;

    /// Default location: user config directory / git-sum.conf.
    static wxString DefaultPath();

    const wxString& FilePath() const { return m_filePath; }
    const wxString& LastPath() const { return m_lastPath; }
    const wxString& UserName() const { return m_userName; }
    const wxString& UserEmail() const { return m_userEmail; }
    const wxString& Credentials() const { return m_credentials; }

    void SetLastPath(const wxString& path) { m_lastPath = path; }
    void SetUserName(const wxString& name) { m_userName = name; }
    void SetUserEmail(const wxString& email) { m_userEmail = email; }
    void SetCredentials(const wxString& credentials) { m_credentials = credentials; }

private:
    wxString m_filePath;
    wxString m_lastPath;
    wxString m_userName;
    wxString m_userEmail;
    wxString m_credentials;
};

#endif // GITSUMCONFIG_H
