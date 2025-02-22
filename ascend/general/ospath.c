/*	ASCEND modelling environment
	Copyright (C) 2006-2010 Carnegie Mellon University

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2, or (at your option)
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*//**
	@file
	Platform-agnostic filesystem path manipulation functions.
	By John Pye.
*/

#if defined(WIN32) || defined(__WIN32) || defined(_MSC_VER)
# ifndef __WIN32__
#  define __WIN32__
# endif
#endif

#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#include "ospath.h"

//#define OSPATH_DEBUG

#ifndef __GNUC__
# ifndef __FUNCTION__
#  define __FUNCTION__ "<ospath>"
# endif
#endif

//#define VERBOSE

#if !defined(VERBOSE)
#ifndef NDEBUG
# define NDEBUG
#endif
#endif

#ifdef _MSC_VER
# define STAT _stat
#else
# define STAT stat
#endif

//#define TRY_GETPWUID

#define DO_FIXSLASHES

#ifndef NDEBUG
# include <assert.h>
# define M(MSG) fprintf(stderr,"%s:%d: (%s) %s\n",__FILE__,__LINE__,__FUNCTION__,MSG);fflush(stderr);fflush(stderr)
# define MC(CLR,MSG) fprintf(stderr,"%c[%sm%s:%d: (%s) %s%c[0m\n",27,CLR,__FILE__,__LINE__,__FUNCTION__,MSG,27);fflush(stderr)
# define MM(MSG) MC("34",MSG)
# define X(VAR) fprintf(stderr,"%s:%d: (%s) %s=%s\n",__FILE__,__LINE__,__FUNCTION__,#VAR,VAR);fflush(stderr)
# define C(VAR) fprintf(stderr,"%s:%d: (%s) %s=%c\n",__FILE__,__LINE__,__FUNCTION__,#VAR,VAR);fflush(stderr)
# define V(VAR) fprintf(stderr,"%s:%d: (%s) %s=%d\n",__FILE__,__LINE__,__FUNCTION__,#VAR,(VAR));fflush(stderr)
# define P(VAR) fprintf(stderr,"%s:%d: (%s) %s=%p\n",__FILE__,__LINE__,__FUNCTION__,#VAR,(VAR));fflush(stderr)
# define D(VAR) fprintf(stderr,"%s:%d: (%s) %s=",__FILE__,__LINE__,__FUNCTION__,#VAR);ospath_debug(VAR);fflush(stderr)
# define DD(VAR) fprintf(stderr,"%c[34;1m%s:%d: (%s)%c[0m %s=",27,__FILE__,__LINE__,__FUNCTION__,27,#VAR);ospath_debug(VAR);fflush(stderr)
#else
# include <assert.h>
# define M(MSG) ((void)0)
# define MC(CLR,MSG) ((void)0)
# define X(VAR) ((void)0)
# define C(VAR) ((void)0)
# define V(VAR) ((void)0)
# define D(VAR) ((void)0)
# define P(VAR) ((void)0)
# define DD(VAR) ((void)0)
# define MM(VAR) ((void)0)
#endif

#if defined(__WIN32__) && !defined(__MINGW32__)
# define STRCPY strcpy
# define STRNCPY(dest,src,n) strncpy_s(dest,n,src,n)
# define STRCAT strcat
# define STRNCAT strncat
# define STRTOK(STR,PAT,VAR) strtok_s(STR,PAT,&VAR)
# define STRTOKVAR(VAR) char *VAR
# define GETCWD _getcwd
# define GETENV(VAR) getenv(VAR)
# define CHDIR chdir
#elif defined(linux)
# define STRCPY strcpy
# define STRNCPY(dest,src,n) strncpy(dest,src,n)
# define STRCAT strcat
# define STRNCAT strncat
# define STRTOK(STR,PAT,VAR) strtok_r(STR,PAT,&VAR)
# define STRTOKVAR(VAR) char *VAR
# define GETCWD getcwd
# define GETENV(VAR) getenv(VAR)
# define CHDIR chdir
#else
# define STRCPY strcpy
# define STRNCPY(dest,src,n) strncpy(dest,src,n)
# define STRCAT strcat
# define STRNCAT strncat
# define STRTOK(STR,PAT,VAR) strtok(STR,PAT)
# define STRTOKVAR(VAR) ((void)0)
# define GETCWD getcwd
# define GETENV(VAR) getenv(VAR)
# define CHDIR chdir
#endif

/* PATH_MAX is in ospath.h */
#define DRIVEMAX 2
#define LISTMAX 256

struct FilePath{
    char path[PATH_MAX+1]; /** the string version of the represented POSIX path */

#ifdef WINPATHS
    char drive[DRIVEMAX+1]; /** the drive the path resides on (field is absent in POSIX systems) */
#endif
};

