/* DMCRadio 1.1.4
 * Copyright (c) 2003-2004 Sven Hesse (DrMcCoy)
 *
 * This file is part of DMCRadio and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

#include <curses.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>	
#include <stdint.h>
#include "radiodev.h"
#include "mixerdev.h"
#include "dspdev.h"

#define tof(a) (a) ? "Yes" : "No"
#define max(a,b) (a) > (b) ? (a) : (b)
#define TUNERWIN windows[winfuncs[0].win].win
#define KEYSWIN windows[winfuncs[1].win].win
#define STATIONSWIN windows[winfuncs[2].win].win
#define STATUSWIN windows[winfuncs[3].win].win
#define SCROLLWIN windows[winfuncs[4].win].win
#define INFOWIN windows[winfuncs[5].win].win
#define RECORDWIN windows[winfuncs[6].win].win

struct station
{
	double frequency;
	char name[79];
};

struct window
{
	int x, y, w, h;
	WINDOW *win;
};

struct windowfunctions
{
	char *name, fg, bg, bold, display;
	int win;
};

struct cochar
{
	int x, y, n, win;
};

struct extracol
{
	char *name, fg, bg, bold;
	int nchars;
	struct cochar *cochars;
};

static struct station stations[256];
static char lcdchars[11][5][7], lcdpoint[5][4], font[4096], dir[4096], dira[4096];
static char *cvolctrl=NULL, rdev[4096], mdev[4096], ddev[4096], recordf[13];
static char exitscroll=0, exitrecord=1, hasexit=0, radiomute=0, mdist;
static char aadjust=0, record=0, **mdevns, **mdevls, pml=1;
static double buttons[20], ffreq;
static int fd, lcdcharwidth, lcdpointwidth, mixerdev=-1, denoiser=0;
static int curhelpp=0, helppagecount=3, recordfd;
static int numwin=5, numfunc=7, nec=1, nmdevs, recordi=0;
static long long int recordeds, maxrecs=104857600;
static pthread_t *scrollthread, *recordthread;
static pthread_mutex_t nclock;
static struct window *windows;
static struct windowfunctions *winfuncs;
static struct extracol *extracols;
extern int errno, LOW, AMONO, ASTEREO, SMUTE, SMUTEA, SVOLUME, SBASS, STREBLE;
extern int dsp_blocksize, dsp_chn, dsp_rate;
extern double radiofreqmin, radiofreqmax;

static char full[] = 
	"##################################################"
	"##################################################"
	"##################################################"
	"##################################################";
static char empty[] =
	"--------------------------------------------------"
	"--------------------------------------------------"
	"--------------------------------------------------"
	"--------------------------------------------------";

void printconshelp(void);
void init(void);
void checkscr(void);
void initwindows(void);
void drawwindowf(int id);
void handle_extracols(int id);
void setuplayout(void);
void *doscroll(void *nothing);
void *dorecord(void *nothing);
void write_header(void);
void drawbut(void);
void loadconf(void);
char *strupper(char *string);
char *trimr(char *string);
void readfont(char *file);
void drawhelppage(int helppage);
void drawfreq(double frequency);
void drawsignal(void);
void drawaudiomode(void);
void stdfont(void);
void saveconf(void);
void mainloop(void);
long long int parse_newmaxrecs(char *newsize);
int rec_start(void);
void rec_stop(void);
char *size2fsize(long long int size);
void deinit(void);

int main(int argc, char *argv[])
{
	int justmute = 0, thisarg;

	rdev[0] = mdev[0] = ddev[0] = '\0';
	if(argc > 1)
	{
		for(thisarg=1;thisarg<argc;thisarg++)
		{
			if(((!strcmp(argv[thisarg], "-h"))
					|| (!strcmp(argv[thisarg], "--help")))
				|| (((!strcmp(argv[thisarg], "-r")
					|| !strcmp(argv[thisarg], "--radiodev")) && (argc==thisarg+1)))
				|| (((!strcmp(argv[thisarg], "-x")
					|| !strcmp(argv[thisarg], "--mixerdev")) && (argc==thisarg+1)))
				|| (((!strcmp(argv[thisarg], "-d")
					|| !strcmp(argv[thisarg], "--dspdev")) && (argc==thisarg+1))))
				printconshelp();
			if((!strcmp(argv[thisarg], "-m")) || (!strcmp(argv[thisarg], "--mute")))
				justmute = TRUE;
			if((!strcmp(argv[thisarg], "-v"))
					|| (!strcmp(argv[thisarg], "--version")))
			{
				printf("DMCRadio %s (c) %s by Sven Hesse (DrMcCoy) ", VERSION, YEAR);
				printf("<SvHe_TM@gmx.de>\n");
				printf("%s, %s\n", DAY, DATE);
				exit(-1);
			}
			if(((!strcmp(argv[thisarg], "-r")
					|| !strcmp(argv[thisarg], "--radiodev"))) && (argc>thisarg))
				strncpy(rdev, argv[thisarg+1], sizeof(rdev));
			if((!strcmp(argv[thisarg], "-x") || !strcmp(argv[thisarg], "--mixerdev"))
					&& (argc>thisarg))
				strncpy(mdev, argv[thisarg+1], sizeof(mdev));
			if((!strcmp(argv[thisarg], "-d") || !strcmp(argv[thisarg], "--dspdev"))
					&& (argc>thisarg))
				strncpy(mdev, argv[thisarg+1], sizeof(mdev));
		}
	}

	loadconf();
	init();

	if(justmute)
	{
		radio_mute(TRUE);
		deinit();
		exit(0);
	}

	setuplayout();
	mainloop();
	saveconf();
	deinit();
	exit(0);
}

void printconshelp(void)
{
	printf("Usage: DMCRadio [options]\n\n");
	printf("	-h	--help			This help\n");
	printf("	-m	--mute			Just mute and exit\n");
	printf("	-r dev	--radiodev dev		Set Radiodevice to dev [/dev/radio]\n");
	printf("	-x dev	--mixerdev dev		Set Mixerdevice to dev [/dev/mixer]\n");
	printf("	-d dev	--dspdev dev		Set DSPdevice to dev [/dev/dsp]\n");
	printf("	-v	--version		Print version and exit\n\n");
	exit(-1);
}

void init(void)
{
	int i, j;

#ifdef __FreeBSD__
	if(!strlen(rdev)) strcpy(rdev, "/dev/tuner");
#else
	if(!strlen(rdev)) strcpy(rdev, "/dev/radio");
#endif
	if(!strlen(mdev)) strcpy(mdev, "/dev/mixer");
	if(!strlen(ddev)) strcpy(ddev, "/dev/dsp");
	errno = 0;
	if(radio_init(rdev))
	{
		fprintf(stderr, "Can't open %s: %s!\n", rdev, strerror(errno));
		exit(-1);
	}
	if(mixer_init(mdev))
	{
		fprintf(stderr, "Can't open %s: %s!\n", mdev, strerror(errno));
		exit(-1);
	}
	mdevns = mixer_getdevnames();
	mdevls = mixer_getdevlabels();
	for(i=0;i<mixer_getnrdevices();i++)
		for(j=(strlen(mdevls[i])-1);j>=0;j--)
			if(mdevls[i][j] == ' ') mdevls[i][j] = '\0'; else break;
	if(!(nmdevs = mixer_getnrdevices()) || (mixer_getfirstsupdevnr() == -1))
	{
		fprintf(stderr, "Mixer has no devices!?\n");
		exit(-1);
	}
	
	if(cvolctrl) mixerdev = mixer_getdevnr(cvolctrl);
	if(mixerdev == -1) mixerdev = mixer_getfirstsupdevnr();
	initscr();
	checkscr();
	printf("[?25l");
	start_color();
	cbreak();
	noecho();

	pthread_mutex_init(&nclock, NULL);

	if((ffreq < radiofreqmin) || (ffreq > radiofreqmax)) ffreq = radiofreqmin;
	dsp_chn = 2;
	dsp_rate = 44100;
}

void checkscr(void)
{
	int i;
	if((LINES < 24) || (COLS < 80))
	{
		endwin();
		fprintf(stderr, "Terminal too small, need at least 80x24 characters!\n");
		exit(-1);
	}
	if(LINES > 25)
		for(i=0;i<numwin;i++) windows[i].y += (LINES-25)/2;
	else if(LINES == 24) windows[4].x = windows[4].y = -1;
	if(COLS > 80)
		for(i=0;i<numwin;i++) windows[i].x += (COLS-80)/2;
}

void initwindows(void)
{
	int i;
	windows = malloc(numwin * sizeof(struct window));
	winfuncs = malloc(numfunc * sizeof(struct windowfunctions));
	extracols = malloc(nec * sizeof(struct extracol));
	extracols[0].cochars = malloc((extracols[0].nchars=2)*sizeof(struct cochar));
	windows[0].x =  0; windows[0].y =  0; windows[0].w = 42; windows[0].h =  7;
	windows[1].x = 44; windows[1].y =  0; windows[1].w = 36; windows[1].h =  7;
	windows[2].x =  0; windows[2].y =  8; windows[2].w = 80; windows[2].h = 12;
	windows[3].x =  0; windows[3].y = 21; windows[3].w = 80; windows[3].h =  3;
	windows[4].x =  0; windows[4].y = 24; windows[4].w = 80; windows[4].h =  1;
	winfuncs[0].name = strdup("Tuner");
	winfuncs[1].name = strdup("Keys");
	winfuncs[2].name = strdup("Stations");
	winfuncs[3].name = strdup("Status");
	winfuncs[4].name = strdup("Scroll");
	winfuncs[5].name = strdup("Info");
	winfuncs[6].name = strdup("Record");
	for(i=0;i<numfunc;i++) winfuncs[i].display = 0;
	winfuncs[0].win = 0;
	winfuncs[1].win = 1;
	winfuncs[2].win = winfuncs[5].win = winfuncs[6].win = 2;
	winfuncs[3].win = 3;
	winfuncs[4].win = 4;
  winfuncs[0].fg = 6;
	winfuncs[1].fg = winfuncs[2].fg = winfuncs[3].fg = 7;
	winfuncs[4].fg = winfuncs[5].fg = winfuncs[6].fg = 7;
	winfuncs[0].bg = winfuncs[1].bg = winfuncs[2].bg = 4;
	winfuncs[3].bg = winfuncs[6].bg = 4;
	winfuncs[5].bg = 1;
	winfuncs[4].bg = 0;
	winfuncs[0].bold = winfuncs[1].bold = winfuncs[2].bold = 1;
	winfuncs[3].bold = winfuncs[4].bold = winfuncs[5].bold = 1;
	winfuncs[6].bold = 1;
	extracols[0].name = strdup("Volume");
	extracols[0].cochars[0].x = extracols[0].cochars[1].x = mdist = 0;
	extracols[0].cochars[0].y =  6; extracols[0].cochars[1].y =  6;
	extracols[0].cochars[0].n =  1; extracols[0].cochars[1].n =  1;
	extracols[0].cochars[0].win = 0; extracols[0].cochars[1].win = 0;
	extracols[0].fg = 4; extracols[0].bg = 1; extracols[0].bold = 0;
}

void drawwindowf(int id)
{
	int i;

	pthread_mutex_lock(&nclock);
	werase(windows[winfuncs[id].win].win);
	init_pair(id+1, winfuncs[id].fg, winfuncs[id].bg);
	wbkgd(windows[winfuncs[id].win].win,
			(winfuncs[id].bold ? A_BOLD : 0) | COLOR_PAIR(id+1));
	for(i=0;i<numfunc;i++)
		if(winfuncs[i].win == winfuncs[id].win) winfuncs[i].display = 0;
	winfuncs[id].display = 1;

	switch(id)
	{
		case 0:
			if(pml)
			{
				mdist = floor(strlen(mdevls[mixerdev])/2);
				extracols[0].cochars[0].x = 17-mdist+strlen(mdevls[mixerdev]);
				extracols[0].cochars[1].x = 26-mdist+strlen(mdevls[mixerdev]);
			}
			else
			{
				mdist = floor(strlen(mdevns[mixerdev])/2);
				extracols[0].cochars[0].x = 17-mdist+strlen(mdevns[mixerdev]);
				extracols[0].cochars[1].x = 26-mdist+strlen(mdevns[mixerdev]);
			}
			wborder(windows[winfuncs[id].win].win, 0, 0, 0, 0, 0, 0, 0, 0); 
			mvwprintw(windows[winfuncs[id].win].win, 0, 1, " Tuner ");
			mvwhline(windows[winfuncs[id].win].win, 0, 21, acs_map['u'], 1);
			mvwhline(windows[winfuncs[id].win].win, 0, 40, acs_map['t'], 1);
			mvwprintw(windows[winfuncs[id].win].win, 0, 22, " Signal: ");
			mvwhline(windows[winfuncs[id].win].win, 6, 12-mdist,
					acs_map['u'], 1);
			if(pml)
				mvwhline(windows[winfuncs[id].win].win, 6,
						29-mdist+strlen(mdevls[mixerdev]), acs_map['t'], 1);
			else
				mvwhline(windows[winfuncs[id].win].win, 6,
						29-mdist+strlen(mdevns[mixerdev]), acs_map['t'], 1);
			mvwhline(windows[winfuncs[id].win].win, 2, 0, ACS_LRCORNER, 1);
			mvwhline(windows[winfuncs[id].win].win, 3, 0, acs_map['+'], 1);
			mvwhline(windows[winfuncs[id].win].win, 4, 0, ACS_URCORNER, 1);
			break;
		case 2:
			wborder(windows[winfuncs[id].win].win, 0, 0, 0, 0, 0, 0, 0, 0); 
			mvwprintw(windows[winfuncs[id].win].win, 0, 1, " Preset Stations ");
			drawbut();
			break;
		case 3:
			wborder(windows[winfuncs[id].win].win, 0, 0, 0, 0, 0, 0, 0, 0); 
			mvwprintw(windows[winfuncs[id].win].win, 0, 1, " Statusbar ");
			mvwprintw(windows[winfuncs[id].win].win, 2, 2,
					"< DMCRadio %s >", VERSION);
			mvwprintw(windows[winfuncs[id].win].win, 2, 46-strlen(YEAR),
					"< (c) %s by Sven Hesse (DrMcCoy) >", YEAR);
			break;
		case 5:
			wborder(windows[winfuncs[id].win].win, 0, 0, 0, 0, 0, 0, 0, 0);
			mvwprintw(windows[winfuncs[id].win].win, 0, 1,
					" Informations about your radio-card ");
			mvwprintw(windows[winfuncs[id].win].win, 2, 2,
					"Range        : %.2f - %.2f", radiofreqmin, radiofreqmax);
			mvwprintw(windows[winfuncs[id].win].win, 3, 2,
					"Mono         : %s", tof(AMONO));
			mvwprintw(windows[winfuncs[id].win].win, 4, 2,
					"Stereo       : %s", tof(ASTEREO));
			mvwprintw(windows[winfuncs[id].win].win, 5, 2,
					"Mute         : %s", tof(SMUTE));
			mvwprintw(windows[winfuncs[id].win].win, 6, 2,
					"Mutable      : %s", tof(SMUTEA));
			mvwprintw(windows[winfuncs[id].win].win, 7, 2,
					"CanSetVolume : %s", tof(SVOLUME));
			mvwprintw(windows[winfuncs[id].win].win, 8, 2,
					"CanSetBass   : %s", tof(SBASS));
			mvwprintw(windows[winfuncs[id].win].win, 9, 2,
					"CanSetTreble : %s", tof(STREBLE));
			wrefresh(windows[winfuncs[id].win].win);
			break;
		case 6:
			wborder(windows[winfuncs[id].win].win, 0, 0, 0, 0, 0, 0, 0, 0);
			mvwprintw(windows[winfuncs[id].win].win, 0, 1, " Record ");
			break;
	}
	pthread_mutex_unlock(&nclock);
	handle_extracols(id);
}

void handle_extracols(int id)
{
	int i, j;

	if(!winfuncs[id].display) return;
	for(i=0;i<nec;i++)
	{
		init_pair(numfunc+i+1, extracols[i].fg, extracols[i].bg);
		for(j=0;j<extracols[i].nchars;j++)
			if(winfuncs[id].win == extracols[i].cochars[j].win)
				mvwchgat(windows[winfuncs[id].win].win, extracols[i].cochars[j].y,
						extracols[i].cochars[j].x, extracols[i].cochars[j].n,
						(extracols[i].bold ? A_BOLD : 0), numfunc+i+1, NULL);
	}
	wnoutrefresh(windows[winfuncs[id].win].win);
}

void setuplayout(void)
{
	FILE *testfile;
	int i;

	for(i=0;i<numwin;i++)
	{
		windows[i].win =
			newwin(windows[i].h, windows[i].w, windows[i].y, windows[i].x);
	}
	for(i=0;i<numwin;i++) drawwindowf(i);
	drawwindowf(3);
	drawhelppage(curhelpp);
	snprintf(dira, sizeof(dira), "%s%s", dir, font);
	strncpy(font, dira, sizeof(font));
	testfile = fopen(font, "r");
	if(testfile == NULL) stdfont();
	else
	{
		readfont(font);
		fclose(testfile);
	}
	drawfreq(ffreq);
	for(i=0;i<numwin;i++)
		wnoutrefresh(windows[i].win);
	doupdate();
	keypad(TUNERWIN, TRUE);
	keypad(INFOWIN, TRUE);
	keypad(STATUSWIN, TRUE);
}

void saveconf(void)
{
	int i;
	char config[256];
	FILE *configfile;

	snprintf(config, sizeof(config), "%s/.DMCRadiorc", getenv("HOME"));
	if((configfile = fopen(config, "w")))
	{
		fprintf(configfile, "[Style]\n\n");
		fprintf(configfile, "LCDFont=%s\n", (char *) basename(font));
		for(i=0;i<numfunc;i++)
		{
			fprintf(configfile, "%sfg=%d\n", winfuncs[i].name, winfuncs[i].fg);
			fprintf(configfile, "%sbg=%d\n", winfuncs[i].name, winfuncs[i].bg);
			fprintf(configfile, "%sbold=%d\n", winfuncs[i].name, winfuncs[i].bold);
		}
		for(i=0;i<nec;i++)
		{
			fprintf(configfile, "%sfg=%d\n", extracols[i].name, extracols[i].fg);
			fprintf(configfile, "%sbg=%d\n", extracols[i].name, extracols[i].bg);
			fprintf(configfile, "%sbold=%d\n", extracols[i].name, extracols[i].bold);
		}
		fprintf(configfile, "[Stations]\n\n");
		for(i=0;i<256;i++)
			if(stations[i].frequency != 0)
				fprintf(configfile, "%.2f=%s\n", stations[i].frequency,
						stations[i].name);
		fprintf(configfile, "\n[Buttons]\n\n");
		for(i=0;i<20;i++)
			if(buttons[i] != 0)
				fprintf(configfile, "%d=%.2f\n", i, buttons[i]);
		fprintf(configfile, "\n[Misc]\n\n");
		fprintf(configfile, "Radiodev=%s\n", rdev);
		fprintf(configfile, "Mixerdev=%s\n", mdev);
		fprintf(configfile, "DSPdev=%s\n", ddev);
		fprintf(configfile, "Audioinput=%s\n", mdevns[mixerdev]);
		fprintf(configfile, "Frequency=%.2f\n", ffreq);
		fprintf(configfile, "Denoiser=%d\n", denoiser);
		fprintf(configfile, "Mixerlabels=%d\n", pml);
		fprintf(configfile, "MaxRecSize=%lld\n", maxrecs);
	}
	fclose(configfile);
}

void *doscroll(void *nothing)
{
	int i, scrollx=1, scrollstep=1;
	double sfreq=0.0;
	char freqstr[78];

	while(!exitscroll)
	{
		pthread_mutex_lock(&nclock);
		drawsignal();
		if(sfreq != ffreq) scrollx = scrollstep = 1;
		sfreq = ffreq;
		werase(SCROLLWIN);
		freqstr[0] = '\0';
		for(i=0;i<256;i++)
			if(rint(sfreq*100) == rint(stations[i].frequency*100))
				snprintf(freqstr, sizeof(freqstr), stations[i].name);
		if(!strlen(freqstr)) sprintf(freqstr, "%.2f MHz", sfreq);
		mvwprintw(SCROLLWIN, 0, scrollx, "%s", freqstr);
		if(strlen(freqstr) < 78) scrollx += scrollstep;
		if(scrollx > (78-(strlen(freqstr)))) scrollstep = -1;
		if(scrollx < 2) scrollstep = 1;
		wnoutrefresh(SCROLLWIN);
		doupdate();
		pthread_mutex_unlock(&nclock);
		usleep(100000);
	}
	return NULL;
}

void drawbut(void)
{
	int i, j;
	char stationname[28];

	for(i=0;i<20;i++)
	{
		if(buttons[i])
			mvwprintw(STATIONSWIN, i%10+1, 34*(i/10)+2,
					"%2d: %7.2f MHz", i+1, buttons[i]);
		for(j=0;j<256;j++)
		{
			if((stations[j].frequency == buttons[i]) && (stations[j].frequency))
			{
				mvwprintw(STATIONSWIN, i%10+1, 34*(i/10)+7,
						"                            ");
				snprintf(stationname, strlen(stations[j].name) > 28 ? 26 : 28,
						 stations[j].name);
				if(strlen(stations[j].name) > 28) strcat(stationname, "...");
				mvwprintw(STATIONSWIN, i%10+1, 34*(i/10)+7, "%s",
						stationname);
			}
		}
	}
}

void loadconf(void)
{
	char buffer[4096];
	int i, k=0, section=42;
	FILE *configfile, *testfile;
	char suffix[256], prefix[256], config[256], testfont[256];

	initwindows(); // <====
	snprintf(config, sizeof(config), "%s/.DMCRadiorc", getenv("HOME"));
	if(!strcmp(PREFIX, "NONE"))
		strcpy(dir, "/usr/local/share/DMCRadio/");
	else
		snprintf(dir, 4096, "%s/share/DMCRadio/", PREFIX);
	strcpy(font, "small.raf");

	configfile = fopen(config, "r");
	if(!(configfile = fopen(config, "r"))) return;
	

	while(!feof(configfile))
	{
		buffer[0] = suffix[0] = prefix[0] = '\0';
		fgets(buffer, sizeof(buffer), configfile);
		if(!strcasecmp(buffer, "[STYLE]\n")) { section = 0; continue; }
		if(!strcasecmp(buffer, "[STATIONS]\n")) { section = 1; continue; }
		if(!strcasecmp(buffer, "[BUTTONS]\n")) { section = 2; continue; }
		if(!strcasecmp(buffer, "[MISC]\n")) { section = 3; continue; }
		if((!strcmp(buffer, "\n")) || (!strcmp(buffer, ""))
				|| (!strchr(buffer, '=')) || (!strchr(buffer, '\n'))
				|| (buffer[0] == '='))
			continue;
		strncpy(suffix, strupper(strtok(buffer, "=")), sizeof(suffix));
		memmove(buffer, strtok(NULL, ""), sizeof(buffer));
		if(strlen(buffer) == 1) continue;
		strncpy(prefix, strtok(buffer, "\n"), sizeof(prefix));
		if(!section)
		{
			if(!strcmp(suffix, "LCDFONT") && strcmp(prefix, ""))
			{
				snprintf(testfont, sizeof(testfont), "%s%s", dir, prefix);
				if((testfile = fopen(testfont, "r")))
				{
					strncpy(font, prefix, sizeof(font));
					fclose(testfile);
				}
				errno=0;
			}
			if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
			{
				for(i=0;i<numfunc;i++)
					if(!strncasecmp(suffix, winfuncs[i].name, strlen(winfuncs[i].name)))
					{
						if(strlen(suffix) == (strlen(winfuncs[i].name)+2))
						{
							if(!strncasecmp(suffix+strlen(winfuncs[i].name), "fg", 2))
								winfuncs[i].fg = atoi(prefix);
							else if(!strncasecmp(suffix+strlen(winfuncs[i].name), "bg", 2))
								winfuncs[i].bg = atoi(prefix);
						}
						else if((strlen(suffix) == (strlen(winfuncs[i].name)+4))
								&& (!strncasecmp(suffix+strlen(winfuncs[i].name), "bold", 4)))
							winfuncs[i].bold = atoi(prefix);
					}
				for(i=0;i<nec;i++)
					if(!strncasecmp(suffix, extracols[i].name, strlen(extracols[i].name)))
					{
						if(strlen(suffix) == (strlen(extracols[i].name)+2))
						{
							if(!strncasecmp(suffix+strlen(extracols[i].name), "fg", 2))
								extracols[i].fg = atoi(prefix);
							else if(!strncasecmp(suffix+strlen(extracols[i].name), "bg", 2))
								extracols[i].bg = atoi(prefix);
						}
						else if((strlen(suffix) == (strlen(extracols[i].name)+4))
								&& (!strncasecmp(suffix+strlen(extracols[i].name), "bold", 4)))
							extracols[i].bold = atoi(prefix);
					}
			}
		}
		else if((section == 1) && (k < 256))
		{
			sscanf(suffix, "%lf", &stations[k].frequency);
			snprintf(stations[k++].name, sizeof(stations[k].name), prefix);
		}
		else if(section == 2)
		{
			int buti = (int)strtoul(suffix, NULL, 10);
			if((buti >= 0) && (buti <= 20)) sscanf(prefix, "%lf", buttons+buti);
		}
		else if(section == 3)
		{
			if(!strcmp(suffix, "FREQUENCY")) sscanf(prefix, "%lf", &ffreq);
			else if(!strcmp(suffix, "RADIODEV") && !strlen(rdev))
				snprintf(rdev, sizeof(rdev), prefix);
			else if(!strcmp(suffix, "MIXERDEV") && !strlen(mdev))
				snprintf(mdev, sizeof(mdev), prefix);
			else if(!strcmp(suffix, "DSPDEV") && !strlen(ddev))
				snprintf(ddev, sizeof(ddev), prefix);
			else if(!strcmp(suffix, "AUDIOINPUT")) cvolctrl = strdup(prefix);
			else if(!strcmp(suffix, "DENOISER")) denoiser = atoi(prefix);
			else if(!strcmp(suffix, "MIXERLABELS")) pml = atoi(prefix);
			else if(!strcmp(suffix, "MAXRECSIZE")) maxrecs = atoll(prefix);
		}
	}
	fclose(configfile);
}

char *strupper(char *string)
{
	int i;

	for(i=0;i<strlen(string);i++) string[i] = toupper(string[i]);
	return string;
}

char *trimr(char *string)
{
	int i=strlen(string)-1;

	while((i >= 0) && (iscntrl(string[i])||(string[i]==' ')||(string[i]=='\t')))
		string[i--] = '\0';

	return string;
}

void readfont(char *file)
{
	int i, j;
	char buffer[4096];
	FILE *fontfile;

	if((fontfile = fopen(file, "r")))
	{
		lcdcharwidth = 0;
		for(i=0;i<10;i++)
			for(j=0;j<5;j++)
			{
				fgets(lcdchars[i][j], sizeof(buffer), fontfile);
				lcdchars[i][j][6] = '\0';
				trimr(lcdchars[i][j]);
				lcdcharwidth = (lcdcharwidth > strlen(lcdchars[i][j])
						 ? lcdcharwidth : strlen(lcdchars[i][j]));
			}
		lcdpointwidth = 0;
		for(i=0;i<=4;i++)
		{
			fgets(lcdpoint[i], sizeof(buffer), fontfile);
			lcdpoint[i][3] = '\0';
			trimr(lcdpoint[i]);
			lcdpointwidth = (lcdpointwidth > strlen(lcdpoint[i])
					 ? lcdpointwidth : strlen(lcdpoint[i]));
		}
		for(i=0;i<5;i++) strcpy(lcdchars[10][i], "			");
		fclose(fontfile);
	}
	for(i=0;i<11;i++)
		for(j=0;j<5;j++)
			lcdchars[i][j][lcdcharwidth] = '\0';
}

void drawhelppage(int helppage)
{
	pthread_mutex_lock(&nclock);
	werase(KEYSWIN);
	wborder(KEYSWIN, 0, 0, 0, 0, 0, 0, 0, 0);
	mvwprintw(KEYSWIN, 0, 1, " Key Commands ");
	switch(helppage)
	{
		case 0:
			mvwprintw(KEYSWIN, 0, 31, "(-)");
			mvwprintw(KEYSWIN, 6, 31, "(+)");
			mvwprintw(KEYSWIN, 1, 1, " UP Key    - increment frequency");
			mvwprintw(KEYSWIN, 2, 1, " DOWN Key  - decrement frequency");
			mvwprintw(KEYSWIN, 3, 1, " g         - go to frequency...");
			mvwprintw(KEYSWIN, 4, 1, " x         - exit");
			mvwprintw(KEYSWIN, 5, 1, " ESC, q, e - mute and exit");
			break;
		case 1:
			mvwprintw(KEYSWIN, 0, 31, "(-)");
			mvwprintw(KEYSWIN, 6, 31, "(+)");
			mvwprintw(KEYSWIN, 1, 1, " LEFT Key  - previous volumectrl");
			mvwprintw(KEYSWIN, 2, 1, " RIGHT Key - next volumectrl");
			mvwprintw(KEYSWIN, 3, 1, " m         - mute");
			mvwprintw(KEYSWIN, 4, 1, " d         - denoiser (%s)", 
					denoiser ? "on" : "off");
			mvwprintw(KEYSWIN, 5, 1, " l         - show mixerlabels (%s)", 
					pml ? "on" : "off");
			break;
		case 2:
			mvwprintw(KEYSWIN, 0, 31, "(-)");
			mvwprintw(KEYSWIN, 6, 31, "(+)");
			mvwprintw(KEYSWIN, 1, 1, " f         - chance font");
			mvwprintw(KEYSWIN, 2, 1, " c         - change colors");
			mvwprintw(KEYSWIN, 3, 1, " i         - infos about card");
			mvwprintw(KEYSWIN, 4, 1, " r         - record");
			break;
		case 23:
			mvwprintw(KEYSWIN, 1, 1, " UP Key    - next window ");
			mvwprintw(KEYSWIN, 2, 1, " DOWN Key  - previous window");
			mvwprintw(KEYSWIN, 3, 1, " f         - cycle forecolor");
			mvwprintw(KEYSWIN, 4, 1, " b         - cycle backcolor");
			mvwprintw(KEYSWIN, 5, 1, " o         - toggle bold");
			break;
		case 24:
			mvwprintw(KEYSWIN, 0, 31, "(-)");
			mvwprintw(KEYSWIN, 6, 31, "(+)");
			mvwprintw(KEYSWIN, 1, 1, " LEFT Key  - previous volumectrl");
			mvwprintw(KEYSWIN, 2, 1, " RIGHT Key - next volumectrl");
			mvwprintw(KEYSWIN, 3, 1, " a         - auto-adjust (%s)",
					aadjust ? "on" : "off");
			mvwprintw(KEYSWIN, 4, 1, " SPACE     - %s recording",
					record ? "stop" : "start");
			mvwprintw(KEYSWIN, 5, 1, " n         - next file");
			break;
		case 25:
			mvwprintw(KEYSWIN, 0, 31, "(-)");
			mvwprintw(KEYSWIN, 6, 31, "(+)");
			mvwprintw(KEYSWIN, 1, 1, " s         - mono/stereo");
			mvwprintw(KEYSWIN, 2, 1, " r         - change samplingrate");
			mvwprintw(KEYSWIN, 3, 1, " m         - change maximal size");
			break;
		case 42:
			mvwprintw(KEYSWIN, 1, 1, " UP Key    - next font");
			mvwprintw(KEYSWIN, 2, 1, " DOWN Key  - previous font");
			break;
	}
	wnoutrefresh(KEYSWIN);
	pthread_mutex_unlock(&nclock);
}

void drawfreq(double frequency)
{
	int i, j, xoffset;
	int freqn[5], freqint;
	char fstr[6];

	freqint = rint(frequency*100);
	snprintf(fstr, 6, "%5d", freqint);
	for(i=0;i<6;i++) freqn[i] = fstr[i] - '0';
	if (!freqn[0]) freqn[0] = 10;
	xoffset = 7 + (5*(6-lcdcharwidth)) + (3-lcdpointwidth);

	drawwindowf(0);
	for(i=0;i<5;i++)
	{
		pthread_mutex_lock(&nclock);
		for(j=0;j<5;j++)
			mvwprintw(TUNERWIN, j+1, xoffset, lcdchars[freqn[i]][j]);
		pthread_mutex_unlock(&nclock);
		switch (i)
		{
			case 0: case 1: case 3:
				xoffset += lcdcharwidth;
				break;
			case 2:
				xoffset += lcdcharwidth + lcdpointwidth;
				break;
		}
	}
	xoffset = xoffset - lcdpointwidth - lcdcharwidth;
	pthread_mutex_lock(&nclock);
	for(j=0;j<5;j++)
		mvwprintw(TUNERWIN, j+1, xoffset, lcdpoint[j]);
	pthread_mutex_unlock(&nclock);

	pthread_mutex_lock(&nclock);
	drawsignal();
	pthread_mutex_unlock(&nclock);
	drawaudiomode();
	wnoutrefresh(TUNERWIN);
}

void drawsignal(void)
{
	int signal, i;

	signal = radio_getsignal();
	for(i=0;i<9;i++)
		mvwprintw(TUNERWIN, 0, i+31, "%s",
				signal>i ? "*" : " ");
	if(denoiser)
	{
		if(signal == 1) radio_mute(TRUE);
		else if(signal != 1 && !radiomute && !hasexit) radio_mute(FALSE);
	}
	if(pml)
		mvwprintw(TUNERWIN, 6, 13-mdist, " %s: [<] %3d%% [>] ", mdevls[mixerdev],
				(int)(((float)mixer_getvol(mdevns[mixerdev])/(float)25600+0.005)*100));
	else
		mvwprintw(TUNERWIN, 6, 13-mdist, " %s: [<] %3d%% [>] ", mdevns[mixerdev],
				(int)(((float)mixer_getvol(mdevns[mixerdev])/(float)25600+0.005)*100));
	handle_extracols(0);
	wnoutrefresh(TUNERWIN);
}

void drawaudiomode(void)
{
	char audiostr[3][7]={"UNDEF.", "MONO", "STEREO"};

	radio_getflags();
	pthread_mutex_lock(&nclock);
	mvwprintw(TUNERWIN, 3, 1, audiostr[radio_getaudiomode()]);
	wnoutrefresh(TUNERWIN);
	pthread_mutex_unlock(&nclock);
}

void *dorecord(void *nothing)
{
	int i, histn, *histl, *histr, histi=0, total=76, lenl, lenr, leni;
	int maxl, maxr, secl, secr, igainvol;
	int16_t *buffer;
	char chnstr[32];

	if(dsp_init(ddev))
	{
		pthread_mutex_lock(&nclock);
		mvwprintw(RECORDWIN, 2, 2, "Error: can't open %s: %s!", ddev,
			strerror(errno));
		wrefresh(RECORDWIN);
		pthread_mutex_unlock(&nclock);
		exitrecord = 1;
		return NULL;
	}
	if(dsp_setrec(dsp_chn, dsp_rate))
	{
		pthread_mutex_lock(&nclock);
		mvwprintw(RECORDWIN, 2, 2, "Error: can't set format to signed 16bit LE!");
		wrefresh(RECORDWIN);
		pthread_mutex_unlock(&nclock);
		dsp_deinit();
		exitrecord = 1;
		return NULL;
	}
	histn = 1.5 * dsp_rate * 2 * dsp_chn / dsp_blocksize;
	histl = memset(malloc(histn * sizeof(int)), 0, histn);
	histr = memset(malloc(histn * sizeof(int)), 0, histn);
	while((!exitrecord) && winfuncs[6].display)
	{
		buffer = dsp_read();
		if(record)
		{
			if((maxrecs > -1) && ((recordeds + 43) == maxrecs))
			{
				rec_stop();
				rec_start();
			}
			else if((maxrecs > -1) && ((recordeds + 43) > maxrecs))
			{
				recordeds +=
					(write(recordfd, buffer, maxrecs-recordeds-43));
				rec_stop();
				rec_start();
				recordeds =
					(write(recordfd, buffer+maxrecs-recordeds-43,
					dsp_blocksize-maxrecs+recordeds+43));
			}
			else
				recordeds += (write(recordfd, buffer, dsp_blocksize));
		}
		maxl = maxr = 0;
		for(i=dsp_blocksize>>2;i>0;i--)
		{
			maxl = max(abs(*buffer), maxl); buffer++;
			maxr = max(abs(*buffer), maxr); buffer++;
    }
		histl[histi] = maxl;
		histr[histi] = maxr;
		if((++histi) >= histn) histi = 0;
		secl = secr = 0;
		for(i=0;i<histn;i++)
		{
			secl = max(secl, histl[i]);
			secr = max(secr, histr[i]);
		}
		igainvol = mixer_getvol(mdevns[mixerdev]);
		lenl = maxl * (total-7) / 32768;
		lenr = maxr * (total-7) / 32768;
		if(pml)
			leni = igainvol * (total-strlen(mdevls[mixerdev])-2) / 25600;
		else
			leni = igainvol * (total-strlen(mdevns[mixerdev])-2) / 25600;
		drawwindowf(6);
		if(record)
			drawwindowf(3);
		pthread_mutex_lock(&nclock);
		mvwprintw(RECORDWIN, 2, 2, "File: %s", recordf);
		if(pml)
		{
			mvwprintw(RECORDWIN, 3, 2, "%s:", mdevls[mixerdev]);
			mvwprintw(RECORDWIN, 3, 4+strlen(mdevls[mixerdev]), "%*.*s",
					leni, leni, full);
			mvwprintw(RECORDWIN, 3, 4+strlen(mdevls[mixerdev])+leni, "%*.*s",
					total-leni-strlen(mdevls[mixerdev])-2,
					total-leni-strlen(mdevls[mixerdev])-2, empty);
		}
		else
		{
			mvwprintw(RECORDWIN, 3, 2, "%s:", mdevns[mixerdev]);
			mvwprintw(RECORDWIN, 3, 4+strlen(mdevns[mixerdev]), "%*.*s",
					leni, leni, full);
			mvwprintw(RECORDWIN, 3, 4+strlen(mdevns[mixerdev])+leni, "%*.*s",
					total-leni-strlen(mdevns[mixerdev])-2,
					total-leni-strlen(mdevns[mixerdev])-2, empty);
		}
		mvwprintw(RECORDWIN, 4, 2, "left :");
		mvwprintw(RECORDWIN, 4, 9, "%*.*s", lenl, lenl, full);
		mvwprintw(RECORDWIN, 4, 9+lenl, "%*.*s", total-lenl-7, total-lenl-7, empty);
		mvwprintw(RECORDWIN, 4, 9+secl*(total-7)/32678, "|");
		mvwprintw(RECORDWIN, 5, 2, "right:");
		mvwprintw(RECORDWIN, 5, 9, "%*.*s", lenr, lenr, full);
		mvwprintw(RECORDWIN, 5, 9+lenr, "%*.*s", total-lenr-7, total-lenr-7, empty);
		mvwprintw(RECORDWIN, 5, 9+secr*(total-7)/32678, "|");
		mvwprintw(RECORDWIN, 7, 2, "size: %s (%lld bytes)", size2fsize(recordeds),
				recordeds);
		mvwprintw(RECORDWIN, 8, 2, "maximal size: %s (%lld bytes)",
				size2fsize(maxrecs), maxrecs);
		mvwprintw(RECORDWIN, 11, 2, "< %d Hz >", dsp_rate);
		if(dsp_chn == 1) strcpy(chnstr, "mono");
		else if(dsp_chn == 2) strcpy(chnstr, "stereo");
		else snprintf(chnstr, sizeof(chnstr), "%d channels", dsp_chn);
		mvwprintw(RECORDWIN, 11, 74-strlen(chnstr), "< %s >", chnstr);
		wnoutrefresh(RECORDWIN);
		if(record)
		{
			mvwprintw(STATUSWIN, 1, 29-strlen(recordf)/2, "Recording to file %s",
					recordf);
			wnoutrefresh(STATUSWIN);
		}
		doupdate();
		pthread_mutex_unlock(&nclock);
		if(((maxl >= 32767) || (maxr >= 32767)) && aadjust && !record)
			mixer_decvol(mdevns[mixerdev], 256);
	}
	dsp_deinit();
	exitrecord = 2;
	return NULL;
}

void write_header(void)
{
	char buffer[1024];
	uint16_t fmtt=1, chn=dsp_chn, bits=16, ba;
	uint32_t riffl=recordeds+32, fmtl=16, rate=dsp_rate, abps, size=recordeds;

	ba = chn * bits / 8;
	abps = rate * ba;
	lseek(recordfd, 0, SEEK_SET);
	strcpy(buffer, "RIFF");
	write(recordfd, buffer, 4);
	write(recordfd, &riffl, 4);
	strcpy(buffer, "WAVEfmt ");
	write(recordfd, buffer, 8);
	write(recordfd, &fmtl, 4);
	write(recordfd, &fmtt, 2);
	write(recordfd, &chn, 2);
	write(recordfd, &rate, 4);
	write(recordfd, &abps, 4);
	write(recordfd, &ba, 2);
	write(recordfd, &bits, 2);
	strcpy(buffer, "data");
	write(recordfd, buffer, 4);
	write(recordfd, &size, 4);
}

void stdfont(void)
{
  strcpy(lcdchars[0][0], "  __  ");
  strcpy(lcdchars[0][1], " /  \\ ");
  strcpy(lcdchars[0][2], "| () |");
  strcpy(lcdchars[0][3], " \\__/ ");
  strcpy(lcdchars[0][4], "      ");
  strcpy(lcdchars[1][0], "   _  ");
  strcpy(lcdchars[1][1], "  / | ");
  strcpy(lcdchars[1][2], "  | | ");
  strcpy(lcdchars[1][3], "  |_| ");
  strcpy(lcdchars[1][4], "      ");
  strcpy(lcdchars[2][0], " ___  ");
  strcpy(lcdchars[2][1], "|_  ) ");
  strcpy(lcdchars[2][2], " / /  ");
  strcpy(lcdchars[2][3], "/___| ");
  strcpy(lcdchars[2][4], "      ");
  strcpy(lcdchars[3][0], " ____ ");
  strcpy(lcdchars[3][1], "|__ / ");
  strcpy(lcdchars[3][2], " |_ \\ ");
  strcpy(lcdchars[3][3], "|___/ ");
  strcpy(lcdchars[3][4], "      ");
  strcpy(lcdchars[4][0], " _ _  ");
  strcpy(lcdchars[4][1], "| | | ");
  strcpy(lcdchars[4][2], "|_  _|");
  strcpy(lcdchars[4][3], "  |_| ");
  strcpy(lcdchars[4][4], "      ");
  strcpy(lcdchars[5][0], " ___  ");
  strcpy(lcdchars[5][1], "| __| ");
  strcpy(lcdchars[5][2], "|__ \\");
  strcpy(lcdchars[5][3], "|___/ ");
  strcpy(lcdchars[5][4], "      ");
  strcpy(lcdchars[6][0], "  __  ");
  strcpy(lcdchars[6][1], " / /  ");
  strcpy(lcdchars[6][2], "/ _ \\");
  strcpy(lcdchars[6][3], "\\___/ ");
  strcpy(lcdchars[6][4], "      ");
  strcpy(lcdchars[7][0], " ____ ");
  strcpy(lcdchars[7][1], "|__  |");
  strcpy(lcdchars[7][2], "  / / ");
  strcpy(lcdchars[7][3], " /_/  ");
  strcpy(lcdchars[7][4], "      ");
  strcpy(lcdchars[8][0], " ___  ");
  strcpy(lcdchars[8][1], "( _ ) ");
  strcpy(lcdchars[8][2], "/ _ \\ ");
  strcpy(lcdchars[8][3], "\\___/ ");
  strcpy(lcdchars[8][4], "      ");
  strcpy(lcdchars[9][0], " ___  ");
  strcpy(lcdchars[9][1], "/ _ \\ ");
  strcpy(lcdchars[9][2], "\\_, / ");
  strcpy(lcdchars[9][3], " /_/  ");
  strcpy(lcdchars[9][4], "      ");
  strcpy(lcdchars[10][0], "      ");
  strcpy(lcdchars[10][1], "      ");
  strcpy(lcdchars[10][2], "      ");
  strcpy(lcdchars[10][3], "      ");
  strcpy(lcdchars[10][4], "      ");
  strcpy(lcdpoint[0], "   ");
  strcpy(lcdpoint[1], "   ");
  strcpy(lcdpoint[2], " _ ");
  strcpy(lcdpoint[3], "(_)");
  strcpy(lcdpoint[4], "   ");
	lcdcharwidth = 6;
	lcdpointwidth = 3;
}

void mainloop(void)
{
	int keywhile = 0, key, globreturn, i, j, ehh;
	int hbutk[10] = {33, 34, 167, 36, 37, 38, 47, 40, 41, 61};
	char newmaxrecsstr[256];
	float ifreq;
	glob_t fonts;

	radio_tune(ffreq);
	radio_mute(FALSE);

	if((windows[winfuncs[4].win].x > -1) && ((windows[winfuncs[4].win].y > -1)))
		pthread_create((pthread_t*) &scrollthread, NULL, doscroll, 0);
	usleep(60000);
	drawaudiomode();
	pthread_mutex_lock(&nclock);
	doupdate();
	pthread_mutex_unlock(&nclock);
	while((keywhile!='q')&&(keywhile!='')&&(keywhile!='e')&&(keywhile!='x'))
	{
		int wind = 0;

		keywhile = wgetch(TUNERWIN);
		for(i=0;i<10;i++)
			if(keywhile == hbutk[i])
				if((buttons[i+10] >= radiofreqmin) && (buttons[i+10] <= radiofreqmax))
					radio_tune(ffreq = (float) buttons[i+10]);
		
		switch (keywhile)
		{
			char fntname[256], *fg, *bg, *bold;
			case KEY_UP:
				radio_tune(ffreq>=radiofreqmax ? (ffreq=radiofreqmax) : (ffreq+=0.05));
				break;
			case KEY_DOWN:
				radio_tune(ffreq<=radiofreqmin ? (ffreq=radiofreqmin) : (ffreq-=0.05));
				break;
			case KEY_RIGHT:
				do
					if((++mixerdev) >= nmdevs) mixerdev = 0;
				while(!mixer_hasdevnr(mixerdev));
				break;
			case KEY_LEFT:
				do
					if((--mixerdev) < 0) mixerdev = nmdevs - 1;
				while(!mixer_hasdevnr(mixerdev));
				break;
			case '>':
				mixer_incvol(mdevns[mixerdev], 256);
				break;
			case '<':
				mixer_decvol(mdevns[mixerdev], 256);
				break;
			case 'g':
				drawwindowf(3);
				pthread_mutex_lock(&nclock);
				mvwprintw(STATUSWIN, 1, 2, "Go to frequency: ");
				wrefresh(STATUSWIN);
				pthread_mutex_unlock(&nclock);
				echo();
				ifreq = ffreq;
				mvwscanw(STATUSWIN, 1, 19, "%f", &ifreq);
				noecho();
				ffreq = (float) ifreq;
				if((double) ffreq > (double) radiofreqmax) ffreq = radiofreqmax;
				if((double) ffreq < (double) radiofreqmin) ffreq = radiofreqmin;
				radio_tune(ffreq);
				drawwindowf(3);
				break;
			case '1': case '2': case '3': case '4': case '5':
			case '6': case '7': case '8': case '9':
				if((buttons[(keywhile - '1')] >= radiofreqmin)
						&& (buttons[(keywhile - '1')] <= radiofreqmax))
					radio_tune(ffreq = (float) buttons[(keywhile - '1')]);
				break;
			case '0':
				if((buttons[9] >= radiofreqmin) && (buttons[9] <= radiofreqmax))
				radio_tune(ffreq = (float) buttons[9]);
				break;
			case '+':
				drawhelppage(curhelpp=((curhelpp+2) > helppagecount ? 0 : curhelpp+1));
				break;
			case '-':
				drawhelppage(curhelpp=(curhelpp == 0 ? (helppagecount-1) : curhelpp-1));
				break;
			case 'm':
				radio_mute((radiomute = !radiomute));
				radio_getflags();
				break;
			case 'd':
				denoiser = !denoiser;
				if(!radiomute && !denoiser) radio_mute(FALSE);
				drawhelppage(curhelpp);
				break;
			case 'l':
				pml = !pml;
				drawhelppage(curhelpp);
				break;
			case 'c':
				key = wind = 0;
				drawhelppage(23);
				pthread_mutex_lock(&nclock);
				mvwprintw(STATUSWIN, 1, 33, "-- %s --", winfuncs[0].name);
				pthread_mutex_unlock(&nclock);
				while(!key)
				{		 
					key = wgetch(STATUSWIN);
					if(wind>=numfunc)
					{
						fg = &(extracols[wind-numfunc].fg);
						bg = &(extracols[wind-numfunc].bg);
						bold = &(extracols[wind-numfunc].bold);
					}
					else
					{
						fg = &(winfuncs[wind].fg);
						bg = &(winfuncs[wind].bg);
						bold = &(winfuncs[wind].bold);
					}
					switch(key)
					{
						case KEY_UP:
							if(++wind>=(numfunc+nec)) wind = 0;
							key = 0;
							break;
						case KEY_DOWN:
							if(--wind<0) wind = numfunc+nec-1;
							key = 0;
							break;
						case 'f':
							if(++(*fg) > 7) *fg = 0;
							init_pair(wind+1, *fg, *bg);
							key = 0;
							break;
						case 'b':
							if(++(*bg) > 7) *bg = 0;
							init_pair(wind+1, *fg, *bg);
							key = 0;
							break;
						case 'o':
							*bold = !(*bold);
							if(wind < numfunc)
							{
								pthread_mutex_lock(&nclock);
								wbkgd(windows[winfuncs[wind].win].win,
										(winfuncs[wind].bold ? A_BOLD : 0) | COLOR_PAIR(wind+1));
								wnoutrefresh(windows[winfuncs[wind].win].win);
								pthread_mutex_unlock(&nclock);
							}
							else for(i=0;i<numfunc;i++) handle_extracols(i);
							key = 0;
					}
					drawwindowf(3);
					pthread_mutex_lock(&nclock);
					if(wind>=numfunc)
					{
						mvwprintw(STATUSWIN, 1,
								(78-strlen(extracols[wind-numfunc].name))/2-3,
								"-- %s --", extracols[wind-numfunc].name);
					}
					else
						mvwprintw(STATUSWIN, 1, (78-strlen(winfuncs[wind].name))/2-3,
								"-- %s --", winfuncs[wind].name);
					wnoutrefresh(STATUSWIN);
					pthread_mutex_unlock(&nclock);
					switch(wind)
					{
						case 5: case 6:
							drawwindowf(wind);
							break;
						default:
							drawwindowf(2);
							break;
					}
					pthread_mutex_lock(&nclock);
					doupdate();
					pthread_mutex_unlock(&nclock);
				}
				drawhelppage(curhelpp);
				drawwindowf(3);
				drawwindowf(2);
				pthread_mutex_lock(&nclock);
				doupdate();
				pthread_mutex_unlock(&nclock);
				break;
			case 'f':
				key = 0;
				drawwindowf(3);
				snprintf(dira, sizeof(dira), "%s*.raf", dir);
				globreturn = glob(dira, 0, NULL, &fonts);
				if(globreturn)
				{
					pthread_mutex_lock(&nclock);
					mvwprintw(STATUSWIN, 1, 30, "|>!NO FONTS FOUND!<|");
					pthread_mutex_unlock(&nclock);
					globfree(&fonts);
					while(!key) key = wgetch(STATUSWIN);
					drawwindowf(3);
					break;
				}
				drawhelppage(42);
				pthread_mutex_lock(&nclock);
				wnoutrefresh(KEYSWIN);
				wnoutrefresh(STATUSWIN);
				doupdate();
				pthread_mutex_unlock(&nclock);
				i = 0;
				for(j=0;j<fonts.gl_pathc;j++)
					if(!strcmp(fonts.gl_pathv[j], font)) i = j;
				while(!key)
				{
					strncpy(fntname, fonts.gl_pathv[i], sizeof(fntname));
					drawwindowf(3);
					pthread_mutex_lock(&nclock);
					mvwprintw(STATUSWIN, 1, 2, "%3d: %s", i, basename(fntname));
					pthread_mutex_unlock(&nclock);
					strcpy(font, fonts.gl_pathv[i]);
					readfont(font);
					drawfreq(ffreq);
					pthread_mutex_lock(&nclock);
					wrefresh(TUNERWIN);
					pthread_mutex_unlock(&nclock);
					key = wgetch(STATUSWIN);
					switch(key)
					{
						case KEY_UP:
							i = i == (fonts.gl_pathc-1) ? (fonts.gl_pathc-1) : i+1;
							key = 0;
							break;
						case KEY_DOWN:
							i = i == 0 ? 0 : i-1;
							key = 0;
							break;
					}
				}
				drawhelppage(curhelpp);
				drawwindowf(3);
				pthread_mutex_lock(&nclock);
				doupdate();
				pthread_mutex_unlock(&nclock);
				globfree(&fonts);
				break;
			case 'i':
				drawwindowf(5);
				key = 0;
				while((!key) || (key == ERR))
				{
					key = wgetch(INFOWIN);
				}
				drawwindowf(2);
				break;
			case 'r':
				drawwindowf(6);
				drawhelppage(ehh = 24);
				pthread_mutex_lock(&nclock);
				doupdate();
				pthread_mutex_unlock(&nclock);
				if(recordi > 99999) recordi = 0;
				snprintf(recordf, 13, "rec%05d.wav", recordi);
				exitrecord = record = 0;
				pthread_create((pthread_t*) &recordthread, NULL, dorecord, 0);
				key = 0;
				while(!key)
				{
					key = wgetch(INFOWIN);
					if(key == ERR) key = 0;
					if(!exitrecord)
						switch(key)
						{
							case 'a':
								aadjust = !aadjust;
								drawhelppage(ehh);
								key = 0;
								break;
							case ' ':
								key = 0;
								if(record) { rec_stop(); record = !record; }
								else if(!rec_start()) record = !record;
								drawhelppage(ehh);
								break;
							case 'n':
								key = 0;
								if(record)
								{
									rec_stop();
									rec_start();
								}
								break;
							case '<':
								mixer_decvol(mdevns[mixerdev], 256);
								key = 0;
								break;
							case '>':
								mixer_incvol(mdevns[mixerdev], 256);
								key = 0;
								break;
							case '+': case '-':
								if(ehh == 24) ehh = 25;
								else ehh = 24;
								key = 0;
								break;
							case KEY_RIGHT:
								do
									if((++mixerdev) >= nmdevs) mixerdev = 0;
								while(!mixer_hasdevnr(mixerdev));
								key = 0;
								break;
							case KEY_LEFT:
								do
									if((--mixerdev) < 0) mixerdev = nmdevs - 1;
								while(!mixer_hasdevnr(mixerdev));
								key = 0;
								break;
							case 's':
								key = 0;
								if(record) break;
								exitrecord = 1;
								while(exitrecord != 2);
								if(dsp_chn == 2) dsp_chn = 1;
								else dsp_chn = 2;
								exitrecord = record = 0;
								pthread_create((pthread_t*) &recordthread, NULL, dorecord, 0);
								break;
							case 'r':
								key = 0;
								if(record) break;
								exitrecord = 1;
								while(exitrecord != 2);
								if(dsp_rate == 44100) dsp_rate = 11025;
								else if(dsp_rate == 11025) dsp_rate = 22050;
								else dsp_rate = 44100;
								exitrecord = record = 0;
								pthread_create((pthread_t*) &recordthread, NULL, dorecord, 0);
								break;
							case 'm':
								key = 0;
								if(record) break;
								drawwindowf(3);
								pthread_mutex_lock(&nclock);
								mvwprintw(STATUSWIN, 1, 2, "Enter maximal filesize:");
								wrefresh(STATUSWIN);
								pthread_mutex_unlock(&nclock);
								echo();
								mvwscanw(STATUSWIN, 1, 26, "%s", &newmaxrecsstr);
								noecho();
								maxrecs = parse_newmaxrecs(newmaxrecsstr);
								drawwindowf(3);
								break;
						}
					if(record) key = 0;
					drawhelppage(ehh);
					drawfreq(ffreq);
					pthread_mutex_lock(&nclock);
					doupdate();
					pthread_mutex_unlock(&nclock);
				}
				exitrecord = 1;
				while(exitrecord != 2);
				if(record)
				{
					write_header();
					close(recordfd);
					record = !record;
				}
				drawwindowf(2);
				drawwindowf(3);
				drawhelppage(curhelpp);
				break;
			case '': case 'q': case 'e':
				hasexit = 1;
				radio_mute(TRUE);
				break;
		}
		drawfreq(ffreq);
		pthread_mutex_lock(&nclock);
		doupdate();
		pthread_mutex_unlock(&nclock);
	}
}	

long long int parse_newmaxrecs(char *newsize)
{
	int i, multi=0, fracdiv=0;
	long long int size=-1, frac=0;
	char chars[]={"KMGT"}, *pp=NULL, *tmp=NULL;

	for(i=0;i<strlen(chars);i++)
		if((toupper(newsize[strlen(newsize)-1]) == chars[i])
				|| ((toupper(newsize[strlen(newsize)-2]) == chars[i])
				&& (toupper(newsize[strlen(newsize)-1]) == 'B')))
			multi=i+1;

	pp = strchr(newsize, '.');
	if(pp)
	{
		i = 1;
		while(pp[i]) if(!isdigit(pp[i++])) break;
		fracdiv = i-2;
		tmp = malloc(i * sizeof(char));
		strncpy(tmp, pp+1, i-2);
		tmp[i-2] = '\0';
		frac = strtoll(tmp, (char **)NULL, 10);
		free(tmp);
	}
	i = 0;
	while(newsize[i]) if(!isdigit(newsize[i++])) break;
	tmp = malloc((i+1) * sizeof(char));
	strncpy(tmp, newsize, i);
	tmp[i] = '\0';
	size = strtoll(tmp, (char **)NULL, 10);
	free(tmp);
	size = (size + frac / pow(10, fracdiv)) * pow(1024, multi);
	if((size == -1) || (size < 50)) return maxrecs;
	else return size;
}

void rec_stop(void)
{
	write_header();
	close(recordfd);
	if(++recordi >= 100000) recordi = 0;
	snprintf(recordf, 13, "rec%05d.wav", recordi);
}

int rec_start(void)
{
	if((recordfd = open(recordf,
			O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1)
	{
		pthread_mutex_lock(&nclock);
		mvwprintw(STATUSWIN, 1, 2, "Can't open file \"%s\": %s",
				recordf, strerror(errno));
		wrefresh(STATUSWIN);
		pthread_mutex_unlock(&nclock);
		return -1;
	}
	recordeds = 0;
	write_header();
	return 0;
}

void deinit(void)
{
	radio_deinit();
	close(fd);
	refresh();
	endwin();
	printf("[?25h");
}

char *size2fsize(long long int size)
{
	char *fsize=NULL, chars[]={"TGMK"};
	double frac;
	int i;

	fsize = malloc(32 * sizeof(char));
	fsize[0] = '\0';
	i = strlen(chars);
	while((i >= 0) && !strlen(fsize))
		if((frac = (size / pow(1024, (--i)+1))) >= 1)
		{
			snprintf(fsize, 32, "%.2f%cB", frac, chars[strlen(chars)-i-1]);
			break;
		}
	if(!strlen(fsize)) snprintf(fsize, 32, "%lld bytes", size);
	return fsize;
}
