
#include <stdlib.h>
#include <stdio.h>
#ifndef DINGOO_NATIVE
    #include <dirent.h>
#endif
#include <string.h>
#include <ctype.h>

#include <unistd.h> /* test! for chdir etc. */


#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <time.h>

#include "gnuboy/Version"

#include "gnuboy.h"

#include "fb.h"
#include "ubytegui/gui.h"
#include "ubytegui/dialog.h"
#include "input.h"
#include "hw.h"
#include "rc.h"
#include "loader.h"
#include "mem.h"
#include "sound.h"
#include "lcd.h"


#ifdef DINGOO_NATIVE
/* NOTE this probably doesn't work (related to save slots maybe?) */
struct stat {
    mode_t    st_mode;    /* protection */
    time_t    st_mtime;   /* time of last modification */
};
int stat(const char *path, struct stat *buf);
int stat(const char *path, struct stat *buf)
{
    /* NOOP */
    buf->st_mode = 0;
    buf->st_mtime = 0;
}
/* int fstat(int filedes, struct stat *buf); */

/* linux stat.h */
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_IFMT  00170000
#define S_IFDIR  0040000


int chdir(const char *path);
int chdir(const char *path)
{
    /* NOOP */
    return 0;
}

char *getcwd(char *buf, size_t size);
char *getcwd(char *buf, size_t size)
{
    char *x=buf;
    strcpy(buf, "b:\\");
    /*
    x[0]='\0';
    */
    return x;
}

char *ctime(const time_t *timep);
char *ctime(const time_t *timep)
{
    char *x="not_time";
    return x;
}


/* Dingoo native SDK temporary workaround dirent.h header start */
struct dirent {
	ino_t d_ino;
	unsigned int d_type; /* NOTE using the same type that fsys uses for file type mask; unsigned int  attributes;*/
	char* d_name;
};

#define DT_DIR FSYS_ATTR_DIR
#define DT_REG FSYS_ATTR_FILE /* Not sure if this is a suitable mapping */

typedef void DIR;

extern DIR* opendir(const char *name);
extern int  closedir(DIR* dir);

extern struct dirent* readdir(DIR* dir);
/* header end */


/* code start */
    FILE *myout=NULL;


typedef struct {
	fsys_file_info_t info;
	int              type;
	char*            path;
	bool             eod;
	struct dirent    cur_entry;
	bool             was_read;
	long             offset;
} _dir_t;



extern char* _app_path;
extern char* _file_path(const char* inPath);



DIR* opendir(const char* name) {
	char* tempPath = NULL;
	_dir_t* tempDir = NULL;
	int tmplen = 0;
	char *tmpstrptr=NULL;


	tempPath = _file_path(name);
	if(tempPath == NULL)
		return NULL;

	tempDir = (_dir_t*)malloc(sizeof(_dir_t));
	if(tempDir == NULL) {
		free(tempPath);
		return NULL;
	}

	tempDir->path   = tempPath;
	tempDir->eod    = false;
	tempDir->offset = 0;
	tempDir->type   = -1; /* All object types */

	char tempMask[strlen(tempDir->path) + 3];
    /* FIXME I'm not keen on stack allocations mid function this is C not C++! */
    
	/* Make sure there are no trailing slashes fsys_findfirst() won't find paths with multiple slashes */
	tmplen = strlen(tempMask);
	strcpy(tempMask, tempDir->path);
	tmplen = strlen(tempMask);
	tmpstrptr = &tempMask[tmplen-1];
	while (*tmpstrptr == '\\') *tmpstrptr-- = '\0';
	strcat(tempMask, "\\*");

    
	if(fsys_findfirst(tempMask, tempDir->type, &(tempDir->info)) != 0) {
		tempDir->eod = true;
	}

	if(!tempDir->eod) {
		/* TODO hide attributes & FSYS_ATTR_DISKLABEL ?*/
		tempDir->cur_entry.d_ino  = tempDir->info.handle;
		tempDir->cur_entry.d_name = tempDir->info.name;
		tempDir->cur_entry.d_type = tempDir->info.attributes;
	}
	tempDir->was_read = false;

	return (DIR*)tempDir;
}

