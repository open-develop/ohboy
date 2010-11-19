
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h> /* test! for chdir etc. */


#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <time.h>

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

char* menu_browsedir(char* file, char *title, char *exts){
	char wd[PATH_MAX];
	DIR *dir;
	struct dirent *d;
	struct stat s;
	int n=0, i, j;
	char *files[1<<16];

	getcwd(wd,PATH_MAX);

	if(!(dir = opendir(wd))) return NULL;

	files[n] = malloc(4);
	strcpy(files[n++], "/..");

	d = readdir(dir);
	if(d && !strcmp(d->d_name,".")) d = readdir(dir);
	if(d && !strcmp(d->d_name,"..")) d = readdir(dir);

	while(d){
		stat(d->d_name,&s);
		if(S_ISDIR (s.st_mode))
		{
			files[n] = malloc(strlen(d->d_name)+2);
			files[n][0] = '/';
			strcpy(files[n]+1, d->d_name);
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

	sys_sanitize(wd);
	dialog_begin(title,wd);

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
	char wd[PATH_MAX];
	char *dir;
	int allocmem = file == NULL;

	getcwd(wd,PATH_MAX);
	if(path) chdir(path);

	if(allocmem) file = malloc(PATH_MAX);
	getcwd(file,PATH_MAX);
	sys_sanitize(file);
	strcat(file,"/");


	while(dir = menu_browsedir(file+strlen(file),title,exts)){
		if(dir[0]!='/'){
			realloc(file,strlen(file)+1);
			break;
		}
		chdir(++dir);
		getcwd(file,PATH_MAX);
		sys_sanitize(file);
		strcat(file,"/");
	}
	if(!dir) file = NULL;

	chdir(wd);
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
				ldirs[ldirn][0] = '/';
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
	sprintf(saveprefix, "%s/%s", savedir, savename);

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
	dialog_text(NULL,NULL,0);
	dialog_text("Apply",NULL,FIELD_SELECTABLE);
	dialog_text("Save",NULL,FIELD_SELECTABLE);

	switch(ret=dialog_end()){
		case 6:
			tmp = menu_requestdir("Select Rom Directory",romdir);
			if(tmp){
				free(romdir);
				romdir = tmp;
			}
			goto start;
		case 8:
		case 9:
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

			if(ret==9){
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

	gui_begin();

launcher:
	dialog_begin("OhBoy Copyright (C) 2009 UBYTE","OhBoy");
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