#include <string.h>

#if !defined(ASC_NEW) || !defined (ASC_NEW_ARRAY) || !defined(ASC_FREE)
# error "where is ASC_NEW etc??"
# define ASC_FREE free
# define ASC_NEW(T) malloc(sizeof(T))
# define ASC_NEW_ARRAY(T,N) malloc((N)*sizeof(T))
#endif

#define E(MSG) fprintf(stderr,"%s:%d: (%s) ERROR: %s\n",__FILE__,__LINE__,__FUNCTION__,MSG)

#ifdef DO_FIXSLASHES
void ospath_fixslash(char *path);
#endif

struct FilePath *ospath_getcwd();

void ospath_copy(struct FilePath *dest, const struct FilePath *src);


#ifdef WINPATHS
/**
	This function splits out the drive letter in the path string, thus completing
	the correct construction of a FilePath object under Win32.
*/
void ospath_extractdriveletter(struct FilePath *);
#endif

#ifdef WINPATHS
# define PATH_SEPARATOR_STR  "\\"
# define PATH_SEPARATOR_CHAR  '\\'
# define PATH_LISTSEP_CHAR ';'
# define PATH_LISTSEP_STR ";"
# define PATH_WRONGSLASH_CHAR '/'
# define PATH_WRONGSLASH_STR "/"
#else
# define PATH_SEPARATOR_STR "/"
# define PATH_SEPARATOR_CHAR '/'
# define PATH_LISTSEP_CHAR ':'
# define PATH_LISTSEP_STR ":"
# define PATH_WRONGSLASH_CHAR '\\'
# define PATH_WRONGSLASH_STR "\\"
#endif

/**
	Create a new path structure from a string
*/
struct FilePath *ospath_new(const char *path){
	struct FilePath *fp;
	fp = ospath_new_noclean(path);

#ifdef DO_FIXSLASHES
	ospath_fixslash(fp->path);
#endif

	ospath_cleanup(fp);

	return fp;
}



/** Create but with no 'cleanup', and no fixing of / vs \. */
struct FilePath *ospath_new_noclean(const char *path){
	struct FilePath *fp = ASC_NEW(struct FilePath);
	P(fp);
	X(path);
	STRNCPY(fp->path,path,PATH_MAX);
	assert(strcmp(fp->path,path)==0);
#ifdef WINPATHS
	ospath_extractdriveletter(fp);
#endif

	return fp;
}

struct FilePath *ospath_new_expand_env(const char *path, GetEnvFn *getenvptr, int free_after_getenv){
	struct FilePath *fp;

	char *pathnew = env_subst(path,getenvptr,free_after_getenv);
	fp = ospath_new(pathnew);
	ASC_FREE(pathnew);

	return fp;
}

struct FilePath *ospath_new_copy(const struct FilePath *fp){
	struct FilePath *fp1 = ASC_NEW(struct FilePath);
	ospath_copy(fp1,fp);
	return fp1;
}

/**
	This function will serve to allow #include-style file paths
	to be specified with platform-independent forward slashes then
	translated into the local filesystem format for subsequent use.

	This method should be identical to ospath_new on posix, right?

	@NOTE: on windows, we also want:
				    C:dir/file --> c:$PWD\dir\file
	                  e:/hello --> e:\hello
	                 here/i/am --> here\i\am

	@NOTE:
	A path-search function should create full file paths by
	appending relative file to each component of the search path
	then performing a callback on each one to determine if the
	match is OK or not.
*/
struct FilePath *ospath_new_from_posix(const char *posixpath){
	struct FilePath *fp;
	fp = ospath_new_noclean(posixpath);

#ifdef DO_FIXSLASHES
	ospath_fixslash(fp->path);
#endif

	ospath_cleanup(fp);

	return fp;
}

void ospath_free(struct FilePath *fp){
	if(fp!=NULL){
		P(fp);
		ASC_FREE(fp);
	}
	fp=NULL;
}

void ospath_free_str(char *str){
	ASC_FREE(str);
}