int closedir(DIR* dir) {
	_dir_t* tempDir = (_dir_t*)dir;

	if(dir == NULL)
		return -1;

	if(!tempDir->eod)
		fsys_findclose(&(tempDir->info));
	if(tempDir->path != NULL)
		free(tempDir->path);
	free(tempDir);

	return 0;
}



struct dirent* readdir(DIR* dir) {
	_dir_t* tempDir = (_dir_t*)dir;

	if(dir == NULL)
		return NULL;

	if(tempDir->eod)
		return NULL;

	if(!tempDir->was_read) {
		tempDir->was_read = true;
		return &(tempDir->cur_entry);
	}

	if(fsys_findnext(&(tempDir->info)) != 0) {
		tempDir->eod = true;
		return NULL;
	}

	tempDir->offset++;
	/* TODO hide attributes & FSYS_ATTR_DISKLABEL ?*/
	tempDir->cur_entry.d_ino  = tempDir->info.handle;
	tempDir->cur_entry.d_name = tempDir->info.name;
	tempDir->cur_entry.d_type = tempDir->info.attributes;

	return &(tempDir->cur_entry);
}

/* code end */

#endif /* DINGOO_NATIVE */

/* Probably DINGOO_NATIVE too... */
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif /* PATH_MAX */


char *menu_getext(char *s){
	char* ext = NULL;
	while(*s){
		if(*s++ == '.') ext=s;
	}

	return ext;
}

int filterfile(char *p, char *exts){
	char* ext;
	char* cmp;
	int n,i;

	if(exts==NULL) return 1;

	ext = exts;
	cmp = menu_getext(p);

	if(cmp==NULL) return 0;

	while(*ext != 0){
		n=0;
		while(*exts++ != ';'){
			n++;
			if(*exts==0) break;
		}

		i=0;
		while(tolower(cmp[i])==tolower(ext[i])){
			i++;
			if((i==n) && (cmp[i]==0)) return 1;
			if((i==n) || (cmp[i]==0)) break;
		}
		ext = exts;
	}
	return 0;
}

int fcompare(const void *a, const void *b){
	char *stra = *(char**)a;
	char *strb = *(char**)b;
	int cmp;
	return strcmp(stra,strb);
}

char* menu_browsedir(char* fpathname, char* file, char *title, char *exts){
    /* this routine has side effects with fpathname and file, FIXME */
	DIR *dir;
	struct dirent *d;
	struct stat s;
	int n=0, i, j;
	char *files[1<<16];  /* 256Kb */
#ifndef  DT_DIR
	char tmpfname[PATH_MAX];
	char *tmpfname_end;
#endif /* DT_DIR */


	if(!(dir = opendir(fpathname))) return NULL;

	files[n] = malloc(4);
	strcpy(files[n], "..");
	strcat(files[n], DIRSEP);
	n++;

	d = readdir(dir);
	if(d && !strcmp(d->d_name,".")) d = readdir(dir);
	if(d && !strcmp(d->d_name,"..")) d = readdir(dir);

#ifndef  DT_DIR
	strcpy(tmpfname, fpathname);
	tmpfname_end = &tmpfname[0];
	tmpfname_end += strlen(tmpfname);
#endif /* DT_DIR */

	while(d){
#ifndef  DT_DIR
		/* can not lookup type from search result have to stat filename*/
		strcpy(tmpfname_end, d->d_name);
		stat(tmpfname, &s);
		if(S_ISDIR (s.st_mode))
#else
		if ((d->d_type & DT_DIR) == DT_DIR)
#endif /* DT_DIR */
		{
			files[n] = malloc(strlen(d->d_name)+2);
			strcpy(files[n], d->d_name);
			strcat(files[n], DIRSEP);
			n++;
		} else if(filterfile(d->d_name,exts)){
			files[n] = malloc(strlen(d->d_name)+1);
			strcpy(files[n], d->d_name);
			n++;
		}
		d  = readdir(dir);
	}
	closedir (dir);
	qsort(files+1,n-1,sizeof(char*),fcompare);

	dialog_begin(title, fpathname);

	for(i=0; i<n; i++){
		dialog_text(files[i],"",FIELD_SELECTABLE);
	}

	if(j = dialog_end()){
		if(file) {
			strcpy(file,files[j-1]);
		} else {
			file = files[j-1];
			files[j-1] = NULL;
		}
	}

	for(i=0; i<n; i++){
		free(files[i]);
	}

	return j ? file : NULL;

}

