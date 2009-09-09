// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the OPERAFS_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// OPERAFS_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#define TC_API OPERAFS_API

#ifdef OPERAFS_EXPORTS
#define OPERAFS_API __declspec(dllexport)
#else
#define OPERAFS_API __declspec(dllimport)
#endif

struct ONode {
	int ID;
	char *Name;
	char *Url;
	char *Description;
	char *ShortName;
	char *IconFile;
	FILETIME Created;
	FILETIME Visited;
	DWORD Flags;

	ONode *_parent;
	ONode *_prev;
	ONode *_next;
	ONode *_firstchild;
};

static const char sign_first_line[] = "Opera Hotlist version 2.0";
static const char sign_second_line[] = "Options: encoding = utf8, version=3";

static const char cProfileName[] = "Profile%dName";
static const char cProfilePath[] = "Profile%dPath";

static const char pl_name[] = "OperaFS";

static const char iconpath[] = "images\\";

#define _USE_32BIT_TIME_T

#define ofs_url 1
#define ofs_note 2
#define ofs_folder 4
#define ofs_trashfolder 8
#define ofs_haveicon 16

