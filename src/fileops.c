/*
 * GQview image viewer
 * (C)2000 John Ellis
 *
 * Author: John Ellis
 *
 */

#include "gqview.h"

/*
 *-----------------------------------------------------------------------------
 * generic file information and manipulation routines (public)
 *-----------------------------------------------------------------------------
 */ 

/* first we try the HOME environment var, if that doesn't work, we try getpwuid(). */
gchar *homedir()
{
	gchar *home = getenv("HOME");
	if(home)
		return home;
	else
		{
		struct passwd *pw = getpwuid(getuid());
		if (pw)
			return pw->pw_dir;
		else
			return NULL ; /* now we've got a problem */
		}
}

int isfile(char *s)
{
   struct stat st;
   
   if ((!s)||(!*s)) return 0;
   if (stat(s,&st)<0) return 0;
   if (S_ISREG(st.st_mode)) return 1;
   return 0;
}

int isdir(char *s)
{
   struct stat st;
   
   if ((!s)||(!*s)) return 0;
   if (stat(s,&st)<0) return 0;
   if (S_ISDIR(st.st_mode)) return 1;
   return 0;
}

int filesize(char *s)
{
   struct stat st;
   
   if ((!s)||(!*s)) return 0;
   if (stat(s,&st)<0) return 0;
   return (int)st.st_size;
}

time_t filetime(gchar *s)
{
        struct stat st;

        if ((!s)||(!*s)) return 0;
        if (stat(s,&st)<0) return 0;
        return st.st_mtime;
}

int copy_file(char *s, char *t)
{
	FILE *fi, *fo;
	char buf[4096];
	int b;

	fi = fopen(s, "rb");
	if (!fi)
		{
		return FALSE;
		}

	fo = fopen(t, "wb");
	if (!fo)
		{
		fclose(fi);
		return FALSE;
		}

	while((b = fread(buf, sizeof(char), 4096, fi)) && b != 0)
		{
		if (fwrite(buf, sizeof(char), b, fo) != b)
			{
			fclose(fi);
			fclose(fo);
			return FALSE;
			}
		}

	fclose(fi);
	fclose(fo);
	return TRUE;
}

int move_file(char *s, char *t)
{
	if (rename (s, t) < 0)
		{
		/* this may have failed because moving a file across filesystems
		was attempted, so try copy and delete instead */
		if (copy_file(s, t))
			{
			if (unlink (s) < 0)
				{
				/* err, now we can't delete the source file so return FALSE */
				return FALSE;
				}
			}
		else
			return FALSE;
		}

	return TRUE;
}

gchar *get_current_dir()
{
	char buf[512];
	if (getcwd(buf, 510) == NULL)
		{
#ifdef __USE_GNU
		char *dbuf;
		gchar *ret;
		dbuf = get_current_dir_name();
		if (buf)
			{
			ret = g_strdup(dbuf);	/* don't mix free w/ g_free */
			free(dbuf);
			return (ret);
			}
#endif
		return (g_strdup("."));		/* well, return something! broken? */
		}
	return g_strdup(buf);
}


