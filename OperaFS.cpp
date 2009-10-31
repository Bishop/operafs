// OperaFS.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "OperaFS.h"
#include "fsplugin.h"

///// ms-help://MS.VSCC.v80/MS.MSDN.v80/MS.WIN32COM.v10.en/sysinfo/base/converting_a_time_t_value_to_a_file_time.htm
void TimetToFileTime(time_t t, LPFILETIME pft)
{
	LONGLONG ll = Int32x32To64(t, 10000000) + 116444736000000000;
	pft->dwLowDateTime = (DWORD) ll;
	pft->dwHighDateTime = (DWORD)(ll >> 32);
}
///// end


/// Глобальные переменные
ONode *tree = NULL;
ONode *cur = NULL;
char ini_file[MAX_PATH];
///

BOOL OClearTree();
ONode *GetNode(const char *path);
BOOL OCreateTree();



BOOL APIENTRY DllMain(HMODULE hModule,
					  DWORD  ul_reason_for_call,
					  LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		OClearTree();
		break;
	}
    return TRUE;
}


// service

ONode *NewNode()
{
	ONode *nn = new ONode();
	ZeroMemory(nn, sizeof(ONode));
	return nn;
}

inline ONode *NewNode(ONode *parent, ONode *prev)
{
	ONode *result = NewNode();
	result->_parent = parent;
	if (!parent->_firstchild)
		parent->_firstchild = result;
	else
	{
		result->_prev = prev;
		prev->_next = result;
	}
	return result;
}

void OVerifyFile(const char *path)
{
	char nodepath[MAX_PATH];
	strncpy_s(nodepath, MAX_PATH, path, MAX_PATH - 1);
	char *pt = nodepath;
	for (int i = 0; i < 3; i++, pt++)
		if (!(pt = strchr(pt, '\\')) && i < 2)
			return;
	if (--pt)
		*pt = 0;
	ONode *node = GetNode(nodepath);
	HANDLE hFile = CreateFile(node->Url, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile)
	{
		FILETIME ft;
		GetFileTime(hFile, NULL, NULL, &ft);
		CloseHandle(hFile);
		if (CompareFileTime(&ft, &node->Created) == 1)
			OCreateTree();
	}
}

// Читает файл fname, пишет в fnode дату изменения этого файла
char *OReadFile(const char *fname, ONode *fnode)
{
	char *buffer = NULL;
	HANDLE hFile = CreateFile(fname, GENERIC_READ, FILE_SHARE_READ,
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile)
	{
		DWORD dwSize = 0, dwReadSzie;
		dwSize = GetFileSize(hFile, NULL);
		buffer = new TCHAR[dwSize + 1];

		GetFileTime(hFile, NULL, NULL, &fnode->Created);

		if (ReadFile(hFile, buffer, dwSize, &dwReadSzie, NULL))
			buffer[dwSize] = '\0';
		else
		{
			delete [] buffer;
			buffer = NULL;
		}

		CloseHandle(hFile);
	}
	return buffer;
}

// Очистка дерева с обнулением всех указателей
BOOL OClearTree()
{
	ONode *curNode = tree, *parentNode = tree, *prevNode = NULL;
	while (parentNode)
	{
		if (parentNode->_firstchild)
			curNode = parentNode->_firstchild;
		else
			curNode = parentNode;
		if (!curNode->_firstchild)
		{
			parentNode->_firstchild = curNode->_next;
			if (curNode->_next)
				curNode->_next->_prev = NULL;
			else
				parentNode = parentNode->_parent;
			delete [] curNode->Name;
			delete [] curNode->Url;
			delete [] curNode->Description;
			delete [] curNode->ShortName;
			delete [] curNode->IconFile;
			delete curNode;
			curNode = NULL;
		}
		else
		{
			parentNode = curNode;
		}
	}
	tree = NULL;
	return TRUE;
}
	//ON PERSONALBAR=YES
	//PERSONALBAR_POS=0
	//IN PANEL=YES
	//PANEL_POS=7

