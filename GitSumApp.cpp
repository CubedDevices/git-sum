/////////////////////////////////////////////////////////////////////////////
// Name:        GitSumApp.cpp
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
#include "GitSumApp.h"

#include "GitSumConfig.h"
#include "GitSumFrame.h"

#include "wx/cmdline.h"

#ifdef __WXMSW__
#include <stdio.h>
#include <windows.h>
#endif

wxIMPLEMENT_APP(GitSumApp);

bool GitSumApp::OnInit()
{
    if ( !wxApp::OnInit() )
        return false;

    SetAppName("git-sum");

    for ( int i = 1; i < argc; ++i )
    {
        if ( wxString(argv[i]) == "--self-test" )
            m_selfTest = true;
    }

    if ( m_selfTest )
    {
#ifdef __WXMSW__
        if ( AttachConsole(ATTACH_PARENT_PROCESS) )
        {
            FILE* unused = nullptr;
            freopen_s(&unused, "CONOUT$", "w", stdout);
            freopen_s(&unused, "CONOUT$", "w", stderr);
        }
#endif
        SetExitOnFrameDelete(false);
        m_selfTestCode = GitSumFrame::RunSelfTest() ? 0 : 1;
        return true;
    }

    GitSumConfig config;
    GitSumFrame* frame = new GitSumFrame(config);
    frame->Show(true);
    return true;
}

void GitSumApp::OnInitCmdLine(wxCmdLineParser& parser)
{
    wxApp::OnInitCmdLine(parser);
    parser.AddLongSwitch("self-test", "Exercise file and Git command behavior, then exit.");
}

bool GitSumApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
    m_selfTest = parser.Found("self-test");
    return wxApp::OnCmdLineParsed(parser);
}

int GitSumApp::OnRun()
{
    if ( m_selfTest )
        return m_selfTestCode;
    return wxApp::OnRun();
}