char* menu_requestfile(char* file, char *title, char* path, char *exts){
	char *dir;
	int allocmem = file == NULL;
	int tmplen = 0;
    /* TODO clear all the dyanamic memory allocations in this routine and require caller to pre-allocate */

	if (allocmem) file = malloc(PATH_MAX);
#ifdef DEBUG_ALWAYS_RETURN_ADJUSTRIS_GB
    strcpy(file, "adjustris.gb");
    return file;
#endif /* DEBUG_ALWAYS_RETURN_ADJUSTRIS_GB */
	if(path)
	{
		strcpy(file, path);
		tmplen = strlen(file);
		if (tmplen >=1)
		{
			if (file[tmplen-1] != DIRSEP_CHAR)
				strcat(file, DIRSEP); /* this is very fragile, e.g. what if dir was specified but does not exist */
		}
	}
	else
		strcpy(file, "");

	while(dir = menu_browsedir(file, file+strlen(file),title,exts)){
		/*
		** Check to see if we have a file name,
		** or a new directory name to scan
		** Directory name will have trailing path sep character
		** FIXME check for "../" and dirname the dir
		*/
		tmplen = strlen(dir);
		if (tmplen >=1)
			if (dir[tmplen-1] != DIRSEP_CHAR)
			{
				if (allocmem) realloc(file, strlen(file)+1);
				break;
			}
	}
	if(!dir) file = NULL; /* FIXME what it file was not null when function was called */

	return file;
}

char *menu_requestdir(const char *title, const char *path){
	char *dir=NULL, *wd, **ldirs;
	int ldirsz, ldirn=0, ret, l;
	DIR *cd;
	struct dirent *d;
	struct stat s;
	char *cdpath;

	wd = malloc(PATH_MAX);
	getcwd(wd,PATH_MAX);

	if(path) chdir(path);

	cdpath = malloc(PATH_MAX);
	getcwd(cdpath,PATH_MAX);

	while(!dir){

		getcwd(cdpath,PATH_MAX);

		cd = opendir(".");

		sys_sanitize(cdpath);

		dialog_begin(title, cdpath);
		dialog_text("[Select This Directory]",NULL,FIELD_SELECTABLE);
		dialog_text("/.. Parent Directory",NULL,FIELD_SELECTABLE);

		ldirsz = 16;
		ldirs = malloc(sizeof(char*)*ldirsz);

		d = readdir(cd);
		if(d && !strcmp(d->d_name,".")) d = readdir(cd);
		if(d && !strcmp(d->d_name,"..")) d = readdir(cd);

		while(d){

			if(ldirn >= ldirsz){
				ldirsz += 16;
				ldirs = realloc(ldirs,ldirsz*sizeof(char*));
			}

			stat(d->d_name,&s);

			if(S_ISDIR (s.st_mode))
			{
				l = strlen(d->d_name);
				ldirs[ldirn] = malloc(l+2);
				ldirs[ldirn][0] = DIRSEP_CHAR;
				strcpy(ldirs[ldirn]+1, d->d_name);

				dialog_text(ldirs[ldirn],NULL,FIELD_SELECTABLE);
				ldirn++;
			}

			d = readdir(cd);
		}
		closedir(cd);

		switch(ret=dialog_end()){
			case 0:
				dir = (char*)-1;
				break;
			case 1:
				dir = strdup(cdpath);
				break;
			case 2:
				chdir("..");
				break;
			default:
				chdir(ldirs[ret-3]+1);
				break;
		}

		while(ldirn)
			free(ldirs[--ldirn]);
		free(ldirs);
	}

	if(dir==(char*)-1)
		dir = NULL;


	chdir(wd);
	free(wd);
	free(cdpath);

	return dir;
}