// Обработка узла, заполнение структуры ONode
BOOL OProcessNode(ONode *node, char* pt)
{
	static const char crlf[] = "\r\n";
	static const char cc_id[] = "ID";
	static const char cc_name[] = "NAME";
	static const char cc_url[] = "URL";
	static const char cc_created[] = "CREATED";
	static const char cc_visited[] = "VISITED";
	static const char cc_description[] = "DESCRIPTION";
	static const char cc_shortname[] = "SHORT NAME";
	static const char cc_iconfile[] = "ICONFILE";
	static const char cc_trashfolder[] = "TRASH FOLDER";

	char *pe = NULL, *ps = NULL;
	char **curstr = NULL;
	char cID[9];
	int namelen = 0;
	BOOL bAddDotID = FALSE;

	pt += 2; // cr lf
	while (*pt++ == 9)
	{
		ps = strstr(pt, "=");
		pe = strstr(pt, crlf);
		int sz = ps - pt;

		if (pe < pt || ps < pt)
			return FALSE;

		curstr = NULL;
		bAddDotID = FALSE;
		if (strncmp(pt, cc_id, sz) == 0)
		{
			node->ID = atoi(++ps);
			strncpy_s(cID, 9, ps, pe - ps);
		}
		else if (strncmp(pt, cc_name, sz) == 0)
		{
			curstr = &node->Name;
			bAddDotID = !(node->Flags & ofs_folder);
		}
		else if (strncmp(pt, cc_url, sz) == 0)
			curstr = &node->Url;
		else if (strncmp(pt, cc_description, sz) == 0)
			curstr = &node->Description;
		else if (strncmp(pt, cc_shortname, sz) == 0)
			curstr = &node->ShortName;
		else if (strncmp(pt, cc_iconfile, sz) == 0)
		{
			curstr = &node->IconFile;
			// Opera 9.5 
			//node->Flags |= ofs_haveicon;
		}
		else if (strncmp(pt, cc_created, sz) == 0)
			TimetToFileTime(atoi(++ps), &node->Created);
		else if (strncmp(pt, cc_visited, sz) == 0)
			TimetToFileTime(atoi(++ps), &node->Visited);
		else if (strncmp(pt, cc_trashfolder, sz) == 0)
			node->Flags |= ofs_trashfolder;

		if (node->Flags & ofs_url)
			node->Flags |= ofs_haveicon;

		if (curstr)
		{
			pt = ps + 1;
			pe[0] = 0;
			pe[1] = 0;
			sz = MultiByteToWideChar(CP_UTF8, 0, pt, -1, NULL, 0);
			wchar_t *wstr = new wchar_t[sz];
			sz = MultiByteToWideChar(CP_UTF8, 0, pt, -1, wstr, sz);
			sz = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wstr, -1, NULL, 0, NULL, NULL);
			if (bAddDotID)
				namelen = sz += 10;
			*curstr = new TCHAR[sz];
			sz = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wstr, -1, *curstr, sz, NULL, NULL);
			delete [] wstr;
			pe[0] = 13;
			pe[1] = 10;
		}
		pt = pe + 2;
	}

	if (!(node->Flags & ofs_folder))
	{
		node->Name[namelen - 11] = '.';
		strcpy_s(node->Name + namelen - 10, 9, cID);
	}

	if (node->Flags & ofs_note)
	{
		pt = node->Name;
		pe = strchr(pt, 2);
		while (pe)
		{
			pe[0] = 13;
			pe[1] = 10;
			pe = strchr(pe, 2);
		}
	}
	
	return TRUE;
}

