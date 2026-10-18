/////////////////////////////////////////////////////////////////////////////
// Name:        GitSumApp.h
// Purpose:     git-sum application entry
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
#ifndef GITSUMAPP_H
#define GITSUMAPP_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
#endif

class wxString;

/// Route wx asserts to a temp file during --self-test. No dialog is shown.
void InstallSelfTestAssertHandler();

/// @c true when a wx assert fired since InstallSelfTestAssertHandler().
bool SelfTestHadAssert(wxString& text);

/**
 * @brief wxWidgets application object for the git-sum file browser.
 */
class wxCmdLineParser;

class GitSumApp : public wxApp
{
public:
    bool OnInit() override;
    int OnRun() override;
    void OnInitCmdLine(wxCmdLineParser& parser) override;
    bool OnCmdLineParsed(wxCmdLineParser& parser) override;

private:
    bool m_selfTest = false;
    int m_selfTestCode = 1;
};

wxDECLARE_APP(GitSumApp);

#endif // GITSUMAPP_H