static const char *slots[] = {"Slot 0","Slot 1","Slot 2","Slot 3","Slot 4","Slot 5","Slot 6","Slot 7",NULL};
static const char *emptyslot = "<Empty>";

int menu_state(int save){

	char *statebody[8];
	char* name;

	int i, flags,ret, del=0,l;
	struct stat fstat;

	time_t time;
	char *tstr;

	char *savedir;
	char *savename;
	char *saveprefix;
	FILE *f;

	savedir = rc_getstr("savedir");
	savename = rc_getstr("savename");
#ifdef DEBUG_TO_STDOUT
	puts(savedir);
	puts(savename);
#endif /* DEBUG_TO_STDOUT */
	saveprefix = malloc(strlen(savedir) + strlen(savename) + 2);
	sprintf(saveprefix, "%s%s%s", savedir, DIRSEP, savename);

	dialog_begin(save?"Save State":"Load State",rom.name);

	for(i=0; i<8; i++){

		name = malloc(strlen(saveprefix) + 5);
		sprintf(name, "%s.%03d", saveprefix, i);

		if(!stat(name,&fstat)){
			time = fstat.st_mtime;
			tstr = ctime(&time);

			l = strlen(tstr);
			statebody[i] = malloc(l);
			strcpy(statebody[i],tstr);
			statebody[i][l-1]=0;
			flags = FIELD_SELECTABLE;
		} else{
			statebody[i] = (char*)emptyslot;
			flags = save ? FIELD_SELECTABLE : 0;
		}
		dialog_text(slots[i],statebody[i],flags);

		free(name);
	}

	if(ret=dialog_end()){
		name = malloc(strlen(saveprefix) + 5);
		sprintf(name, "%s.%03d", saveprefix, ret-1);
		if(save){
			if(f=fopen(name,"wb")){
				savestate(f);
				fclose(f);
			}
		}else{
			if(f=fopen(name,"rb")){
				loadstate(f);
				fclose(f);
				vram_dirty();
				pal_dirty();
				sound_dirty();
				mem_updatemap();
			}
		}
		free(name);
	}

	for(i=0; i<8; i++)
		if(statebody[i] != emptyslot) free(statebody[i]);

	free(saveprefix);
	return ret;
}