// Анализ буфера
BOOL OAnalyse(ONode *tree, const char *fname)
{
	static const char crlf[] = "\r\n";		// Конец строки
	static const char sep[] = "#SEPERATOR";
	static const char url[] = "#URL";
	static const char note[] = "#NOTE";
	static const char fld[] = "#FOLDER";
	static const char b[] = "\r\n#";		// Начало секции
	static const char flde[] = "\r\n-";	// Конец папки

	char *buffer = OReadFile(fname, tree);
	if (!buffer)
		return FALSE;

	char *pt = buffer, *pe;

	// Проверка корректности файла
	if (strstr(pt, sign_first_line) != pt)
		return FALSE;
	pt += strlen(sign_first_line) + 2;
	if (strstr(pt, sign_second_line) != pt)
		return FALSE;
	pt += strlen(sign_second_line) + 2;

	// Анализ
	ONode *curNode = NULL, *parentNode = tree, *prevNode = NULL;
	
	pe = strstr(pt, flde);
	pt = strstr(pt, b);
	if (pt && pe)
		pt = min(pt, pe);
	if (pt)
		pt += 2;

	while (pt)
	{
		pe = strstr(pt, crlf);
		BOOL process = FALSE;
		if (strncmp(pt, fld, pe - pt) == 0) // Folder
		{
			curNode = NewNode(parentNode, prevNode);
			parentNode = curNode;
			prevNode = curNode;
			curNode->Flags = ofs_folder;
			process = TRUE;
		}
		else if (strncmp(pt, "-", pe - pt) == 0) // Folder close
		{
			if (parentNode == curNode)
			{
				prevNode = curNode;
				parentNode = curNode->_parent;
			}
			else
			{
				prevNode = parentNode;
				parentNode = parentNode->_parent;
				curNode = prevNode;
			}
		}
		else if (strncmp(pt, url, pe - pt) == 0) // Url
		{
			curNode = NewNode(parentNode, prevNode);
			//curNode->_parent = parentNode;
			// url
			curNode->Flags = ofs_url;
			prevNode = curNode;
			process = TRUE;
		}
		else if (strncmp(pt, note, pe - pt) == 0) // Note
		{
			curNode = NewNode(parentNode, prevNode);
			//curNode->_parent = parentNode;
			// note
			curNode->Flags = ofs_note;
			prevNode = curNode;
			process = TRUE;
		}
		else if (strncmp(pt, sep, pe - pt) == 0)
		{
		}

		pt = pe;

		if (process)
			OProcessNode(curNode, pt);

		// next iteration
		pe = strstr(pt, flde);
		pt = strstr(pt, b);
		if (pt && pe)
			pt = min(pt, pe);
		if (pt)
			pt += 2;
	};

	delete [] buffer;
	buffer = NULL;

	return TRUE;
}


BOOL OCreateTree()
{
	OClearTree();
	tree = NewNode();
	tree->Name = new char[2];
	strcpy_s(tree->Name, 2, "\\");

	// Номер профиля
	int n = 1;
	char cKey[MAX_PATH];
	char cPath[MAX_PATH];

	ONode *node = NULL, *nodePrev = NULL;
	DWORD dwRSize = 100;
	while (dwRSize)
	{
		sprintf_s(cKey, cProfileName, n);
		dwRSize = GetPrivateProfileString(pl_name, cKey, NULL, cPath, MAX_PATH, ini_file);
		if (dwRSize)
		{
			node = NewNode(tree, nodePrev);
			node->Name = new char[++dwRSize];
			strcpy_s(node->Name, dwRSize, cPath);
			node->ID = n;
			
			static const char *cBookMarks[] = {"bookmarks.adr", "opera6.adr", "notes.adr"};
			static const char *cOpIniSect[] = {"User Prefs", "User Prefs", "MailBox"};
			static const char *cOpIniKeys[] = {"Hot List File Ver2", "Hot List File Ver2", "NotesFile"};
			
			sprintf_s(cKey, cProfilePath, n);
			dwRSize = GetPrivateProfileString(pl_name, cKey, NULL, cPath, MAX_PATH, ini_file);
			if (dwRSize)
			{
				node->Url = new char[++dwRSize];
				strcpy_s(node->Url, dwRSize, cPath);

				ONode *chNode = NULL, *chNodePrev = NULL;
				for (int i = 0; i < sizeof(cBookMarks)/sizeof(char*); i++)
				{
					chNode = NewNode(node, chNodePrev);
					chNode->Flags = ofs_folder;

					int nlen = strnlen(cBookMarks[i], MAX_PATH) + 1;
					chNode->Name = new char[nlen];
					strcpy_s(chNode->Name, nlen, cBookMarks[i]);
					chNode->Url = new char[MAX_PATH];
					// соберем в cKey путь к ini файлу
					strncpy_s(cKey, MAX_PATH, cPath, MAX_PATH - 1);
					strncpy_s(cKey + strlen(cKey), MAX_PATH - strlen(cKey), "opera6.ini", 10);
					GetPrivateProfileString(cOpIniSect[i], cOpIniKeys[i], NULL, chNode->Url, MAX_PATH, cKey);
					if (strlen(chNode->Url) != 0)
					{
						wchar_t *wstr = new wchar_t[MAX_PATH * 2];
						MultiByteToWideChar(CP_UTF8, 0, chNode->Url, -1, wstr, MAX_PATH * 2);
						WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wstr, -1, chNode->Url, MAX_PATH, NULL, NULL);
						delete [] wstr;
					}
					else
						sprintf_s(chNode->Url, MAX_PATH, "%s%s", cPath, cBookMarks[i]);
					if (OAnalyse(chNode, chNode->Url))
						chNodePrev = chNode;
					else
					{
						delete [] chNode->Name;
						delete [] chNode->Url;
						if (node->_firstchild == chNode)
							node->_firstchild = NULL;
						else
							chNode->_prev->_next = NULL;
						delete chNode;
					}
				}
				node->Flags = ofs_folder;
				nodePrev = node;
			}
			else
			{
				delete [] node->Name;
				if (tree->_firstchild == node)
					tree->_firstchild = NULL;
				else
					node->_prev->_next = NULL;
				delete node;
//				node = NULL;
			}
		}
		n++;
	}

	return TRUE;
}

