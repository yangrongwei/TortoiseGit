// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2003-2008 - TortoiseSVN
// Copyright (C) 2008-2012 - TortoiseGit

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#include "StdAfx.h"
#include "GitDiff.h"
#include "AppUtils.h"
#include "git.h"
#include "gittype.h"
#include "resource.h"
#include "MessageBox.h"
#include "FileDiffDlg.h"
#include "SubmoduleDiffDlg.h"

CGitDiff::CGitDiff(void)
{
}

CGitDiff::~CGitDiff(void)
{
}
int CGitDiff::SubmoduleDiffNull(CTGitPath *pPath, git_revnum_t &rev1)
{
	CString oldhash = GIT_REV_ZERO;
	CString oldsub ;
	CString newsub;
	CString newhash;

	CString cmd;
	if (rev1 != GIT_REV_ZERO)
		cmd.Format(_T("git.exe ls-tree \"%s\" -- \"%s\""), rev1, pPath->GetGitPathString());
	else
		cmd.Format(_T("git.exe ls-files -s -- \"%s\""), pPath->GetGitPathString());

	CString output, err;
	if (g_Git.Run(cmd, &output, &err, CP_UTF8))
	{
		CMessageBox::Show(NULL, output + L"\n" + err, _T("TortoiseGit"), MB_OK|MB_ICONERROR);
		return -1;
	}

	int start=0;
	start=output.Find(_T(' '),start);
	if(start>0)
	{
		if (rev1 != GIT_REV_ZERO) // in ls-files the hash is in the second column; in ls-tree it's in the third one
			start = output.Find(_T(' '), start + 1);
		if(start>0)
			newhash=output.Mid(start+1, 40);

		CGit subgit;
		subgit.m_CurrentDir=g_Git.m_CurrentDir+_T("\\")+pPath->GetWinPathString();
		int encode=CAppUtils::GetLogOutputEncode(&subgit);

		cmd.Format(_T("git.exe log -n1  --pretty=format:\"%%s\" %s"),newhash);
		bool toOK = !subgit.Run(cmd,&newsub,encode);

		bool dirty = false;
		if (rev1 == GIT_REV_ZERO)
		{
			CString dirtyList;
			subgit.Run(_T("git.exe status --porcelain"), &dirtyList, encode);
			dirty = !dirtyList.IsEmpty();
		}

		CSubmoduleDiffDlg submoduleDiffDlg;
		submoduleDiffDlg.SetDiff(pPath->GetWinPath(), false, oldhash, oldsub, true, newhash, newsub, toOK, dirty, CSubmoduleDiffDlg::NewSubmodule);
		submoduleDiffDlg.DoModal();

		return 0;
	}

	if (rev1 != GIT_REV_ZERO)
		CMessageBox::Show(NULL, _T("ls-tree output format error"), _T("TortoiseGit"), MB_OK | MB_ICONERROR);
	else
		CMessageBox::Show(NULL, _T("ls-files output format error"), _T("TortoiseGit"), MB_OK | MB_ICONERROR);
	return -1;
}

int CGitDiff::DiffNull(CTGitPath *pPath, git_revnum_t rev1,bool bIsAdd)
{
	CString temppath;
	GetTempPath(temppath);
	rev1 = g_Git.GetHash(rev1); // make sure we have a HASH here, otherwise filenames might be invalid
	CString file1;
	CString nullfile;
	CString cmd;

	if(pPath->IsDirectory())
	{
		git_revnum_t rev2;
		return SubmoduleDiffNull(pPath,rev1);
	}

	if(rev1 != GIT_REV_ZERO )
	{
		TCHAR szTempName[MAX_PATH];
		GetTempFileName(temppath, pPath->GetBaseFilename(), 0, szTempName);
		CString temp(szTempName);
		DeleteFile(szTempName);
		CreateDirectory(szTempName, NULL);
		file1.Format(_T("%s\\%s-%s%s"),
				temp,
				pPath->GetBaseFilename(),
				rev1.Left(g_Git.GetShortHASHLength()),
				pPath->GetFileExtension());

		g_Git.GetOneFile(rev1,*pPath,file1);
	}
	else
	{
		file1=g_Git.m_CurrentDir+_T("\\")+pPath->GetWinPathString();
	}

	// preserve FileExtension, needed especially for diffing deleted images (detection on new filename extension)
	CString tempfile=::GetTempFile() + pPath->GetFileExtension();
	CStdioFile file(tempfile,CFile::modeReadWrite|CFile::modeCreate );
	//file.WriteString();
	file.Close();
	::SetFileAttributes(tempfile, FILE_ATTRIBUTE_READONLY);

	CAppUtils::DiffFlags flags;

	if(bIsAdd)
		CAppUtils::StartExtDiff(tempfile,file1,
							pPath->GetGitPathString(),
							pPath->GetGitPathString() + _T(":") + rev1.Left(g_Git.GetShortHASHLength()),
							g_Git.m_CurrentDir + _T("\\") + pPath->GetWinPathString(), g_Git.m_CurrentDir + _T("\\") + pPath->GetWinPathString(),
							git_revnum_t(GIT_REV_ZERO), rev1
							, flags);
	else
		CAppUtils::StartExtDiff(file1,tempfile,
							pPath->GetGitPathString() + _T(":") + rev1.Left(g_Git.GetShortHASHLength()),
							pPath->GetGitPathString(),
							g_Git.m_CurrentDir + _T("\\") + pPath->GetWinPathString(), g_Git.m_CurrentDir + _T("\\") + pPath->GetWinPathString(),
							rev1, git_revnum_t(GIT_REV_ZERO)
							, flags);

	return 0;
}