#define GBPAL_COUNT 13
struct pal_s{
	char name[16];
	unsigned int dmg_bgp[4];
	unsigned int dmg_wndp[4];
	unsigned int dmg_obp0[4];
	unsigned int dmg_obp1[4];
}gbpal[GBPAL_COUNT] = {
	{
		.name = "Default",
		.dmg_bgp  = {0X98D0E0,0X68A0B0,0X60707C,0X2C3C3C},
		.dmg_wndp = {0X98D0E0,0X68A0B0,0X60707C,0X2C3C3C},
		.dmg_obp0 = {0X98D0E0,0X68A0B0,0X60707C,0X2C3C3C},
		.dmg_obp1 = {0X98D0E0,0X68A0B0,0X60707C,0X2C3C3C}
	},{//Grey Pallete
		.name = "Grey",
		.dmg_bgp  = {  0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000 }, //BG
		.dmg_wndp = {  0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000 }, //WIN
		.dmg_obp0 = {  0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000 }, //OB0
		.dmg_obp1 = {  0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000 }  //OB1
	},{//Left
		.name = "Blue",
		.dmg_bgp  = {   0xFFFFFF, 0xF8A878, 0xF8A878, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0xF83000, 0xF83000, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x8888E8, 0x004080, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x8888E8, 0x004080, 0x000000 }
	},{//Left+A
		.name = "Dark Blue",
		.dmg_bgp  = {   0xFFFFFF, 0xF6939D, 0xA03346, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0xF6939D, 0xA03346, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x8888E8, 0x004080, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x8888E8, 0x004080, 0x000000 }
	},{//Up
		.name = "Brown",
		.dmg_bgp  = {   0xFFFFFF, 0x5098E8, 0x004080, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x5098E8, 0x004080, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x5098E8, 0x004080, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x5098E8, 0x004080, 0x000000 }
	},{//Up+A
		.name = "Red",
		.dmg_bgp  = {   0xFFFFFF, 0x8888E8, 0x2727A8, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x8888E8, 0x2727A8, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x00F800, 0xFF3300, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x00F800, 0xFF3300, 0x000000 }
	},{//Up+B
		.name = "Dark Brown",
		.dmg_bgp  = {   0xFFFFFF, 0x94ACC0, 0x4A7B94, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x94ACC0, 0x4A7B94, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x5098E8, 0x004080, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x5098E8, 0x004080, 0x000000 }
	},{//Right
		.name = "Green",
		.dmg_bgp  = {   0xFFFFFF, 0x00F800, 0x0033F8, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x00F800, 0x0033F8, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x00F800, 0x0033F8, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x00F800, 0x0033F8, 0x000000 }
	},{//Right+A
		.name = "Dark Green",
		.dmg_bgp  = {   0xFFFFFF, 0x00F800, 0x0033F8, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x00F800, 0x0033F8, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0xE88888, 0x2727A8, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0xE88888, 0x2727A8, 0x000000 }
	},{//Right+B
		.name = "Inverted",
		.dmg_bgp  = {   0x000000, 0xA1A200, 0x00FFF0, 0xFFFFFF },
		.dmg_wndp = {   0x000000, 0xA1A200, 0x00FFF0, 0xFFFFFF },
		.dmg_obp0 = {   0x000000, 0xA1A200, 0x00FFF0, 0xFFFFFF },
		.dmg_obp1 = {   0x000000, 0xA1A200, 0x00FFF0, 0xFFFFFF }
	},{//Down
		.name = "Pastel",
		.dmg_bgp  = {   0xFFFFFF, 0x8888E8, 0xF6939D, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x8888E8, 0xF6939D, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x8888E8, 0xF6939D, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x8888E8, 0xF6939D, 0x000000 }
	},{//Down+A
		.name = "Orange",
		.dmg_bgp  = {   0xFFFFFF, 0x00FFF0, 0x0033F8, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x00FFF0, 0x0033F8, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0x00FFF0, 0x0033F8, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0x00FFF0, 0x0033F8, 0x000000 }
	},{//Down+B
		.name = "Yellow",
		.dmg_bgp  = {   0xFFFFFF, 0x00FFF8, 0x004080, 0x000000 },
		.dmg_wndp = {   0xFFFFFF, 0x00FFF8, 0x004080, 0x000000 },
		.dmg_obp0 = {   0xFFFFFF, 0xF8A878, 0x008848, 0x000000 },
		.dmg_obp1 = {   0xFFFFFF, 0xF8A878, 0x008848, 0x000000 }
	}
};

int findpal(){
	int *a, *b;
	int i,j;
	for(i=0; i<GBPAL_COUNT; i++){
		a = gbpal[i].dmg_bgp ; b = rc_getvec("dmg_bgp");
		if(a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3])
			continue;
		a = gbpal[i].dmg_wndp; b = rc_getvec("dmg_wndp");
		if(a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3])
			continue;
		a = gbpal[i].dmg_obp0; b = rc_getvec("dmg_obp0");
		if(a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3])
			continue;
		a = gbpal[i].dmg_obp1; b = rc_getvec("dmg_obp1");
		if(a[0] != b[0] || a[1] != b[1] || a[2] != b[2] || a[3] != b[3])
			continue;
		return i;
	}
	return 0;
}