#ifdef DO_FIXSLASHES
void ospath_fixslash(char *path){

	char *p;
	char temp[PATH_MAX+1];
	int startslash;
	int endslash;
	STRTOKVAR(nexttok);

	strcpy(temp,path);temp[PATH_MAX]='\0';


	startslash = (strlen(temp) > 0 && temp[0] == PATH_WRONGSLASH_CHAR);
	endslash = (strlen(temp) > 1 && temp[strlen(temp) - 1] == PATH_WRONGSLASH_CHAR);

	/* reset fp->path as required. */
	STRNCPY(path, (startslash ? PATH_SEPARATOR_STR : ""), PATH_MAX);

	for(p = STRTOK(temp, PATH_WRONGSLASH_STR,nexttok);
			p!=NULL;
			p = STRTOK(NULL,PATH_WRONGSLASH_STR,nexttok)
	){
		/* add a separator if we've already got some stuff */
		if(
			strlen(path) > 0
			&& path[strlen(path) - 1] != PATH_SEPARATOR_CHAR
		){
			STRCAT(path,PATH_SEPARATOR_STR);
		}

		STRCAT(path,p);
	}

	/* put / on end as required, according to what the starting path had */
	if(endslash && (strlen(path) > 0 ? (path[strlen(path) - 1] != PATH_SEPARATOR_CHAR) : 1))
	{

		STRCAT(path, PATH_SEPARATOR_STR);
	}

}
#endif

struct FilePath *ospath_getcwd(void){
	struct FilePath *fp;
	char *cwd;
	char temp[PATH_MAX];

	/* get current working directory */
	cwd = (char *)GETCWD(temp, PATH_MAX);

	/* create new path with resolved working directory */
	fp = ospath_new_noclean(cwd != NULL ? cwd : ".");

	D(fp);

	return fp;
}

int ospath_chdir(struct FilePath *fp){
	char *s = ospath_str(fp);
	X(s);
	int res = CHDIR(s);
	ASC_FREE(s);
	return res;
}

/**
	Use getenv() function to retrieve HOME path, or if not set, use
	the password database and try to retrieve it that way (???)
*/
struct FilePath *ospath_gethomepath(void){

	const char *pfx = (const char *)getenv("HOME");
	struct FilePath *fp;

#ifndef __WIN32__
# ifdef TRY_GETPWUID
	struct passwd *pw;

	if(pfx==NULL){
		 pw = (struct passwd*)getpwuid(getuid());

		if(pw){
			pfx = pw->pw_dir;
		}
	}
# endif
#endif

	/* create path object from HOME, but don't compress it
	 (because that would lead to an infinite loop) */
	fp = ospath_new_noclean(pfx ? pfx : "");

#ifdef DO_FIXSLASHES
	ospath_fixslash(fp->path);
#endif

	return fp;
}

#ifdef WINPATHS
void ospath_extractdriveletter(struct FilePath *fp)
{
	char *p;

	/* extract the drive the path resides on... */
	if(strlen(fp->path) >= 2 && fp->path[1] == ':')
	{
		STRNCPY(fp->drive,fp->path,2);
		fp->drive[2]='\0';
		for(p=fp->path+2; *p!='\0'; ++p){
			*(p-2)=*p;
		}
		*(p-2)='\0';
	}else{
		fp->drive[0] = '\0';
	}
}
#endif

void ospath_cleanup(struct FilePath *fp){
	char path[PATH_MAX];
	char *p;
	struct FilePath *home;
	struct FilePath *working;
	struct FilePath *parent;
	int startslash, endslash;
	STRTOKVAR(nexttok);

	/*  compress the path, and resolve ~ */
	startslash = (strlen(fp->path) > 0 && fp->path[0] == PATH_SEPARATOR_CHAR);
	endslash = (strlen(fp->path) > 1 && fp->path[strlen(fp->path) - 1] == PATH_SEPARATOR_CHAR);

	home = ospath_gethomepath();

	/* create a copy of fp->path. */
	STRCPY(path, fp->path);

	/* reset fp->path as required. */
	STRCPY(fp->path, (startslash ? PATH_SEPARATOR_STR : ""));

	D(fp);
	X(path);

	/* split path into it tokens, using STRTOK which is NOT reentrant
	 so be careful! */

	/*M("STARTING STRTOK"); */
	for(p = STRTOK(path, PATH_SEPARATOR_STR,nexttok);
			p!=NULL;
			p = STRTOK(NULL,PATH_SEPARATOR_STR,nexttok)
	){
		if(strcmp(p, "~")==0){

			if(p == path){ /* check that the ~ is the first character in the path */
				if(ospath_isvalid(home)){
					ospath_copy(fp,home);
					continue;
				}else{
					E("HOME does not resolve to valid path");
				}
			}else{
				E("A tilde (~) present as a component in a file path must be at the start!");
			}
		}else if(strcmp(p, ".") == 0){

			if(p==path){/* start of path: */
				M("EXPANDING LEADING '.' IN PATH");
				X(path);

				working = ospath_getcwd();

				D(working);
#ifdef WINPATHS
				X(working->drive);
#endif
				X(p);
				X(path);

				ospath_copy(fp,working);


				D(fp);
				X(p);
				X(path+strlen(p)+1);

				ospath_free(working);
				continue;
			}else{/* later in the path: just skip it */
				M("SKIPPING '.' IN PATH");
				continue;
			}

		}else if(strcmp(p, "..") == 0){
			M("GOING TO PARENT");
			parent = ospath_getparent(fp);
			if(ospath_isvalid(parent)){
				ospath_copy(fp,parent);
			}
			ospath_free(parent);
			continue;
		}

		/* add a separator if we've already got some stuff */
		if(
			strlen(fp->path) > 0
			&& fp->path[strlen(fp->path) - 1] != PATH_SEPARATOR_CHAR
		){
			STRCAT(fp->path,PATH_SEPARATOR_STR);
		}

		/* add the present path component */
		STRCAT(fp->path, p);
	}

	ospath_free(home);

	// put / on end as required, according to what the starting path had
	if(endslash && (strlen(fp->path) > 0 ? (fp->path[strlen(fp->path) - 1] != PATH_SEPARATOR_CHAR) : 1))
	{
		STRCAT(fp->path, PATH_SEPARATOR_STR);
	}
}