inline ONode *GetNode(const char *path)
{
	if (!tree)
		OCreateTree();

	const char sep_char[] = "\\";
	char *token, *next_token;
	char inner_buffer[MAX_PATH];
	strncpy_s(inner_buffer, MAX_PATH, path, MAX_PATH - 1);

	ONode *cur = tree;
	token = strtok_s(inner_buffer, sep_char, &next_token);
	while (token)
	{
		cur = cur->_firstchild;
		while (cur)
		{
			if (strncmp(cur->Name, token, strnlen(token, MAX_PATH)) == 0)
				break;
			else
				cur = cur->_next;
		}
		if (!cur)
			return NULL;
		token = strtok_s(NULL, sep_char, &next_token);
	}

	return cur;
}

// export
OPERAFS_API int FsInit(int PluginNr, tProgressProc pProgressProc,
						tLogProc pLogProc, tRequestProc pRequestProc)
{
	return 0;
}

OPERAFS_API HANDLE FsFindFirst(char* Path, WIN32_FIND_DATA *FindData)
{
	OVerifyFile(Path);
	ZeroMemory(FindData, sizeof(WIN32_FIND_DATA));
	FindData->ftLastWriteTime.dwHighDateTime=0xFFFFFFFF;
	FindData->ftLastWriteTime.dwLowDateTime=0xFFFFFFFE;
	
	cur = GetNode(Path);

	if (!cur)
		return INVALID_HANDLE_VALUE;

	cur = cur->_firstchild;
	if (cur)
	{
		strncpy_s(FindData->cFileName, MAX_PATH, cur->Name, MAX_PATH-1);
		if (cur->Flags & ofs_folder)
			FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		FindData->ftLastWriteTime = cur->Created;
		return cur;
	}
	else
	{
		SetLastError(ERROR_NO_MORE_FILES);
		return INVALID_HANDLE_VALUE;
	}
}

OPERAFS_API BOOL FsFindNext(HANDLE Hdl, WIN32_FIND_DATA *FindData)
{
	if (cur->_next)
	{
		ZeroMemory(FindData, sizeof(WIN32_FIND_DATA));
		FindData->ftLastWriteTime.dwHighDateTime=0xFFFFFFFF;
		FindData->ftLastWriteTime.dwLowDateTime=0xFFFFFFFE;

		cur = cur->_next;
		strncpy_s(FindData->cFileName, MAX_PATH, cur->Name, MAX_PATH-1);
		if (cur->Flags & ofs_folder)
			FindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		FindData->ftLastWriteTime = cur->Created;
		return TRUE;
	}
	else
		return FALSE;
}

OPERAFS_API int FsFindClose(HANDLE Hdl)
{
	return 0;
}


////
OPERAFS_API void FsGetDefRootName(char* DefRootName, int maxlen)
{
	strcpy_s(DefRootName, maxlen, "Opera FS");
}

OPERAFS_API void FsSetDefaultParams(FsDefaultParamStruct* dps)
{
	strncpy_s(ini_file, MAX_PATH, dps->DefaultIniName, MAX_PATH - 1);
}

BOOL OGetIconFile9(ONode *node, const char *profile, char *iconname)
{
	sprintf_s(iconname, MAX_PATH, "%s%s%s", profile, iconpath, node->IconFile);
	WIN32_FIND_DATA fd;
	HANDLE find = FindFirstFile(iconname, &fd);
	if (find != INVALID_HANDLE_VALUE)
	{
		FindClose(find);
		return TRUE;
	}
	else
	{
		FindClose(find);
		return FALSE;
	}
}