int CGitDiff::SubmoduleDiff(CTGitPath * pPath,CTGitPath * /*pPath2*/, git_revnum_t rev1, git_revnum_t rev2, bool /*blame*/, bool /*unified*/)
{
	CString oldhash;
	CString newhash;
	bool dirty = false;
	CString cmd;
	bool isWorkingCopy = false;
	if( rev2 == GIT_REV_ZERO || rev1 == GIT_REV_ZERO )
	{
		oldhash = GIT_REV_ZERO;
		newhash = GIT_REV_ZERO;

		CString rev;
		if( rev2 != GIT_REV_ZERO )
			rev = rev2;
		if( rev1 != GIT_REV_ZERO )
			rev = rev1;

		isWorkingCopy = true;

		cmd.Format(_T("git.exe diff %s -- \"%s\""),
		rev,pPath->GetGitPathString());

		CString output, err;
		if (g_Git.Run(cmd, &output, &err, CP_UTF8))
		{
			CMessageBox::Show(NULL, output + L"\n" + err, _T("TortoiseGit"), MB_OK|MB_ICONERROR);
			return -1;
		}
		int start =0;
		int oldstart = output.Find(_T("-Subproject commit"),start);
		if(oldstart<0)
		{
			CMessageBox::Show(NULL,_T("Subproject Diff Format error") ,_T("TortoiseGit"),MB_OK|MB_ICONERROR);
			return -1;
		}
		oldhash = output.Mid(oldstart+ CString(_T("-Subproject commit")).GetLength()+1,40);
		start = 0;
		int newstart = output.Find(_T("+Subproject commit"),start);
		if(oldstart<0)
		{
			CMessageBox::Show(NULL,_T("Subproject Diff Format error") ,_T("TortoiseGit"),MB_OK|MB_ICONERROR);
			return -1;
		}
		newhash = output.Mid(newstart+ CString(_T("+Subproject commit")).GetLength()+1,40);
		dirty = output.Mid(newstart + CString(_T("+Subproject commit")).GetLength() + 41) == _T("-dirty\n");
	}
	else
	{
		cmd.Format(_T("git.exe diff-tree -r -z %s %s -- \"%s\""),
		rev2,rev1,pPath->GetGitPathString());

		BYTE_VECTOR bytes, errBytes;
		if(g_Git.Run(cmd, &bytes, &errBytes))
		{
			CString err;
			g_Git.StringAppend(&err, &errBytes[0], CP_UTF8);
			CMessageBox::Show(NULL,err,_T("TortoiseGit"),MB_OK|MB_ICONERROR);
			return -1;
		}

		g_Git.StringAppend(&oldhash, &bytes[15], CP_UTF8, 40);
		g_Git.StringAppend(&newhash, &bytes[15+41], CP_UTF8, 40);

	}

	CString oldsub;
	CString newsub;
	bool oldOK = false, newOK = false;

	CGit subgit;
	subgit.m_CurrentDir=g_Git.m_CurrentDir+_T("\\")+pPath->GetWinPathString();
	CSubmoduleDiffDlg::ChangeType changeType = CSubmoduleDiffDlg::Unknown;

	if(pPath->HasAdminDir())
	{
		int encode=CAppUtils::GetLogOutputEncode(&subgit);
		int oldTime = 0, newTime = 0;

		if(oldhash != GIT_REV_ZERO)
		{
			CString cmdout, cmderr;
			cmd.Format(_T("git log -n1 --pretty=format:\"%%ct %%s\" %s"), oldhash);
			oldOK = !subgit.Run(cmd, &cmdout, &cmderr, encode);
			if (oldOK)
			{
				int pos = cmdout.Find(_T(" "));
				oldTime = _ttoi(cmdout.Left(pos));
				oldsub = cmdout.Mid(pos + 1);
			}
			else
				oldsub = cmderr;
		}
		if (newhash != GIT_REV_ZERO)
		{
			CString cmdout, cmderr;
			cmd.Format(_T("git log -n1 --pretty=format:\"%%ct %%s\" %s"), newhash);
			newOK = !subgit.Run(cmd, &cmdout, &cmderr, encode);
			if (newOK)
			{
				int pos = cmdout.Find(_T(" "));
				newTime = _ttoi(cmdout.Left(pos));
				newsub = cmdout.Mid(pos + 1);
			}
			else
				newsub = cmderr;
		}

		if (oldhash == GIT_REV_ZERO)
		{
			oldOK = true;
			changeType = CSubmoduleDiffDlg::NewSubmodule;
		}
		else if (newhash == GIT_REV_ZERO)
		{
			newOK = true;
			changeType = CSubmoduleDiffDlg::DeleteSubmodule;
		}
		else if (oldhash != newhash)
		{
			bool ffNewer = false, ffOlder = false;
			ffNewer = subgit.IsFastForward(oldhash, newhash);
			if (!ffNewer)
			{
				ffOlder = subgit.IsFastForward(newhash, oldhash);
				if (!ffOlder)
				{
					if (newTime > oldTime)
						changeType = CSubmoduleDiffDlg::NewerTime;
					else if (newTime < oldTime)
						changeType = CSubmoduleDiffDlg::OlderTime;
					else
						changeType = CSubmoduleDiffDlg::SameTime;
				}
				else
					changeType = CSubmoduleDiffDlg::Rewind;
			}
			else
				changeType = CSubmoduleDiffDlg::FastForward;
		}
	}

	CSubmoduleDiffDlg submoduleDiffDlg;
	submoduleDiffDlg.SetDiff(pPath->GetWinPath(), isWorkingCopy, oldhash, oldsub, oldOK, newhash, newsub, newOK, dirty, changeType);
	submoduleDiffDlg.DoModal();

	return 0;
}