int ospath_isvalid(const struct FilePath *fp){
	if(fp==NULL) return 0;
	return strlen(fp->path) > 0 ? 1 : 0;
}


char *ospath_str(const struct FilePath *fp){
	char *s;
#ifdef WINPATHS
	s = ASC_NEW_ARRAY(char, strlen(fp->drive)+strlen(fp->path) +1);
	STRCPY(s,fp->drive);
	STRCAT(s,fp->path);
#else
	s = ASC_NEW_ARRAY(char, strlen(fp->path)+1);
	STRCPY(s,fp->path);
#endif
	return s;
}

void ospath_strncpy(struct FilePath *fp, char *dest, int destsize){
#ifdef WINPATHS
	STRNCPY(dest,fp->drive,destsize);
	STRNCAT(dest,fp->path,destsize-strlen(dest));
#else
	STRNCPY(dest,fp->path,destsize);
#endif
}

void ospath_strcat(struct FilePath *fp, char *dest, int destsize){
	int remaining = destsize - strlen(dest);
	V(remaining);
#ifdef WINPATHS
	STRNCAT(dest,fp->drive,remaining);
	STRNCAT(dest,fp->path,remaining-strlen(dest));
#else
	STRNCAT(dest,fp->path,remaining);
#endif
	D(fp);
}

void ospath_fwrite(struct FilePath *fp, FILE *dest){
#ifdef WINPATHS
	fprintf(dest,"%s%s",fp->drive,fp->path);
#else
	fprintf(dest,"%s",fp->path);
#endif
}

unsigned ospath_length(struct FilePath *fp){
#ifdef  WINPATHS
	/* we've already validated this path, so it's on to just add it up
	 (unless someone has been tinkering with the internal structure here)*/
	return (unsigned) (strlen(fp->drive) + strlen(fp->path));
#else
	return (unsigned) (strlen(fp->path));
#endif
}

struct FilePath *ospath_getparent(struct FilePath *fp)
{
	int length;
	int offset;
	char *pos;
	ptrdiff_t len1;
#ifdef OSPATH_DEBUG
	int len2;
#endif
	char sub[PATH_MAX];
	struct FilePath *fp1, *fp2;

#ifdef OSPATH_DEBUG
	fprintf(stderr,"finding parent of %s\n",fp->path);
#endif
	D(fp);

	if(strlen(fp->path) == 0){
		fp1 = ospath_getcwd();
		fp2 = ospath_getparent(fp1);
		ospath_free(fp1);
		return fp2;
	}else if(ospath_isroot(fp)){
#ifdef OSPATH_DEBUG
		fprintf(stderr,"path is root\n");
#endif
		/* stay at root */
		return ospath_new("/");
	}

	/* reverse find a / ignoring the end / if it exists.
	/// FIXME */
	length = strlen(fp->path);
	offset = (
			fp->path[length - 1] == PATH_SEPARATOR_CHAR /* last char is slash?*/
			&& length > 1 /* and more than just leading slash... */
		) ? length - 1 : length; /* then remove last char */

	for(pos = fp->path + offset - 1; *pos!=PATH_SEPARATOR_CHAR && pos>=fp->path; --pos){
#ifdef OSPATH_DEBUG
		CONSOLE_DEBUG("CURRENT CHAR: %c\n",*pos);
#endif
	}

	len1 = pos - (fp->path);
#ifdef OSPATH_DEBUG
	len2 = (int)len1;
	V(len2);
#endif
	/*fprintf(stderr,"POS = %d\n",len2);*/

