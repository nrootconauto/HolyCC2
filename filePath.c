#include <stdlib.h>
#include <string.h>
#include <cleanup.h>
#include <filePath.h>
static int pathIsAbsolute(const char *path) {
#ifdef __unix__
		return path[0]=='/';
#endif
#ifdef _WIN32
		if(strlen(path)<2)
				return 0;
		return path[1]==':';
#endif
}
static void free2(char **t) {
		free(*t);
}
char *fnFromPath(const char *path) {
			char *retVal=calloc(strlen(path)+1, 1);
#ifdef __unix__
		const char dirSep='/';
#endif
#ifdef _WIN32
		const char dirSep='\\';
#endif
		char *end=strrchr(path, dirSep);
		if(end)
				return strncpy(retVal,end+1,strlen(path)-(end-path)+1);
#ifdef _WIN32
		{
				char *drvEnd=strchr(path, ':');
				if(drvEnd)
						return strncpy(retVal,drvEnd+1,strlen(path)-(drvEnd-path)+1);
		}
#endif
		return strcpy(retVal, path);
}
char *dirFromPath(const char *cwd,const char *_path) {
		char *path CLEANUP(free2)=NULL;
		if(!pathIsAbsolute(_path)) {
				path=calloc(strlen(cwd)+strlen(_path)+1,1);
				strcpy(path, cwd);
				strcat(path, _path);
		} else {
				path=calloc(strlen(_path)+1,1);
				strcpy(path, _path);
		}
		char *retVal=calloc(strlen(path)+1, 1);
#ifdef __unix__
		const char dirSep='/';
#endif
#ifdef _WIN32
		const char dirSep='\\';
#endif
		char *end=strrchr(path, dirSep);
		if(end)
				return strncpy(retVal,path,end-path+1);
#ifdef _WIN32
		{
				char *drvEnd=strchr(path, ':');
				if(drvEnd)
						return strncpy(retVal,path,drvEnd-path+1);
		}
#endif
		return retVal;
}