int CGitDiff::Diff(CTGitPath * pPath,CTGitPath * pPath2, git_revnum_t rev1, git_revnum_t rev2, bool /*blame*/, bool /*unified*/)
{
	CString temppath;
	GetTempPath(temppath);

	// make sure we have HASHes here, otherwise filenames might be invalid
	rev1 = g_Git.GetHash(rev1);
	rev2 = g_Git.GetHash(rev2);

	CString file1;
	CString title1;
	CString cmd;

	if(pPath->IsDirectory() || pPath2->IsDirectory())
	{
		return SubmoduleDiff(pPath,pPath2,rev1,rev2);
	}

	if(rev1 != GIT_REV_ZERO )
	{
		TCHAR szTempName[MAX_PATH];
		GetTempFileName(temppath, pPath->GetBaseFilename(), 0, szTempName);
		CString temp(szTempName);
		DeleteFile(szTempName);
		CreateDirectory(szTempName, NULL);
		// use original file extension, an external diff tool might need it
		file1.Format(_T("%s\\%s-%s-right%s"),
				temp,
				pPath->GetBaseFilename(),
				rev1.Left(g_Git.GetShortHASHLength()),
				pPath->GetFileExtension());
		title1 = pPath->GetFileOrDirectoryName() + _T(":") + rev1.Left(g_Git.GetShortHASHLength());
		g_Git.GetOneFile(rev1,*pPath,file1);
		::SetFileAttributes(file1, FILE_ATTRIBUTE_READONLY);
	}
	else
	{
		file1=g_Git.m_CurrentDir+_T("\\")+pPath->GetWinPathString();
		title1.Format( IDS_DIFF_WCNAME, pPath->GetFileOrDirectoryName() );
		if (!PathFileExists(file1))
		{
			CString sMsg;
			sMsg.Format(IDS_PROC_DIFFERROR_FILENOTINWORKINGTREE, file1);
			if (MessageBox(NULL, sMsg, _T("TortoiseGit"), MB_ICONEXCLAMATION | MB_OKCANCEL) == IDCANCEL)
				return 1;
			if (!CCommonAppUtils::FileOpenSave(file1, NULL, IDS_DIFF_WCNAME, IDS_COMMONFILEFILTER, true))
				return 1;
			title1.Format(IDS_DIFF_WCNAME, CTGitPath(file1).GetUIFileOrDirectoryName());
		}
	}

	CString file2;
	CString title2;
	if(rev2 != GIT_REV_ZERO)
	{
		TCHAR szTempName[MAX_PATH];
		GetTempFileName(temppath, pPath2->GetBaseFilename(), 0, szTempName);
		CString temp(szTempName);
		DeleteFile(szTempName);
		CreateDirectory(szTempName, NULL);
		CTGitPath fileName = *pPath2;
		if (rev1 == GIT_REV_ZERO && pPath2->m_Action & CTGitPath::LOGACTIONS_REPLACED)
			fileName = CTGitPath(pPath2->GetGitOldPathString());

		// use original file extension, an external diff tool might need it
		file2.Format(_T("%s\\%s-%s-left%s"),
				temp,
				fileName.GetBaseFilename(),
				rev2.Left(g_Git.GetShortHASHLength()),
				fileName.GetFileExtension());
		title2 = fileName.GetFileOrDirectoryName() + _T(":") + rev2.Left(g_Git.GetShortHASHLength());
		g_Git.GetOneFile(rev2, fileName, file2);
		::SetFileAttributes(file2, FILE_ATTRIBUTE_READONLY);
	}
	else
	{
		file2=g_Git.m_CurrentDir+_T("\\")+pPath2->GetWinPathString();
		title2.Format( IDS_DIFF_WCNAME, pPath2->GetFileOrDirectoryName() );
	}

	if (pPath->m_Action == pPath->LOGACTIONS_ADDED)
	{
		CGitDiff::DiffNull(pPath, rev1, true);
	}
	else if (pPath->m_Action == pPath->LOGACTIONS_DELETED)
	{
		CGitDiff::DiffNull(pPath, rev2, false);
	}
	else
	{
		CAppUtils::DiffFlags flags;
		CAppUtils::StartExtDiff(file2,file1,
								title2,
								title1,
								g_Git.m_CurrentDir + _T("\\") + pPath2->GetWinPathString(),
								g_Git.m_CurrentDir + _T("\\") + pPath->GetWinPathString(),
								rev2,
								rev1,
								flags);
	}
	return 0;
}