	if(*pos==PATH_SEPARATOR_CHAR){
#ifdef WINPATHS
		STRCPY(sub,fp->drive);
		STRNCAT(sub,fp->path,len1);
#else
		STRNCPY(sub,fp->path,len1);
		sub[len1]='\0';
#endif
		X(sub);
		if(strcmp(sub,"")==0){
			M("DIRECTORY IS EMPTY");
			STRCAT(sub,PATH_SEPARATOR_STR);
		}
	}else{
		/* ie pos<fp->path */
		M("NO PARENT DIR");
		if(fp->path[0]==PATH_SEPARATOR_CHAR){
			E("NO PARENT DIR");
			return ospath_new_noclean(fp->path);
		}else{
			M("RETURNING '.'");
#ifdef WINPATHS
			/* eg 'c:bin' --> 'c:' */
			STRCPY(sub,fp->drive);
			return ospath_new_noclean(sub);
#else
			/* eg 'bin' --> '.' */
			return ospath_new_noclean(".");
#endif
		}
	}

	M("Creating 'sub'");
	fp1 = ospath_new_noclean(sub);
	M("... 'sub'");
	D(fp1);
	return fp1;
}

#define ospath_gpadn_used 0
#if ospath_gpadn_used
// this function not yet in use
struct FilePath *ospath_getparentatdepthn(struct FilePath *fp, unsigned depth)
{
	int startslash;
	char path[PATH_MAX];
	char *temp;
	char *p;
	STRTOKVAR(nexttok);
#ifdef WINPATHS
	char temp2[PATH_MAX];
#endif

	if(
		!ospath_isvalid(fp)
		|| depth >= ospath_depth(fp)
	){
		return fp;
	}

	/* create FilePath object to parent object at depth N relative to this
	 path object.*/
	startslash = (strlen(fp->path) > 0 && fp->path[0] == PATH_SEPARATOR_CHAR);

	/* create a copy of fp->path.*/
	STRCPY(path, fp->path);

	/* reset fp->path as required.*/
	temp = startslash ? PATH_SEPARATOR_STR : "";

	/* split path into it tokens.
	M("STARTING STRTOK");*/
	p = STRTOK(path, PATH_SEPARATOR_STR, nexttok);

	while(p && depth > 0)
	{
		if(strlen(temp) > 0 && temp[strlen(temp) - 1] != PATH_SEPARATOR_CHAR)
		{
			strcat(temp,PATH_SEPARATOR_STR);
		}

		STRCAT(temp,p);
		--depth;

		p = STRTOK(NULL, PATH_SEPARATOR_STR, nexttok);
	}

	/* put / on end as required*/
	if(strlen(temp) > 0 ? (temp[strlen(temp) - 1] != PATH_SEPARATOR_CHAR) : 1)
	{
		strcat(temp,PATH_SEPARATOR_STR);
	}

#ifdef WINPATHS
	STRCPY(temp2,fp->drive);
	STRCAT(temp2,temp);
	return ospath_new_noclean(temp2);
#else
	return ospath_new_noclean(temp);
#endif
}
#endif

char *ospath_getbasefilename(struct FilePath *fp){
	char *temp;
	unsigned length	;
	char *pos;

	if(fp==NULL)return NULL;

	if(strlen(fp->path) == 0){
		/* return empty name.*/
		return "";
	}

	if(fp->path[strlen(fp->path)-1]==PATH_SEPARATOR_CHAR){
		return NULL;
	}

	/* reverse find '/' but DON'T ignore a trailing slash*/
	/* (this is changed from the original implementation)*/
	length = strlen(fp->path);

	pos = strrchr(fp->path, PATH_SEPARATOR_CHAR);

	/* extract filename given position of find / and return it.*/
	if(pos != NULL){
		unsigned length1 = length - ((pos - fp->path));
		temp = ASC_NEW_ARRAY(char, length1+1);

		V(length1);
		STRNCPY(temp, pos + 1, length1);
		*(temp + length1)='\0';
		return temp;
	}else{
		temp = ASC_NEW_ARRAY(char, length+1);
		strcpy(temp, fp->path);
		*(temp+length)='\0';
		return temp;
	}
}

char *ospath_getfilestem(struct FilePath *fp){
	char *temp;
	char *pos;

	if(!ospath_isvalid(fp)){
		return NULL;
	}

	temp = ospath_getbasefilename(fp);
	if(temp==NULL){
		/* it's a directory*/
		return NULL;
	}

	pos = strrchr(temp,'.');

	if(pos==NULL || pos==temp){
		/* no extension, or a filename starting with '.'
		 -- return the whole filename*/
		return temp;
	}

	/* remove extension.*/
	*pos = '\0';

	return temp;
}