char *lpalettes[] = {
	gbpal[0].name,
	gbpal[1].name,
	gbpal[2].name,
	gbpal[3].name,
	gbpal[4].name,
	gbpal[5].name,
	gbpal[6].name,
	gbpal[7].name,
	gbpal[8].name,
	gbpal[9].name,
	gbpal[10].name,
	gbpal[11].name,
	gbpal[12].name,
	NULL
};

const char *lcolorfilter[] = {"Off","On","GBC Only",NULL};
const char *lupscaler[] = {"Sample1.5x","Scale3x+Sample.75x",NULL};
const char *lframeskip[] = {"Auto","Off","1","2","3","4",NULL};
#if WIZ
const char *lclockspeeds[] = {"Default","250 mhz","300 mhz","350 mhz","400 mhz","450 mhz","500 mhz","550 mhz","600 mhz","650 mhz","700 mhz","750 mhz",NULL};
#endif
const char *volume_levels[] = {"0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%", NULL};
int volume_hardware = 100 / 10; /* todo make this an rc variable */


static char config[16][256];

int menu_options(){

	struct pal_s *palp=0;
	int pal=0, skip=0, ret=0, cfilter=0, upscale=0, speed=0, i=0;
	char *tmp=0, *romdir=0;

	FILE *file;

	pal = findpal();
	cfilter = rc_getint("colorfilter");
	if(cfilter && !rc_getint("filterdmg")) cfilter = 2;
	upscale = rc_getint("upscaler");
	skip = rc_getint("frameskip")+1;
	speed = rc_getint("cpuspeed")/50 - 4;
	if(speed<0) speed = 0;
	if(speed>11) speed = 11;

	romdir = rc_getstr("romdir");
	romdir = romdir ? strdup(romdir) : strdup(".");

	start:

	dialog_begin("Options",NULL);

	dialog_option("Mono Palette",lpalettes,&pal);
	dialog_option("Color Filter",lcolorfilter,&cfilter);
	dialog_option("Upscaler",lupscaler,&upscale);
	dialog_option("Frameskip",lframeskip,&skip);
#if WIZ
	dialog_option("Clock Speed",lclockspeeds,&speed);
#else
	dialog_text("Clock Speed","Default",0);
#endif
	dialog_text("Rom Path",romdir,FIELD_SELECTABLE);
	#ifdef GNUBOY_HARDWARE_VOLUME
	dialog_option("Volume", volume_levels, &volume_hardware); /* this is not the OSD volume.. */
	#else
	dialog_text("Volume", "Default - use soft volume", 0); /* this is not the OSD volume.. */
	#endif /* GNBOY_HARDWARE_VOLUME */
	dialog_text(NULL,NULL,0);
	dialog_text("Apply",NULL,FIELD_SELECTABLE);
	dialog_text("Save",NULL,FIELD_SELECTABLE);

	switch(ret=dialog_end()){
		case 6: /* "Rom Path" romdir */
			tmp = menu_requestdir("Select Rom Directory",romdir);
			if(tmp){
				free(romdir);
				romdir = tmp;
			}
			goto start;
		case 9: /* Apply */
		case 10: /* Save */
			#ifdef GNUBOY_HARDWARE_VOLUME
			pcm_volume(volume_hardware * 10);
			#endif /* GNBOY_HARDWARE_VOLUME */
			palp = &gbpal[pal];
			if(speed)
				speed = speed*50 + 200;
			sprintf(config[0],"set dmg_bgp 0x%.6x 0x%.6x 0x%.6x 0x%.6x", palp->dmg_bgp[0], palp->dmg_bgp[1], palp->dmg_bgp[2], palp->dmg_bgp[3]);
			sprintf(config[1],"set dmg_wndp 0x%.6x 0x%.6x 0x%.6x 0x%.6x",palp->dmg_wndp[0],palp->dmg_wndp[1],palp->dmg_wndp[2],palp->dmg_wndp[3]);
			sprintf(config[2],"set dmg_obp0 0x%.6x 0x%.6x 0x%.6x 0x%.6x",palp->dmg_obp0[0],palp->dmg_obp0[1],palp->dmg_obp0[2],palp->dmg_obp0[3]);
			sprintf(config[3],"set dmg_obp1 0x%.6x 0x%.6x 0x%.6x 0x%.6x",palp->dmg_obp1[0],palp->dmg_obp1[1],palp->dmg_obp1[2],palp->dmg_obp1[3]);
			sprintf(config[4],"set colorfilter %i",cfilter!=0);
			sprintf(config[5],"set filterdmg %i",cfilter==1);
			sprintf(config[6],"set upscaler %i",upscale);
			sprintf(config[7],"set frameskip %i",skip-1);
			sprintf(config[8],"set cpuspeed %i",speed);
			sprintf(config[9],"set romdir \"%s\"",romdir);

			for(i=0; i<10; i++)
				rc_command(config[i]);

			pal_dirty();

			if (ret == 10){ /* Save */
				file = fopen("ohboy.rc","w");
				for(i=0; i<10; i++){
					fputs(config[i],file);
					fputs("\n",file);
				}
				fclose(file);
			}

		break;
	}

	free(romdir);

	return ret;
}