int CGitDiff::DiffCommit(CTGitPath &path, GitRev *r1, GitRev *r2)
{
	return DiffCommit(path, path, r1, r2);
}

int CGitDiff::DiffCommit(CTGitPath path1, CTGitPath path2, GitRev *r1, GitRev *r2)
{
	if (path1.GetWinPathString().IsEmpty())
	{
		CFileDiffDlg dlg;
		dlg.SetDiff(NULL, *r1, *r2);
		dlg.DoModal();
	}
	else if (path1.IsDirectory())
	{
		CFileDiffDlg dlg;
		dlg.SetDiff(&path1, *r1, *r2);
		dlg.DoModal();
	}
	else
	{
		Diff(&path1, &path2, r1->m_CommitHash.ToString(), r2->m_CommitHash.ToString());
	}
	return 0;
}

int CGitDiff::DiffCommit(CTGitPath &path, CString r1, CString r2)
{
	return DiffCommit(path, path, r1, r2);
}


int CGitDiff::DiffCommit(CTGitPath path1, CTGitPath path2, CString r1, CString r2)
{
	if (path1.GetWinPathString().IsEmpty())
	{
		CFileDiffDlg dlg;
		dlg.SetDiff(NULL, r1, r2);
		dlg.DoModal();
	}
	else if (path1.IsDirectory())
	{
		CFileDiffDlg dlg;
		dlg.SetDiff(&path1, r1, r2);
		dlg.DoModal();
	}
	else
	{
		Diff(&path1, &path2, r1, r2);
	}
	return 0;
}