char *ospath_getfileext(struct FilePath *fp){
	char *temp, *temp2, *pos;
	int len1;

	if(!ospath_isvalid(fp)){
		return NULL;
	}

	temp = ospath_getbasefilename(fp);
	if(temp==NULL){
		/* it's a directory*/
		return NULL;
	}

	/* make sure there is no / on the end.
	 FIXME: is this good policy, removing a trailing slash?*/
	if(temp[strlen(temp) - 1] == PATH_SEPARATOR_CHAR){
		temp[strlen(temp)-1] = '\0';
	}

	pos = strrchr(temp,'.');

	if(pos != NULL && pos!=temp){
		/* extract extension.*/
		len1 = temp + strlen(temp) - pos + 1;
		temp2 = ASC_NEW_ARRAY(char,len1);
		STRNCPY(temp2, pos, len1);
	}else{
		/* no extension*/
		temp2 = NULL;
	}
	ASC_FREE(temp);
	return temp2;
}

int ospath_isroot(struct FilePath *fp)
{
	if(!ospath_isvalid(fp)){
#ifdef OSPATH_DEBUG
		fprintf(stderr,"path is invalid\n");
#endif
		return 0;
	}

	return strcmp(fp->path, PATH_SEPARATOR_STR) ? 0 : 1;
}

#define ospath_depth_used 0
#if ospath_depth_used
// disused, so far
unsigned ospath_depth(struct FilePath *fp){
	unsigned length;
	unsigned depth;
	unsigned i;

	length = strlen(fp->path);
	depth = 0;

	for(i = 0; i < length; i++){
		if(fp->path[i] == PATH_SEPARATOR_CHAR){
			++depth;
		}
	}

	if(
		depth > 0
		&& length > 0
		&& fp->path[length - 1] == PATH_SEPARATOR_CHAR
	){
		/* PATH_SEPARATOR_CHAR on the end, reduce count by 1*/
		--depth;
	}

	return depth;
}
#endif

struct FilePath *ospath_root(struct FilePath *fp){
#ifdef WINPATHS
	char *temp;
	struct FilePath *r;

	if(strlen(fp->drive)){
		temp = ASC_NEW_ARRAY(char, strlen(fp->drive)+1);
		STRCPY(temp,fp->drive);
		STRCAT(temp,PATH_SEPARATOR_STR);
		X(temp);
		r = ospath_new(temp);
		ASC_FREE(temp);
	}else{
		r = ospath_new(fp->path);
	}
	return r;
#else
	(void)fp;
	return ospath_new(PATH_SEPARATOR_STR);
#endif
}

struct FilePath *ospath_getdir(struct FilePath *fp){
	char *pos;
	char s[PATH_MAX];
#ifdef WINPATHS
	int e;
#endif

	pos = strrchr(fp->path,PATH_SEPARATOR_CHAR);
	if(pos==NULL){
		return ospath_new_noclean("");
	}
#ifdef WINPATHS
	strncpy(s,fp->drive,PATH_MAX);
	e = strlen(s);
	strncat(s,fp->path,pos - fp->path);
	s[e+pos-fp->path]='\0';
#else
	strncpy(s,fp->path,pos - fp->path);
	s[pos-fp->path]='\0';
	/* CONSOLE_DEBUG("DIRECTORY: '%s'",s); */
#endif
	return ospath_new(s);
}

ASC_DLLSPEC struct FilePath *ospath_getabs(const struct FilePath *fp){
	struct FilePath *fp1, *fp2;
	if(fp->path[0]==PATH_SEPARATOR_CHAR){
		fp1 = ospath_new_copy(fp);
	}else{
		fp2 = ospath_new(".");
		fp1 = ospath_concat(fp2,fp);
		ospath_free(fp2);
	}
	return fp1;
}

int ospath_cmp(struct FilePath *fp1, struct FilePath *fp2){
	char temp[2][PATH_MAX];
#ifdef WINPATHS
	char *p;
	struct FilePath *fp;
#endif

	if(!ospath_isvalid(fp1)){
		if(!ospath_isvalid(fp2)){
			return 0;
		}else{
			return -1;
		}
	}else if(!ospath_isvalid(fp2)){
		return 1;
	}

#ifdef WINPATHS
	if(strcmp(fp1->drive,"")==0){
		M("PATH IS MISSING DRIVE LETTER");
		D(fp1);
		fp = ospath_getcwd();
		assert(strlen(fp->drive)!=0);
		X(fp->drive);
		STRCPY(temp[0],fp->drive);
		ospath_free(fp);
	}else{
		STRCPY(temp[0],fp1->drive);
	}

	if(strcmp(fp2->drive,"")==0){
		M("PATH IS MISSING DRIVE LETTER");
		D(fp2);
		fp = ospath_getcwd();
		assert(strlen(fp->drive)!=0);
		X(fp->drive);
		STRCPY(temp[1],fp->drive);
		ospath_free(fp);
	}else{
		STRCPY(temp[1],fp2->drive);
	}

	STRCAT(temp[0],fp1->path);
	STRCAT(temp[1],fp2->path);
#else
	STRCPY(temp[0], fp1->path);
	STRCPY(temp[1], fp2->path);
#endif

#ifdef WINPATHS
	X(temp[0]);
	for(p=temp[0];*p!='\0';++p){
		*p=tolower(*p);
	}
	X(temp[1]);
	for(p=temp[1];*p!='\0';++p){
		*p=tolower(*p);
	}
	X(temp[0]);
	X(temp[1]);
#endif

	/* we will count two paths that different only in a trailing slash to be the *same*
	 so we add trailing slashes to both now: */
	if(temp[0][strlen(temp[0]) - 1] != PATH_SEPARATOR_CHAR){
		STRCAT(temp[0],PATH_SEPARATOR_STR);
	}

	if(temp[1][strlen(temp[1]) - 1] != PATH_SEPARATOR_CHAR){
		STRCAT(temp[1],PATH_SEPARATOR_STR);
	}


	return strcmp(temp[0],temp[1]);
}

