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
#include "wx/debug.h"
#include "wx/filefn.h"

#ifdef __WXMSW__
#include <stdio.h>
#include <windows.h>
#endif

namespace
{

wxString g_assertText;
int g_assertCount = 0;

wxString AssertPath()
{
#ifdef __WXMSW__
    wchar_t temp[MAX_PATH] = { 0 };
    const DWORD n = ::GetTempPathW(MAX_PATH, temp);
    if ( n > 0 && n < MAX_PATH )
        return wxString(temp) + "git-sum-selftest-assert.txt";
#endif
    return "git-sum-selftest-assert.txt";
}

void SelfTestAssertHandler(const wxString& file,
                           int line,
                           const wxString& func,
                           const wxString& cond,
                           const wxString& msg)
{
    if ( g_assertCount >= 30 )
    {
        ++g_assertCount;
        return;
    }
    ++g_assertCount;
    const wxString text = wxString::Format("%s(%d): %s: assert (%s) failed: %s\n",
                                           file, line, func, cond, msg);
    g_assertText += text;
    FILE* out = nullptr;
#ifdef __WXMSW__
    _wfopen_s(&out, AssertPath().wc_str(), L"a");
#else
    out = fopen(AssertPath().utf8_str(), "a");
#endif
    if ( out )
    {
        const wxScopedCharBuffer utf8 = text.utf8_str();
        std::fputs(utf8.data() ? utf8.data() : "", out);
        std::fclose(out);
    }
}

} // namespace

void InstallSelfTestAssertHandler()
{
    g_assertText.clear();
    g_assertCount = 0;
#ifdef __WXMSW__
    _wremove(AssertPath().wc_str());
#else
    remove(AssertPath().mb_str());
#endif
    wxSetAssertHandler(SelfTestAssertHandler);
}

bool SelfTestHadAssert(wxString& text)
{
    text = g_assertText;
    if ( g_assertCount > 30 )
        text += wxString::Format("... %d further asserts omitted\n", g_assertCount - 30);
    return g_assertCount > 0;
}

wxIMPLEMENT_APP(GitSumApp);

bool GitSumApp::OnInit()
{
    for ( int i = 1; i < argc; ++i )
    {
        if ( wxString(argv[i]) == "--self-test" )
            m_selfTest = true;
    }
    if ( m_selfTest )
        InstallSelfTestAssertHandler();

    if ( !wxApp::OnInit() )
        return false;

    SetAppName("git-sum");

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
        const bool passed = GitSumFrame::RunSelfTest();
        wxString asserts;
        if ( SelfTestHadAssert(asserts) )
        {
            const wxScopedCharBuffer buf = ("git-sum self-test failed: wx assert\n" + asserts).utf8_str();
            std::fprintf(stderr, "%s", buf.data() ? buf.data() : "");
            m_selfTestCode = 1;
        }
        else
            m_selfTestCode = passed ? 0 : 1;
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