int menu(){;

	char *dir;
	int mexit=0;
	static char *loadrom;

	gui_begin();
	while(!mexit){
		dialog_begin(rom.name,"ohBoy");

		dialog_text("Back to Game",NULL,FIELD_SELECTABLE);

		dialog_text("Load ROM",NULL,FIELD_SELECTABLE);
		dialog_text(NULL,NULL,0);
		dialog_text("Load State",NULL,FIELD_SELECTABLE);
		dialog_text("Save State",NULL,FIELD_SELECTABLE);
		dialog_text(NULL,NULL,0);
		dialog_text("Options",NULL,FIELD_SELECTABLE);
		dialog_text("Quit","",FIELD_SELECTABLE);

		switch(dialog_end()){
			case 2:
				dir = rc_getstr("romdir");
				if(loadrom = menu_requestfile(NULL,"Select Rom",dir,"gb;gbc;zip")) {
					loader_unload();
					ohb_loadrom(loadrom);
					mexit=1;
				}
				break;
			case 4:
				if(menu_state(0)) mexit=1;
				break;
			case 5:
				if(menu_state(1)) mexit=1;
				break;
			case 7:
				if(menu_options()) mexit=1;
				break;
			case 8:
				exit(0);
				break;
			default:
				mexit=1;
				break;
		}
	}
	gui_end();

	return 0;
}

/*#include VERSION*/

int launcher(){;

	char *rom = 0;
	char *dir = rc_getstr("romdir");
	char *version_str[80];
    
    snprintf(version_str, sizeof(version_str)-1, "gnuboy %s", VERSION);

	gui_begin();

launcher:
	dialog_begin("OhBoy http://ohboy.googlecode.com/", version_str);
	dialog_text("Load ROM",NULL,FIELD_SELECTABLE);
	dialog_text("Options",NULL,FIELD_SELECTABLE);
	dialog_text("Quit","",FIELD_SELECTABLE);

	switch(dialog_end()){
		case 1:
			rom = menu_requestfile(NULL,"Select Rom",dir,"gb;gbc;zip");
			if(!rom) goto launcher;
			break;
		case 2:
			if(!menu_options()) goto launcher;
			break;
		case 3:
			exit(0);
		default:
			goto launcher;
	}

	gui_end();

	return rom;
}