struct FilePath *ospath_concat(const struct FilePath *fp1, const struct FilePath *fp2){

	struct FilePath *fp;
	char temp[2][PATH_MAX+1];
	char temp2[2*PATH_MAX+1];
	struct FilePath *r;

	X(fp1->path);
	X(fp2->path);
#ifdef WINPATHS
	X(fp1->drive);
	X(fp2->drive);
#endif

	fp = ASC_NEW(struct FilePath);

	if(!ospath_isvalid(fp1)){
		if(ospath_isvalid(fp2)){
			ospath_copy(fp,fp2);
		}else{
			/* both invalid*/
			ospath_copy(fp,fp1);
		}
		return fp;
	}

	if(!ospath_isvalid(fp2)){
		ospath_copy(fp,fp1);
		return fp;
	}

	/* not just a copy of one or the other...*/
	ospath_free(fp);

	/* now, both paths are valid...*/

#ifdef WINPATHS
	strcpy(temp[0],fp1->drive);
	STRNCAT(temp[0],fp1->path,PATH_MAX-strlen(temp[0]));
#else
	strcpy(temp[0], fp1->path);
#endif

	strcpy(temp[1], fp2->path);

	/* make sure temp has a / on the end. */
	if(temp[0][strlen(temp[0]) - 1] != PATH_SEPARATOR_CHAR){
		STRNCAT(temp[0],PATH_SEPARATOR_STR,PATH_MAX-strlen(temp[0]));
	}

#ifdef DO_FIXSLASHES
	ospath_fixslash(temp[0]);
	ospath_fixslash(temp[1]);
#endif

#if 1
	size_t s0 = strlen(temp[0]);
	size_t s1 = strlen(temp[1]);
	V(s0);
	X(temp[0]);
	V(s1);
	X(temp[1]);
#endif

	/* make sure rhs path has NOT got a / at the start. */
	if(temp[1][0] == PATH_SEPARATOR_CHAR){
		return NULL;
	}

	/* create a new path object with the two path strings appended together.*/
	strcpy(temp2,temp[0]);
	if (s1 >= (PATH_MAX - s0)) {
		return NULL;
	}
	strncpy(temp2+s0, temp[1], PATH_MAX-s0);
#if 1
	V(strlen(temp2));
	X(temp2);
#endif
	r = ospath_new_noclean(temp2);
	D(r);

	/* ospath_cleanup(r);*/
	return r;
}

void ospath_append(struct FilePath *fp, struct FilePath *fp1){
	char *p;
	char temp[2][PATH_MAX+1];
	struct FilePath fp2;

	ospath_copy(&fp2,fp1);
#ifdef DO_FIXSLASHES
	ospath_fixslash(fp2.path);
#endif

	if(!ospath_isvalid(&fp2)){
		M("fp1 invalid");
		return;
	}

	if(!ospath_isvalid(fp) && ospath_isvalid(&fp2)){
		/* set this object to be the same as the rhs */
		M("fp invalid");
		ospath_copy(fp,&fp2);
#ifdef DO_FIXSLASHES
		ospath_fixslash(fp->path);
#endif
		return;
	}

	X(fp->path);
	X(fp2.path);

	/* both paths are valid...*/
	strcpy(temp[0], fp->path);
	strcpy(temp[1], fp2.path);

	X(temp[0]);
	X(temp[1]);

	/* make sure temp has a / on the end. */
	if(temp[0][strlen(temp[0]) - 1] != PATH_SEPARATOR_CHAR)
	{
		STRCAT(temp[0],PATH_SEPARATOR_STR);
	}

	/* make sure rhs path has NOT got a / at the start. */
	if(temp[1][0] == PATH_SEPARATOR_CHAR){
		for(p=temp[1]+1; *p != '\0'; ++p){
			*(p-1)=*p;
		}
		*(p-1)='\0';
	}

	X(temp[0]);
	X(temp[1]);

	size_t s0 = strlen(temp[0]);
	size_t s1 = strlen(temp[1]);
	/*create new path string.*/
	strcpy(fp->path, temp[0]);
	if (s1 >= (PATH_MAX - s0)) {
		M("ospath_append: result bigger than PATH_MAX truncated\n");
		return;
	}
	strncpy(fp->path + s0, temp[1], PATH_MAX-s0);

	X(fp->path);

	D(fp);
	ospath_cleanup(fp);
}