BOOL OGetIconFile95(ONode *node, const char *oprofile, char *iconname)
{
	// Opera 9.5 
	BOOL result = FALSE;
	static char url[MAX_PATH*2];
	static char profile[MAX_PATH];
	strcpy_s(url, MAX_PATH*2, node->Url);
	strcpy_s(profile, MAX_PATH, oprofile);
	char *addr = NULL;
	if (strstr(url, "http://") == url)
		addr = url + 7;
	else if (strstr(url, "ftp://") == url)
		addr = url + 6;
	else if (strstr(url, "javascript:") == url)
		addr = NULL;
	else
		addr = url;

	if (addr)
	{
		char *slash = strchr(addr, '/');
		if (slash)
			*slash = 0;
		sprintf_s(iconname, MAX_PATH, "%s%s%s.idx", profile, iconpath, addr);

		char *buffer = NULL;
		HANDLE hFile = CreateFile(iconname, GENERIC_READ, FILE_SHARE_READ,
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			static char localappdata[MAX_PATH];
			if (ExpandEnvironmentStrings("%LOCALAPPDATA%\\Opera", localappdata, MAX_PATH))
			{
				char *profiledir = profile + strlen(profile) - 1;
				char *backslash = NULL;
				int n = 0;
				while (profile != profiledir)
				{
					if (*profiledir == '\\')
						n++;
					if (n == 3)
						break;
					profiledir--;
				}
				if (profile != profiledir)
				{
					sprintf_s(profile, MAX_PATH, "%s%s", localappdata, profiledir);
					sprintf_s(iconname, MAX_PATH, "%s%s%s.idx", profile, iconpath, addr);
					hFile = CreateFile(iconname, GENERIC_READ, FILE_SHARE_READ,
						NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				}
			}
		}
		if (hFile != INVALID_HANDLE_VALUE)
		{
			DWORD dwSize = 0, dwReadSzie;
			dwSize = GetFileSize(hFile, NULL);
			buffer = new TCHAR[dwSize + 1];

			if (ReadFile(hFile, buffer, dwSize, &dwReadSzie, NULL))
			{
				buffer[dwSize] = '\0';
				char *b = NULL, *e = NULL;
				b = strchr(buffer, 10);
				e = strchr(b + 1, 10);

				if (b && e)
				{
					b++;
					static char newname[MAX_PATH];
					strncpy_s(newname, MAX_PATH, b, e - b);
					sprintf_s(iconname, MAX_PATH, "%s%s%s", profile, iconpath, newname);
					result = TRUE;
				}
			}
			delete [] buffer;
			CloseHandle(hFile);
		}
	}
	return result;
}

OPERAFS_API int FsExtractCustomIcon(char* RemoteName, int ExtractFlags, HICON* TheIcon)
{
	int result = FS_ICON_USEDEFAULT;
	static char iconname[MAX_PATH];

	ONode *node = GetNode(RemoteName);
	if (node->Flags & ofs_haveicon)
	{
		BOOL nado = FALSE;
		ONode *profile = NULL;

		if (node->IconFile && (node->IconFile[1] == ':')) // Уже содержит путь к файлу
			nado = TRUE;
		else if (node->IconFile && (strcmp(node->IconFile, "Bookmark ") == 0))
		{
			delete [] node->IconFile;
			node->IconFile = NULL;
			nado = FALSE;
		}
		else 
		{
			profile = node->_parent;
			while (profile->_parent != tree)
				profile = profile->_parent;
			if (node->IconFile && OGetIconFile9(node, profile->Url, iconname))
				nado = TRUE;
			else
			{
				delete [] node->IconFile;
				node->IconFile = NULL;
				nado = OGetIconFile95(node, profile->Url, iconname);
			}

			if (nado)
			{
				size_t iconnamelen = strnlen(iconname, MAX_PATH) + 1;
				node->IconFile = new char[iconnamelen];
				strcpy_s(node->IconFile, iconnamelen, iconname);
			}
		}

		if (nado && ExtractIconEx(node->IconFile, 0, NULL, TheIcon, 1))
			result = FS_ICON_EXTRACTED_DESTROY;
		else
			node->Flags &= ~ofs_haveicon;
	}
	else if (node->Flags & ofs_trashfolder)
	{
		static char buffer[MAX_PATH];
		DWORD size = MAX_PATH;
		DWORD type = REG_EXPAND_SZ;
		static const char empty[] = "Empty";
		static const char full[] = "Full";

		HKEY key = NULL;
		RegOpenKeyEx(HKEY_CLASSES_ROOT, "CLSID\\{645FF040-5081-101B-9F08-00AA002F954E}\\DefaultIcon", 0,
			KEY_READ, &key);
		RegQueryValueEx(key, node->_firstchild?full:empty, NULL, &type, (LPBYTE) buffer, &size);
		RegCloseKey(key);

		char *comma = strchr(buffer, ',');
		*comma = 0;
		ExpandEnvironmentStrings(buffer, iconname, MAX_PATH);
		
		if (ExtractIconEx(iconname, atoi(++comma), NULL, TheIcon, 1))
			result = FS_ICON_EXTRACTED_DESTROY;
	}
	return result;
}

// File Operations
OPERAFS_API int FsExecuteFile(HWND MainWin, char* RemoteName, char* Verb)
{
	if (strcmp(Verb, "open") == 0)
	{
		ONode *node = GetNode(RemoteName);
		ShellExecute(MainWin, Verb, node->Url, NULL, NULL, SW_SHOWNORMAL);
		return FS_EXEC_OK;
	}
	return FS_EXEC_ERROR;
}

OPERAFS_API int FsPutFile(char* LocalName, char* RemoteName, int CopyFlags)
{
	char cext[10];
	_splitpath_s(LocalName, NULL, 0, NULL, 0, NULL, 0, cext, sizeof(cext));

	if (_stricmp(cext, ".adr") == 0 || _stricmp(cext, ".ini") == 0)
	if (strrchr(RemoteName, '\\') == RemoteName) // мы в корне Opera FS 
	{
		char cKey[MAX_PATH];
		char cPath[MAX_PATH];
		ONode *node = tree->_firstchild;
		// посчитаем профили
		int n = 1;
		while (node)
		{
			node = node->_next;
			n++;
		}
		sprintf_s(cKey, cProfileName, n);
		WritePrivateProfileString(pl_name, cKey, RemoteName + 1, ini_file);
		sprintf_s(cKey, cProfilePath, n);
		strncpy_s(cPath, MAX_PATH, LocalName, strrchr(LocalName, '\\') - LocalName + 1);
		WritePrivateProfileString(pl_name, cKey, cPath, ini_file);
		OCreateTree();
	}
	return FS_FILE_OK;
}

OPERAFS_API int FsGetFile(char* RemoteName, char* LocalName, int CopyFlags, RemoteInfoStruct* ri)
{
	static char file_name[MAX_PATH];
	static char file_path[MAX_PATH];
	static char file_drive[_MAX_DRIVE];

	ONode *node = GetNode(RemoteName);

	char *endname = NULL;
	if (node->Flags & ofs_note)
		endname = strchr(node->Name, 13);
	if (node->Flags & ofs_url || !endname)
		endname = strrchr(node->Name, '.');
	int file_name_len = 0;
	if (endname)
		file_name_len = endname - node->Name;
	if (file_name_len > 30)
		file_name_len = 30;
	strncpy_s(file_name, MAX_PATH, node->Name, file_name_len);
	file_name_len = strlen(file_name);
	
	char *nosymbol = file_name;
	int nosym = strcspn(file_name, "\\/:*?\"<>|");
	while (nosym != file_name_len)
	{
		nosymbol += nosym;
		*nosymbol = '_';
		file_name_len -= nosym;
		nosym = strcspn(nosymbol, "\\/:*?\"<>|");
	}

	_splitpath_s(LocalName, file_drive, _MAX_DRIVE, file_path, MAX_PATH, NULL, 0, NULL, 0);
	_makepath_s(LocalName, MAX_PATH, file_drive, file_path, file_name, (node->Flags & ofs_note)?"txt":((node->Flags & ofs_url)?"url":NULL));

	if (node->Flags & ofs_note)
	{
		HANDLE hFile = CreateFile(LocalName, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_ARCHIVE, NULL);
		if (hFile)
		{
			DWORD dwWSize = 0;
			WriteFile(hFile, node->Name, strrchr(node->Name, '.') - node->Name/*strlen(node->Name)*/, &dwWSize, NULL);
			if (node->Url)
			{
				WriteFile(hFile, "\r\n", 2, &dwWSize, NULL);
				WriteFile(hFile, node->Url, strlen(node->Url), &dwWSize, NULL);
			}
			CloseHandle(hFile);
		}
	}
	else if (node->Flags & ofs_url)
		WritePrivateProfileString("InternetShortcut", "URL", node->Url, LocalName);

	return FS_FILE_OK;
}

OPERAFS_API BOOL FsDeleteFile(char* RemoteName)
{
	return TRUE;
}

OPERAFS_API BOOL FsRemoveDir(char* RemoteName)
{
	if (strrchr(RemoteName, '\\') == RemoteName) // мы в корне Opera FS 
	{
		ONode *node = GetNode(RemoteName);
		static char cKey[MAX_PATH];
		static char cPath[MAX_PATH];

		int n = node->ID;

		// Удаляем
		sprintf_s(cKey, cProfileName, n);
		WritePrivateProfileString(pl_name, cKey, NULL, ini_file);
		sprintf_s(cKey, cProfilePath, n);
		WritePrivateProfileString(pl_name, cKey, NULL, ini_file);

		// Меняем индексы
		DWORD dwRSize = 100;
		while (dwRSize)
		{
			n++;
			for (char i = 0; i < 2; i++)
			{
				sprintf_s(cKey, i?cProfileName:cProfilePath, n);
				dwRSize = GetPrivateProfileString(pl_name, cKey, NULL, cPath, MAX_PATH, ini_file);
				// если прочитали
				if (dwRSize)
				{
					// удаляем
					WritePrivateProfileString(pl_name, cKey, NULL, ini_file);
					// и сохраняем под предыдущим номером
					sprintf_s(cKey, i?cProfileName:cProfilePath, n - 1);
					WritePrivateProfileString(pl_name, cKey, cPath, ini_file);
				}
			}
		}
		// перестраиваем дерево
		OCreateTree();
	}
	return TRUE;
}

// Content
OPERAFS_API int FsContentGetSupportedField(int FieldIndex, char* FieldName, char* Units, int maxlen)
{
	static const char *fields[] = {"Created", "Visited", "Url", "Description", "ShortName"};
	static const int field_types[] = {ft_datetime, ft_datetime, ft_string, ft_string, ft_string, ft_nomorefields};

	if (FieldIndex < sizeof(field_types)/sizeof(int) - 1)
		strcpy_s(FieldName, maxlen, fields[FieldIndex]);
	return field_types[FieldIndex];
}

OPERAFS_API BOOL FsContentGetDefaultView(char* ViewContents, char* ViewHeaders, char* ViewWidths, char* ViewOptions, int maxlen)
{
	strcpy_s(ViewContents, maxlen, "[=<fs>.Url]\\n[=<fs>.ShortName]\\n[=<fs>.Description]\\n[=<fs>.Created]\\n[=<fs>.Visited]");
	strcpy_s(ViewHeaders, maxlen, "Url\\nКороткое имя\\nОписание\\nСоздана\\nПосещена");
	strcpy_s(ViewWidths, maxlen, "100, 30, 100, 50, 100, 60, 60");
	strcpy_s(ViewOptions, maxlen, "-1|1");
	return TRUE;
}
	
OPERAFS_API int FsContentGetValue(char* FileName, int FieldIndex, int UnitIndex, void* FieldValue, int maxlen, int flags)
{
	ONode *node = GetNode(FileName);
	if (!node) return ft_nosuchfield;
	flags = 0;
	int result = ft_nosuchfield;
	FILETIME *ft = (FILETIME *)FieldValue;
	char **curstr = NULL;
	switch (FieldIndex)
	{
	case 0:
		*ft = node->Created;
		result = (node->Created.dwHighDateTime || node->Created.dwLowDateTime) ? ft_datetime : ft_fieldempty;
		break;
	case 1:
		*ft = node->Visited;
		result = (node->Visited.dwHighDateTime || node->Visited.dwLowDateTime) ? ft_datetime : ft_fieldempty;
		break;
	case 2: curstr = &node->Url; break;
	case 3: curstr = &node->Description; break;
	case 4: curstr = &node->ShortName; break;
	default:
		result = ft_nosuchfield;
	}

	if (curstr && *curstr)
	{
		strncpy_s((char *)FieldValue, maxlen, *curstr, maxlen - 1);
		result = ft_string;
	}

	return result;
}

OPERAFS_API void FsContentPluginUnloading(void)
{
	OClearTree();
}