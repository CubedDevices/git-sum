/////////////////////////////////////////////////////////////////////////////
// Name:        GitSumConfig.cpp
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
#include "GitSumConfig.h"

#include "wx/fileconf.h"
#include "wx/filename.h"
#include "wx/stdpaths.h"

GitSumConfig::GitSumConfig(const wxString& filePath)
    : m_filePath(filePath.empty() ? DefaultPath() : filePath)
{
    Load();
}

wxString GitSumConfig::DefaultPath()
{
    return wxFileName(wxStandardPaths::Get().GetUserConfigDir(), "git-sum.conf").GetFullPath();
}

void GitSumConfig::Load()
{
    wxFileConfig cfg(wxEmptyString, wxEmptyString, m_filePath, wxEmptyString,
                     wxCONFIG_USE_LOCAL_FILE);
    m_lastPath = cfg.Read("/Browser/LastPath", wxString());
    m_userName = cfg.Read("/Git/user.name", wxString());
    m_userEmail = cfg.Read("/Git/user.email", wxString());
    m_credentials = cfg.Read("/Git/credentials", wxString());
}

bool GitSumConfig::Save() const
{
    wxFileName file(m_filePath);
    if ( !file.GetPath().empty() )
        wxFileName::Mkdir(file.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);

    wxFileConfig cfg(wxEmptyString, wxEmptyString, m_filePath, wxEmptyString,
                     wxCONFIG_USE_LOCAL_FILE);
    cfg.Write("/Browser/LastPath", m_lastPath);
    cfg.Write("/Git/user.name", m_userName);
    cfg.Write("/Git/user.email", m_userEmail);
    cfg.Write("/Git/credentials", m_credentials);
    return cfg.Flush();
}