void ospath_copy(struct FilePath *dest, const struct FilePath *src){
	STRCPY(dest->path,src->path);
#ifdef WINPATHS
	STRCPY(dest->drive,src->drive);
#endif
}

void ospath_debug(struct FilePath *fp){
#ifdef WINPATHS
	fprintf(stderr,"{\"%s\",\"%s\"}\n",fp->drive,fp->path);
#else
	fprintf(stderr,"{\"%s\"}\n",fp->path);
#endif
}

FILE *ospath_fopen(struct FilePath *fp, const char *mode){
	char s[PATH_MAX];
	FILE *f;

	if(!ospath_isvalid(fp)){
		E("Invalid path");
		return NULL;
	}
	ospath_strncpy(fp,s,PATH_MAX);
	f = fopen(s,mode);
	return f;
}

int ospath_stat(struct FilePath *fp,ospath_stat_t *buf){
	char s[PATH_MAX];

	if(!ospath_isvalid(fp)){
		E("Invalid path");
		return -1;
	}
	ospath_strncpy(fp,s,PATH_MAX);
	return STAT(s,buf);
}

/*------------------------
 SEARCH PATH FUNCTIONS
*/

struct FilePath **ospath_searchpath_new(const char *path){
	char *p;
	char *list[LISTMAX];
	unsigned n=0;
	char *c;
	unsigned i;
	struct FilePath **pp;

#if PATH_MAX < 4096
# define SEARCHPATH_MAX 4096
#else
# define SEARCHPATH_MAX PATH_MAX
#endif

	if(strlen(path) > SEARCHPATH_MAX){
		E("Search path is too long, increase SEARCHPATH_MAX in ospath code");
		return NULL;
	}

	char path1[SEARCHPATH_MAX+1];

	STRTOKVAR(nexttok);

	strncpy(path1,path,SEARCHPATH_MAX);

	X(path1);
	X(PATH_LISTSEP_STR);

	V(strlen(path1));
	V(strlen(PATH_LISTSEP_STR));

	/*
	c = strstr(path,PATH_LISTSEP_CHAR);
	if(c==NULL){
		E("NO TOKEN FOUND");
	}
	*/

	p=STRTOK(path1,PATH_LISTSEP_STR,nexttok);
	X(p);
	for(; p!= NULL; p=STRTOK(NULL,PATH_LISTSEP_STR,nexttok)){
		c = ASC_NEW_ARRAY(char,strlen(p)+1);
		/* XXX FIXME what if one path component is longer than PATH_MAX?*/
		X(p);
		STRCPY(c,p);
		if(n>=LISTMAX){
			E("IGNORING SOME PATH COMPONENTS");
			break;
		}
		list[n++]=c;
	}


	for(i=0;i<n;++i){
		X(list[i]);
	}
	V(n);


	pp = ASC_NEW_ARRAY(struct FilePath*, n+1);
	for(i=0; i<n; ++i){
		pp[i] = ospath_new_noclean(list[i]);
		ASC_FREE(list[i]);
	}
	pp[n] = NULL;

	for(i=0;i<n;++i){
#ifdef DO_FIXSLASHES
		ospath_fixslash(pp[i]->path);
#endif
		D(pp[i]);
	}

	return pp;
}

void ospath_searchpath_free(struct FilePath **searchpath){
	struct FilePath **p;
	for(p=searchpath; *p!=NULL; ++p){
		ospath_free(*p);
	}
	ASC_FREE(searchpath);
}

int ospath_searchpath_length(struct FilePath **searchpath){
	int i=0;
	struct FilePath **p;
	for(p=searchpath; *p!=NULL; ++p){
		++i;
	}
	return i;
}

struct FilePath *ospath_searchpath_iterate(
		struct FilePath **searchpath
		, FilePathTestFn *testfn
		, void *searchdata
){
	struct FilePath **p;

	p = searchpath;

	M("SEARCHING IN...");
	for(p=searchpath; *p!=NULL; ++p){
		D(*p);
	}

	for(p=searchpath; *p!=NULL; ++p){
		D(*p);
		assert(*p!=NULL);
		if((*testfn)(*p,searchdata)){
			return *p;
		}
	}
	return NULL;
}
