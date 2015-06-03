/* DMCRadio 1.0.0
 * Copyright (c) 2002 Sven Hesse (DrMcCoy)
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
#include "radiodev.h"
#include "mixerdev.h"

struct station
{
	double frequency;
	char name[79];
};

struct colors
{
	int tuner_foreground;
	int tuner_background;
	int tuner_bold;
	int keys_foreground;
	int keys_background;
	int keys_bold;
	int stations_foreground;
	int stations_background;
	int stations_bold;
	int status_foreground;
	int status_background;
	int status_bold;
	int scroll_foreground;
	int scroll_background;
	int scroll_bold;
	int info_foreground;
	int info_background;
	int info_bold;
	int volume_foreground;
	int volume_background;
	int volume_bold;
};

static WINDOW *freqw, *helpw, *stationw, *statusw, *infow, *scrollw;
static struct station stations[256];
static struct colors radiocolors;
static char lcdchars[11][5][7], lcdpoint[5][4], font[256], dir[256], dira[256];
static char mixerdev[256], rdev[256], mdev[256];
static double buttons[20], freqmin, freqmax, ffreq;
static int fd, lcdcharwidth, lcdpointwidth, exitscroller = 0;
static int actualhelppage = 0, helppagecount = 2;
static pthread_t *scrollthread;
extern int errno;

int printconshelp(void);
int init(void);
int setuplayout(void);
int doscroll(void);
int drawbut(void);
int setdefaults(void);
int handleconf(void);
char *strupper(char *string);
char *trimr(char *string);
int readfont(char *file);
int drawhelppage(int helppage);
int drawfreq(double frequency);
int drawsignal(void);
int stdfont(void);
int drawstatus(void);
int saveconf(void);
int mainloop(void);
int deinit(void);

int main(int argc, char *argv[])
{
	int justmute = 0, thisarg;

	strcpy(mixerdev,"");
	strcpy(rdev,"");
	strcpy(mdev,"");
	if(argc > 1)
	{
		for(thisarg=1;thisarg<argc;thisarg++)
		{
			if((!strcmp(argv[thisarg],"-h")) || (!strcmp(argv[thisarg],"--help")))
				printconshelp();
			if((!strcmp(argv[thisarg],"-r") || !strcmp(argv[thisarg],"--radiodev"))
					&& (argc==thisarg+1))
				printconshelp();
			if((!strcmp(argv[thisarg],"-x") || !strcmp(argv[thisarg],"--mixerdev"))
					&& (argc==thisarg+1))
				printconshelp();
			if((!strcmp(argv[thisarg],"-a") || !strcmp(argv[thisarg],"--audioinput"))
					&& (argc==thisarg+1))
				printconshelp();
			if((!strcmp(argv[thisarg],"-m")) || (!strcmp(argv[thisarg],"--mute")))
			{
				justmute = TRUE;
			}
			if((!strcmp(argv[thisarg],"-v")) || (!strcmp(argv[thisarg],"--version")))
			{
				printf("DMCRadio %s (c) %s by Sven Hesse (DrMcCoy) ",VERSION,YEAR);
				printf("<SvHe_TM@gmx.de>\n");
				printf("%s, %s\n",DAY,DATE);
				exit(-1);
			}
			if(!strcmp(argv[thisarg],"-r") || !strcmp(argv[thisarg],"--radiodev"))
				if(argc>thisarg)
					strcpy(rdev,argv[thisarg+1]);
			if(!strcmp(argv[thisarg],"-x") || !strcmp(argv[thisarg],"--mixerdev"))
				if(argc>thisarg)
					strcpy(mdev,argv[thisarg+1]);
			if(!strcmp(argv[thisarg],"-a") || !strcmp(argv[thisarg],"--audioinput"))
				if(argc>thisarg)
					strcpy(mixerdev,argv[thisarg+1]);
		}
	}

	setdefaults();
	handleconf();
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

int printconshelp(void)
{
	printf("Usage: DMCRadio [options]\n\n");
	printf("	-h	--help			This help\n");
	printf("	-m	--mute			Just mute and exit\n");
	printf("	-r dev	--radiodev dev		Set Radiodevice to dev [/dev/radio]\n");
	printf("	-x dev	--mixerdev dev		Set Mixerdevice to dev [/dev/mixer]\n");
	printf("	-a inp	--audioinput inp	Set Audioinput to inp [vol]\n");
	printf("	-v	--version		Print version and exit\n\n");
	exit(-1);
}

int init(void)
{
	if(!strcmp(rdev,""))
		strcpy(rdev,"/dev/radio");
	if(!strcmp(mdev,""))
		strcpy(mdev,"/dev/mixer");
	errno=0;
	radio_init(rdev);
	if(errno)
	{
		fprintf(stderr,"Can't open %s: %s!\n",rdev,strerror(errno));
		exit(-1);
	}
	mixer_init(mdev);
	if(errno)
	{
		fprintf(stderr,"Can't open %s: %s!\n",mdev,strerror(errno));
		exit(-1);
	}
	if(!mixer_hasdev(mixerdev)) strcpy(mixerdev,"vol");
	
	radio_getflags();
	printf("[?25l");
	initscr();
	start_color();
	cbreak();
	noecho();

	freqmin = radio_getlowestfrequency();
	freqmax = radio_gethighestfrequency();
	if((ffreq < freqmin) || (ffreq > freqmax))
		ffreq = freqmin;
	return 0;
}

int setuplayout(void)
{
	FILE *testfile;

	freqw = newwin(7,42,0,0);
	helpw = newwin(7,36,0,44);
	stationw = newwin(12,80,8,0);
	statusw = newwin(3,80,21,0);
	scrollw = newwin(1,80,24,0);
	init_pair(1,radiocolors.tuner_foreground,radiocolors.tuner_background);
	init_pair(2,radiocolors.keys_foreground,radiocolors.keys_background);
	init_pair(3,radiocolors.stations_foreground,radiocolors.stations_background);
	init_pair(4,radiocolors.status_foreground,radiocolors.status_background);
	init_pair(5,radiocolors.scroll_foreground,radiocolors.scroll_background);
	init_pair(6,radiocolors.status_foreground,radiocolors.status_background);
	wbkgd(freqw,(radiocolors.tuner_bold ? A_BOLD : 0) | COLOR_PAIR(1));
	wbkgd(helpw,(radiocolors.keys_bold ? A_BOLD : 0)| COLOR_PAIR(2));
	wbkgd(stationw,(radiocolors.stations_bold ? A_BOLD : 0) | COLOR_PAIR(3));
	wbkgd(statusw,(radiocolors.status_bold ? A_BOLD : 0) | COLOR_PAIR(4));
	wbkgd(scrollw,(radiocolors.scroll_bold ? A_BOLD : 0 ) | COLOR_PAIR(5));
	wborder(stationw,0,0,0,0,0,0,0,0); 
	mvwprintw(stationw,0,1," Preset Stations ");
	drawstatus();
	drawhelppage(actualhelppage);
	strcpy(dira,dir);
	strcat(dira,font);
	strcpy(font,dira);
	testfile = fopen(font,"r");
	if(testfile == NULL)
		stdfont();
	else
	{
		readfont(font);
		fclose(testfile);
	}
	drawbut();
	drawfreq(ffreq);
	wnoutrefresh(stationw);
	wnoutrefresh(freqw);
	wnoutrefresh(statusw);
	wnoutrefresh(helpw);
	wnoutrefresh(scrollw);
	doupdate();
	keypad(freqw,TRUE);
	keypad(statusw,TRUE);
	return 0;
}

int drawstatus(void)
{
	werase(statusw);
	wborder(statusw,0,0,0,0,0,0,0,0); 
	mvwprintw(statusw,0,1," Statusbar ");
	mvwprintw(statusw,2,2,"< DMCRadio %s >",VERSION);
	mvwprintw(statusw,2,42,"< (c) %s by Sven Hesse (DrMcCoy) >",YEAR);
	return 0;
}

int saveconf(void)
{
	int i;
	char config[256];
	FILE *configfile;

	strcpy(config,(char *) getenv("HOME"));
	strcat(config,"/.DMCRadiorc");
	configfile = fopen(config,"w");

	if(configfile != NULL)
	{
		fprintf(configfile,"[Style]\n\n");
		fprintf(configfile,"LCDFont=%s\n",(char *) basename(font));
		fprintf(configfile,"Tunerfg=%d\n",radiocolors.tuner_foreground);
		fprintf(configfile,"Tunerbg=%d\n",radiocolors.tuner_background);
		fprintf(configfile,"Tunerbold=%d\n",radiocolors.tuner_bold);
		fprintf(configfile,"Keysfg=%d\n",radiocolors.keys_foreground);
		fprintf(configfile,"Keysbg=%d\n",radiocolors.keys_background);
		fprintf(configfile,"Keysbold=%d\n",radiocolors.keys_bold);
		fprintf(configfile,"Stationsfg=%d\n",radiocolors.stations_foreground);
		fprintf(configfile,"Stationsbg=%d\n",radiocolors.stations_background);
		fprintf(configfile,"Stationsbold=%d\n",radiocolors.stations_bold);
		fprintf(configfile,"Statusfg=%d\n",radiocolors.status_foreground);
		fprintf(configfile,"Statusbg=%d\n",radiocolors.status_background);
		fprintf(configfile,"Statusbold=%d\n",radiocolors.status_bold);
		fprintf(configfile,"Infofg=%d\n",radiocolors.info_foreground);
		fprintf(configfile,"Infobg=%d\n",radiocolors.info_background);
		fprintf(configfile,"Infobold=%d\n",radiocolors.info_bold);
		fprintf(configfile,"Scrollfg=%d\n",radiocolors.scroll_foreground);
		fprintf(configfile,"Scrollbg=%d\n",radiocolors.scroll_background);
		fprintf(configfile,"Scrollbold=%d\n",radiocolors.scroll_bold);
		fprintf(configfile,"Volumefg=%d\n",radiocolors.volume_foreground);
		fprintf(configfile,"Volumebg=%d\n",radiocolors.volume_background);
		fprintf(configfile,"Volumebold=%d\n\n",radiocolors.volume_bold);
		fprintf(configfile,"[Stations]\n\n");
		for(i=0;i<256;i++)
			if(stations[i].frequency != 0)
				fprintf(configfile,"%.2f=%s\n",stations[i].frequency,stations[i].name);
		fprintf(configfile,"\n[Buttons]\n\n");
		for(i=0;i<20;i++)
			if(buttons[i] != 0)
				fprintf(configfile,"%d=%.2f\n",i,buttons[i]);
		fprintf(configfile,"\n[Misc]\n\n");
		fprintf(configfile,"Radiodev=%s\n",rdev);
		fprintf(configfile,"Mixerdev=%s\n",mdev);
		fprintf(configfile,"Audioinput=%s\n",mixerdev);
		fprintf(configfile,"Frequency=%.2f\n",ffreq);
	}
	fclose(configfile);
	return 0;
}

int doscroll(void)
{
	int i, scrollerx = 1, scrollerstep = 1, j;
	double actualfrequency=0.0;
	char frequencystring[78];

	while(1)
	{
		drawsignal();
		if(actualfrequency != ffreq) scrollerx = scrollerstep = 1;
		actualfrequency = ffreq;
		werase(scrollw);
		j = 0;
		strcpy(frequencystring,"");
		for(i=0;i<256;i++)
		{
			if(((int)((actualfrequency*100)+0.5))==
				((int)((stations[i].frequency*100)+0.5)))
			{
				mvwprintw(scrollw,0,scrollerx,"%s",stations[i].name);
				if (strlen(stations[i].name) < 78)
					scrollerx += scrollerstep;
				if(scrollerx>(78-(strlen(stations[i].name)))) scrollerstep=-1;
				if(scrollerx<2) scrollerstep=1;
				wrefresh(scrollw);
				usleep(100000);
				j = 1;
			}
		}
		if(exitscroller) break;
		if(j) continue;
		sprintf(frequencystring,"%.2f MHz",actualfrequency);
		mvwprintw(scrollw,0,scrollerx,"%s",frequencystring);
		if (strlen(frequencystring) < 78)
			scrollerx += scrollerstep;
		if(scrollerx>(78-(strlen(frequencystring)))) scrollerstep=-1;
		if(scrollerx<2) scrollerstep=1;
		wrefresh(scrollw);
		usleep(100000);
		if(exitscroller) break;
	}
	return 0;
}

int drawbut(void)
{
	int i, j;
	char clear[34], stationname[28];

	for(i=0;i<20;i++)
	{
		if(buttons[i] != 0)
			mvwprintw(stationw,i%10+1,34*(i/10)+2,"%2d: %7.2f MHz",i, buttons[i]);
		strcpy(clear,"");
		for(j=0;j<28;j++) strcat(clear," ");
		for(j=0;j<256;j++)
		{
			if((stations[j].frequency == buttons[i]) && (stations[j].frequency != 0))
			{
				mvwprintw(stationw,i%10+1,34*(i/10)+7,"%s",clear);
				strncpy(stationname,stations[j].name,28);
				stationname[28] = '\0';
				if(strlen(stations[j].name) > 28)
				{
					stationname[25] = '.';
					stationname[26] = '.';
					stationname[27] = '.';
				}
				mvwprintw(stationw,i%10+1,34*(i/10)+7,"%s",stationname);
			}
		}
	}
	return 0;
}

int setdefaults(void)
{
  radiocolors.tuner_foreground = 6;
	radiocolors.keys_foreground = radiocolors.stations_foreground = 7;
	radiocolors.status_foreground = radiocolors.scroll_foreground = 7;
  radiocolors.info_foreground = 7;
	radiocolors.volume_foreground = 5;
	radiocolors.tuner_background = radiocolors.keys_background = 4;
	radiocolors.stations_background = radiocolors.status_background = 4;
	radiocolors.scroll_background = 0;
	radiocolors.info_background = 4;
	radiocolors.volume_background = 4;
	radiocolors.tuner_bold = radiocolors.keys_bold = 1;
	radiocolors.stations_bold = radiocolors.status_bold = 1;
	radiocolors.scroll_bold = radiocolors.info_bold = 1;
	radiocolors.volume_bold = 0;
	return 0;
}

int handleconf(void)
{
	char buffer[4096];
	int k = 0, section = 42;
	FILE *configfile, *testfile;
	char suffix[256], prefix[256], config[256], testfont[256];

	strcpy(config,(char *) getenv("HOME"));
	strcat(config,"/.DMCRadiorc");
	strcpy(dir,"/usr/local/share/DMCRadio/");
	strcpy(font,"small.raf");

	configfile = fopen(config,"r");
	if (configfile == NULL) return -1;

	while(!feof(configfile))
	{
		strcpy(buffer,"");
		strcpy(suffix,"");
		strcpy(prefix,"");
		fgets(buffer,sizeof(buffer),configfile);
		if(!strcasecmp(buffer,"[STYLE]\n")) { section = 0; continue; }
		if(!strcasecmp(buffer,"[STATIONS]\n")) { section = 1; continue; }
		if(!strcasecmp(buffer,"[BUTTONS]\n")) { section = 2; continue; }
		if(!strcasecmp(buffer,"[MISC]\n")) { section = 3; continue; }
		if(!strcmp(buffer,"\n")) continue;
		if(!strcmp(buffer,"")) continue;
		if(strchr(buffer,'=') == NULL) continue;
		if(strchr(buffer,'\n') == NULL) continue;
		if(buffer[0] == '=') continue;
		strcpy(suffix,strupper(strtok(buffer,"=")));
		strcpy(buffer,strtok(NULL,""));
		if(strlen(buffer) == 1) continue;
		strcpy(prefix,strtok(buffer,"\n"));
		if(section == 0)
		{
			if(!strcmp(suffix,"LCDFONT") && strcmp(prefix,""))
			{
				strcpy(testfont,dir);
				strcat(testfont,prefix);
				if((testfile = fopen(testfont,"r")) != NULL)
				{
					strcpy(font,prefix);
					fclose(testfile);
				}
				errno=0;
			}
			if(!strcmp(suffix,"TUNERFG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.tuner_foreground = atoi(prefix);
			if(!strcmp(suffix,"TUNERBG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.tuner_background = atoi(prefix);
			if(!strcmp(suffix,"KEYSFG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.keys_foreground = atoi(prefix);
			if(!strcmp(suffix,"KEYSBG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.keys_background = atoi(prefix);
			if(!strcmp(suffix,"STATIONSFG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.stations_foreground = atoi(prefix);
			if(!strcmp(suffix,"STATIONSBG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.stations_background = atoi(prefix);
			if(!strcmp(suffix,"STATUSFG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.status_foreground = atoi(prefix);
			if(!strcmp(suffix,"STATUSBG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.status_background = atoi(prefix);
			if(!strcmp(suffix,"INFOFG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.info_foreground = atoi(prefix);
			if(!strcmp(suffix,"INFOBG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.info_background = atoi(prefix);
			if(!strcmp(suffix,"SCROLLFG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.scroll_foreground = atoi(prefix);
			if(!strcmp(suffix,"SCROLLBG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.scroll_background = atoi(prefix);
			if(!strcmp(suffix,"VOLUMEFG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.volume_foreground = atoi(prefix);
			if(!strcmp(suffix,"VOLUMEBG"))
				if((atoi(prefix) >= 0) && (atoi(prefix) < 8))
					radiocolors.volume_background = atoi(prefix);
			if(!strcmp(suffix,"TUNERBOLD")) radiocolors.tuner_bold = atoi(prefix);
			if(!strcmp(suffix,"KEYSBOLD")) radiocolors.keys_bold = atoi(prefix);
			if(!strcmp(suffix,"STATIONSBOLD")) radiocolors.stations_bold=atoi(prefix);
			if(!strcmp(suffix,"STATUSBOLD")) radiocolors.status_bold = atoi(prefix);
			if(!strcmp(suffix,"INFOBOLD")) radiocolors.info_bold = atoi(prefix);
			if(!strcmp(suffix,"SCROLLBOLD")) radiocolors.scroll_bold = atoi(prefix);
			if(!strcmp(suffix,"VOLUMEBOLD")) radiocolors.volume_bold = atoi(prefix);
		}
		else if(section == 1)
		{
			if(k < 256)
			{
				prefix[78] = '\0';
				sscanf(suffix,"%lf",&stations[k].frequency);
				strcpy(stations[k].name,prefix);
				k++;
			}
		}
		else if(section == 2)
		{
			int buti;

			buti = (int) strtoul(suffix,NULL,10);
			if((buti >= 0) && (buti <= 20))
			{
				sscanf(prefix,"%lf",buttons+buti);
			}
		}
		else if(section == 3)
		{
			if(!strcmp(suffix,"FREQUENCY")) sscanf(prefix,"%lf",&ffreq);
			if(!strcmp(suffix,"RADIODEV") && !strcmp(rdev,"")) strcpy(rdev,prefix);
			if(!strcmp(suffix,"MIXERDEV") && !strcmp(mdev,"")) strcpy(mdev,prefix);
			if(!strcmp(suffix,"AUDIOINPUT") && !strcmp(mixerdev,""))
				strcpy(mixerdev,prefix);
		}
	}
	fclose(configfile);
	return 0;
}

char *strupper(char *string)
{
	int i;

	for(i=0;i<strlen(string);i++)
		string[i] = toupper(string[i]);
	return string;
}

char *trimr(char *string)
{
	int i;

	for(i=strlen(string)-1;i>=0;i++)
	{
		if(iscntrl(string[i]) || (string[i] == ' ') || (string[i] == '\t'))
			string[i] = '\0';
		else break;
	}
	return string;
}

int readfont(char *file)
{
	int i, j, k, lengthdifference;
	char buffer[4096];
	FILE *fontfile;

	fontfile = fopen(file,"r");

	lcdcharwidth = 0;
	for(i=0;i<10;i++)
		for(j=0;j<5;j++)
		{
			fgets(lcdchars[i][j],sizeof(buffer),fontfile);
			lcdchars[i][j][6] = '\0';
			trimr(lcdchars[i][j]);
			lcdcharwidth = (lcdcharwidth > strlen(lcdchars[i][j])
					 ? lcdcharwidth : strlen(lcdchars[i][j]));
			lengthdifference = 6 - strlen(lcdchars[i][j]);
			for(k=0;k<lengthdifference;k++) strcat(lcdchars[i][j]," ");
		}

	lcdpointwidth = 0;
	for(i=0;i<=4;i++)
	{
		fgets(lcdpoint[i],sizeof(buffer),fontfile);
		lcdpoint[i][3] = '\0';
		trimr(lcdpoint[i]);
		lcdpointwidth = (lcdpointwidth > strlen(lcdpoint[i])
				 ? lcdpointwidth : strlen(lcdpoint[i]));
		lengthdifference = 3 - strlen(lcdpoint[i]);
		for(j=0;j<lengthdifference;j++) strcat(lcdpoint[i]," ");
		lcdpoint[i][lcdpointwidth] = '\0';
	}

	for(i=0;i<5;i++)
		strcpy(lcdchars[10][i],"			");

	fclose(fontfile);
	return 0;
}

int drawhelppage(int helppage)
{
	werase(helpw);
	wborder(helpw,0,0,0,0,0,0,0,0);
	mvwprintw(helpw,0,1," Key Commands ");
	switch (helppage)
	{
		case 0:
			mvwprintw(helpw,0,31,"(-)");
			mvwprintw(helpw,6,31,"(+)");
			mvwprintw(helpw,1,1," UP Key    - increment frequency");
			mvwprintw(helpw,2,1," DOWN Key  - decrement frequency");
			mvwprintw(helpw,3,1," g         - go to frequency...");
			mvwprintw(helpw,4,1," x         - exit");
			mvwprintw(helpw,5,1," ESC, q, e - mute and exit");
			break;
		case 1:
			mvwprintw(helpw,0,31,"(-)");
			mvwprintw(helpw,6,31,"(+)");
			mvwprintw(helpw,1,1," m         - mute");
			mvwprintw(helpw,2,1," f         - chance font");
			mvwprintw(helpw,3,1," c         - change colors");
			mvwprintw(helpw,4,1," i         - infos about card");
			break;
		case 23:
			mvwprintw(helpw,1,1," UP Key    - next window ");
			mvwprintw(helpw,2,1," DOWN Key  - previous window");
			mvwprintw(helpw,3,1," f         - cycle forecolor");
			mvwprintw(helpw,4,1," b         - cycle backcolor");
			mvwprintw(helpw,5,1," o         - toggle bold");
			break;
		case 42:
			mvwprintw(helpw,1,1," UP Key    - next font");
			mvwprintw(helpw,2,1," DOWN Key  - previous font");
			break;
	}
	return 0;
}

int drawfreq(double frequency)
{
	int i, j, xoffset;
	int frequencynumber[5], frequencyinteger;

	frequencyinteger = (int)((frequency + 0.005) * 100);
	for(i=0;i<4;i++)
	{
		frequencynumber[i] = (int) (frequencyinteger / pow(10,(4-i)));
		frequencyinteger = frequencyinteger - frequencynumber[i] * pow(10,(4-i));
	}
	frequencynumber[4] = frequencyinteger;
	if (frequencynumber[0] == 0) frequencynumber[0] = 10;
	xoffset = 7 + (5*(6-lcdcharwidth)) + (3-lcdpointwidth);

	werase(freqw);

	for(i=0;i<5;i++)
	{
		for(j=0;j<5;j++)
			mvwprintw(freqw,j+1,xoffset,lcdchars[frequencynumber[i]][j]);
		switch (i)
		{
			case 0: case 1: case 3:
				xoffset = xoffset + lcdcharwidth;
				break;
			case 2:
				xoffset = xoffset + lcdcharwidth + lcdpointwidth;
				break;
		}
	}
	xoffset = xoffset - lcdpointwidth - lcdcharwidth;
	for(j=0;j<5;j++)
		mvwprintw(freqw,j+1,xoffset,lcdpoint[j]);
	wborder(freqw,0,0,0,0,0,0,0,0); 
	mvwprintw(freqw,0,1," Tuner ");
	mvwhline(freqw,0,21,acs_map['u'],1);
	mvwhline(freqw,0,40,acs_map['t'],1);
	mvwprintw(freqw,0,22," Signal: ");
	mvwhline(freqw,6,9,acs_map['u'],1);
	mvwhline(freqw,6,32,acs_map['t'],1);
	mvwhline(freqw,2,0,ACS_LRCORNER,1);
	mvwhline(freqw,3,0,acs_map['+'],1);
	mvwhline(freqw,4,0,ACS_URCORNER,1);
	drawsignal();

	switch(radio_getaudiomode())
	{
		case 0:
			mvwprintw(freqw,3,1,"UNDEF.");
			break;
		case 1:
			mvwprintw(freqw,3,1,"MONO");
			break;
		case 2:
			mvwprintw(freqw,3,1,"STEREO");
			break;
	}
	return 0;
}

int drawsignal(void)
{
	int signal, i;

	signal = radio_getsignal();
	for(i=0;i<9;i++)
	{
		mvwprintw(freqw,0,i+31,"%s", signal>i ? "*" : " ");
	}
	mvwprintw(freqw,6,10," Volume: [<] %3d%% [>] ",
			(int)(((float)mixer_getvol(mixerdev)/(float)25600+0.005)*100));
	init_pair(8,radiocolors.volume_foreground,radiocolors.volume_background);
	mvwchgat(freqw,6,20,1,(radiocolors.volume_bold ? A_BOLD : 0),8,NULL);
	mvwchgat(freqw,6,29,1,(radiocolors.volume_bold ? A_BOLD : 0),8,NULL);
	wrefresh(freqw);
	return 0;
}

int stdfont(void)
{
  strcpy(lcdchars[0][0],"  __  ");
  strcpy(lcdchars[0][1]," /  \\ ");
  strcpy(lcdchars[0][2],"| () |");
  strcpy(lcdchars[0][3]," \\__/ ");
  strcpy(lcdchars[0][4],"      ");
  strcpy(lcdchars[1][0],"   _  ");
  strcpy(lcdchars[1][1],"  / | ");
  strcpy(lcdchars[1][2],"  | | ");
  strcpy(lcdchars[1][3],"  |_| ");
  strcpy(lcdchars[1][4],"      ");
  strcpy(lcdchars[2][0]," ___  ");
  strcpy(lcdchars[2][1],"|_  ) ");
  strcpy(lcdchars[2][2]," / /  ");
  strcpy(lcdchars[2][3],"/___| ");
  strcpy(lcdchars[2][4],"      ");
  strcpy(lcdchars[3][0]," ____ ");
  strcpy(lcdchars[3][1],"|__ / ");
  strcpy(lcdchars[3][2]," |_ \\ ");
  strcpy(lcdchars[3][3],"|___/ ");
  strcpy(lcdchars[3][4],"      ");
  strcpy(lcdchars[4][0]," _ _  ");
  strcpy(lcdchars[4][1],"| | | ");
  strcpy(lcdchars[4][2],"|_  _|");
  strcpy(lcdchars[4][3],"  |_| ");
  strcpy(lcdchars[4][4],"      ");
  strcpy(lcdchars[5][0]," ___  ");
  strcpy(lcdchars[5][1],"| __| ");
  strcpy(lcdchars[5][2],"|__ \\");
  strcpy(lcdchars[5][3],"|___/ ");
  strcpy(lcdchars[5][4],"      ");
  strcpy(lcdchars[6][0],"  __  ");
  strcpy(lcdchars[6][1]," / /  ");
  strcpy(lcdchars[6][2],"/ _ \\");
  strcpy(lcdchars[6][3],"\\___/ ");
  strcpy(lcdchars[6][4],"      ");
  strcpy(lcdchars[7][0]," ____ ");
  strcpy(lcdchars[7][1],"|__  |");
  strcpy(lcdchars[7][2],"  / / ");
  strcpy(lcdchars[7][3]," /_/  ");
  strcpy(lcdchars[7][4],"      ");
  strcpy(lcdchars[8][0]," ___  ");
  strcpy(lcdchars[8][1],"( _ ) ");
  strcpy(lcdchars[8][2],"/ _ \\ ");
  strcpy(lcdchars[8][3],"\\___/ ");
  strcpy(lcdchars[8][4],"      ");
  strcpy(lcdchars[9][0]," ___  ");
  strcpy(lcdchars[9][1],"/ _ \\ ");
  strcpy(lcdchars[9][2],"\\_, / ");
  strcpy(lcdchars[9][3]," /_/  ");
  strcpy(lcdchars[9][4],"      ");
  strcpy(lcdchars[10][0],"      ");
  strcpy(lcdchars[10][1],"      ");
  strcpy(lcdchars[10][2],"      ");
  strcpy(lcdchars[10][3],"      ");
  strcpy(lcdchars[10][4],"      ");
  strcpy(lcdpoint[0],"   ");
  strcpy(lcdpoint[1],"   ");
  strcpy(lcdpoint[2]," _ ");
  strcpy(lcdpoint[3],"(_)");
  strcpy(lcdpoint[4],"   ");
	lcdcharwidth = 6;
	lcdpointwidth = 3;
	return 0;
}

int mainloop(void)
{
	int keywhile = 0, radiomute = 0, key, globreturn;
	float ifreq;
	glob_t fonts;

	radio_tune(ffreq);
	radio_mute(FALSE);

	pthread_create((pthread_t*) &scrollthread,NULL,(void*(*)(void*)) doscroll, 0);
	
	while((keywhile!='q')&&(keywhile!='')&&(keywhile!='e')&&(keywhile!='x'))
	{
		int wind = 0;

		keywhile = wgetch(freqw);
		switch (keywhile)
		{
			int i, j;
			char fntname[256];

			case KEY_UP:
				ffreq = ffreq + 0.050000;
				if (ffreq > freqmax) ffreq = freqmax;
				radio_tune(ffreq);
				break;
			case KEY_DOWN:
				ffreq = ffreq - 0.050000;
				if (ffreq < freqmin) ffreq = freqmin;
				radio_tune(ffreq);
				break;
			case '>':
				mixer_incvol(mixerdev,256);
				break;
			case '<':
				mixer_decvol(mixerdev,256);
				break;
			case 'g':
				drawstatus();
				mvwprintw(statusw,1,1," Go to frequency: ");
				wrefresh(statusw);
				echo();
				ifreq = ffreq;
				mvwscanw(statusw,1,19,"%f",&ifreq);
				noecho();
				ffreq = (float) ifreq;
				if((double) ffreq > (double) freqmax) ffreq = freqmax;
				if((double) ffreq < (double) freqmin) ffreq = freqmin;
				radio_tune(ffreq);
				drawstatus();
				break;
			case '0': case '1': case '2': case '3': case '4':
		 	case '5': case '6': case '7': case '8': case '9':
				if((buttons[(keywhile-48)]>=freqmin)&&(buttons[(keywhile-48)]<=freqmax))
				{
					ffreq = (float) buttons[(keywhile - 48)];
					radio_tune(ffreq);
				}
				break;
			case '=':
				if((buttons[10] >= freqmin) && (buttons[10] <= freqmax))
				{
					ffreq = (float) buttons[10];
					radio_tune(ffreq);
				}
				break;
			case '!':
				if((buttons[11] >= freqmin) && (buttons[11] <= freqmax))
				{
					ffreq = (float) buttons[11];
					radio_tune(ffreq);
				}
				break;
			case '\"':
				if((buttons[12] >= freqmin) && (buttons[12] <= freqmax))
				{
					ffreq = (float) buttons[12];
					radio_tune(ffreq);
				}
				break;
			case 167:
				if((buttons[13] >= freqmin) && (buttons[13] <= freqmax))
				{
					ffreq = (float) buttons[13];
					radio_tune(ffreq);
				}
				break;
			case '$':
				if((buttons[14] >= freqmin) && (buttons[14] <= freqmax))
				{
					ffreq = (float) buttons[14];
					radio_tune(ffreq);
				}
				break;
			case '%':
				if((buttons[15] >= freqmin) && (buttons[15] <= freqmax))
				{
					ffreq = (float) buttons[15];
					radio_tune(ffreq);
				}
				break;
			case '&':
				if((buttons[16] >= freqmin) && (buttons[16] <= freqmax))
				{
					ffreq = (float) buttons[16];
					radio_tune(ffreq);
				}
				break;
			case '/':
				if((buttons[17] >= freqmin) && (buttons[17] <= freqmax))
				{
					ffreq = (float) buttons[17];
					radio_tune(ffreq);
				}
				break;
			case '(':
				if((buttons[18] >= freqmin) && (buttons[18] <= freqmax))
				{
					ffreq = (float) buttons[18];
					radio_tune(ffreq);
				}
				break;
			case ')':
				if((buttons[19] >= freqmin) && (buttons[19] <= freqmax))
				{
					ffreq = (float) buttons[19];
					radio_tune(ffreq);
				}
				break;
			case '+':
				actualhelppage++;
				if ((actualhelppage+1) > helppagecount) actualhelppage = 0;
				drawhelppage(actualhelppage);
				break;
			case '-':
				actualhelppage--;
				if ((actualhelppage+1) <= 0) actualhelppage = helppagecount - 1;
				drawhelppage(actualhelppage);
				break;
			case 'm':
				radiomute = !radiomute;
				radio_mute(radiomute);
				break;
			case 'c':
				exitscroller = 1;
				key = 0;
				drawhelppage(23);
				infow = newwin(10,38,7,21);
				init_pair(7,radiocolors.info_foreground,radiocolors.info_background);
				wbkgd(infow,(radiocolors.info_bold ? A_BOLD : 0) | COLOR_PAIR(7));
				wborder(infow,0,0,0,0,0,0,0,0);
				mvwprintw(infow,0,1," Informations about your radio-card ");
				mvwprintw(statusw,1,33,"-- Tuner --");
				wnoutrefresh(helpw);
				wnoutrefresh(statusw);
				wnoutrefresh(infow);
				doupdate();
				while(key==0)
				{		 
					char winds[10];

					key = wgetch(statusw);
					switch(key)
					{
						case KEY_UP:
							if(++wind>6) wind = 0;
							key = 0;
							break;
						case KEY_DOWN:
							if(--wind<0) wind = 6;
							key = 0;
							break;
						case 'f':
							switch(wind)
							{
								case 0:
									if(++radiocolors.tuner_foreground>7)
									 	radiocolors.tuner_foreground = 0;
									init_pair(1,radiocolors.tuner_foreground,
										radiocolors.tuner_background);
									break;
								case 1:
									if(++radiocolors.volume_foreground>7)
									 	radiocolors.volume_foreground = 0;
									init_pair(8,radiocolors.volume_foreground,
										radiocolors.volume_background);
									break;
								case 2:
									if(++radiocolors.keys_foreground>7)
										radiocolors.keys_foreground = 0;
									init_pair(2,radiocolors.keys_foreground,
										radiocolors.keys_background);
									break;
								case 3:
									if(++radiocolors.stations_foreground>7)
										radiocolors.stations_foreground = 0;
									init_pair(3,radiocolors.stations_foreground,
										radiocolors.stations_background);
									break;
								case 4:
									if(++radiocolors.status_foreground>7)
										radiocolors.status_foreground = 0;
									init_pair(4,radiocolors.status_foreground,
										radiocolors.status_background);
									break;
								case 5:
									if(++radiocolors.scroll_foreground>7)
										radiocolors.scroll_foreground = 0;
									init_pair(5,radiocolors.scroll_foreground,
										radiocolors.scroll_background);
									break;
								case 6:
									if(++radiocolors.info_foreground>7)
										radiocolors.info_foreground = 0;
									init_pair(7,radiocolors.info_foreground,
										radiocolors.info_background);
									break;
							}
							key = 0;
							break;
						case 'b':
							switch(wind)
							{
								case 0:
									if(++radiocolors.tuner_background>7)
										radiocolors.tuner_background = 0;
									init_pair(1,radiocolors.tuner_foreground,
										radiocolors.tuner_background);
									break;
								case 1:
									if(++radiocolors.volume_background>7)
										radiocolors.volume_background = 0;
									init_pair(8,radiocolors.volume_foreground,
										radiocolors.volume_background);
									break;
								case 2:
									if(++radiocolors.keys_background>7)
										radiocolors.keys_background = 0;
									init_pair(2,radiocolors.keys_foreground,
										radiocolors.keys_background);
									break;
								case 3:
									if(++radiocolors.stations_background>7)
										radiocolors.stations_background = 0;
									init_pair(3,radiocolors.stations_foreground,
										radiocolors.stations_background);
									break;
								case 4:
									if(++radiocolors.status_background>7)
										radiocolors.status_background = 0;
									init_pair(4,radiocolors.status_foreground,
										radiocolors.status_background);
									break;
								case 5:
									if(++radiocolors.scroll_background>7)
										radiocolors.scroll_background = 0;
									init_pair(5,radiocolors.scroll_foreground,
										radiocolors.scroll_background);
									break;
								case 6:
									if(++radiocolors.info_background>7)
										radiocolors.info_background = 0;
									init_pair(7,radiocolors.info_foreground,
										radiocolors.info_background);
									break;
							}
							key = 0;
							break;
						case 'o':
							switch(wind)
							{
								case 0:
									radiocolors.tuner_bold = !radiocolors.tuner_bold;
									wbkgd(freqw,(radiocolors.tuner_bold ? A_BOLD : 0)
										| COLOR_PAIR(1));
									wrefresh(freqw);
									break;
								case 1:
									radiocolors.volume_bold = !radiocolors.volume_bold;
									wrefresh(freqw);
									break;
								case 2:
									radiocolors.keys_bold = !radiocolors.keys_bold;
									wbkgd(helpw,(radiocolors.keys_bold ? A_BOLD : 0)
										| COLOR_PAIR(2));
									wrefresh(helpw);
									break;
								case 3:
									radiocolors.stations_bold = !radiocolors.stations_bold;
									wbkgd(stationw,(radiocolors.stations_bold ? A_BOLD : 0)
										| COLOR_PAIR(3));
									touchwin(infow);
									wnoutrefresh(stationw);
									wnoutrefresh(infow);
									doupdate();
									break;
								case 4:
									radiocolors.status_bold = !radiocolors.status_bold;
									wbkgd(statusw,(radiocolors.status_bold ? A_BOLD : 0)
										| COLOR_PAIR(4));
									wrefresh(statusw);
									break;
								case 5:
									radiocolors.scroll_bold = !radiocolors.scroll_bold;
									wbkgd(scrollw,(radiocolors.scroll_bold ? A_BOLD : 0)
										| COLOR_PAIR(5));
									wrefresh(scrollw);
									break;
								case 6:
									radiocolors.info_bold = !radiocolors.info_bold;
									wbkgd(infow,(radiocolors.info_bold ? A_BOLD : 0)
										| COLOR_PAIR(7));
									wrefresh(infow);
									break;
							}
							key = 0;
					}
					drawstatus();
					drawsignal();
					switch(wind)
					{
						case 0:
							strcpy(winds,"Tuner");
							break;
						case 1:
							strcpy(winds,"Volume");
							break;
						case 2:
							strcpy(winds,"Keys");
							break;
						case 3:
							strcpy(winds,"Stations");
							break;
						case 4:
							strcpy(winds,"Statusbar");
							break;
						case 5:
							strcpy(winds,"Scroller");
							break;
						case 6:
							strcpy(winds,"Infos");
							break;
					}
					mvwprintw(statusw,1,(78-strlen(winds))/2-3,"-- %s --",winds);
					wrefresh(statusw);
				}
				drawhelppage(actualhelppage);
				drawstatus();
				delwin(infow);
				touchwin(stdscr);
				touchwin(stationw);
				touchwin(freqw);
				touchwin(helpw);
				touchwin(statusw);
				wnoutrefresh(stdscr);
				wnoutrefresh(stationw);
				wnoutrefresh(statusw);
				wnoutrefresh(freqw);
				wnoutrefresh(helpw);
				doupdate();
				exitscroller = 0;
				pthread_create((pthread_t*) &scrollthread,NULL,
					(void*(*)(void*)) doscroll, 0);
				break;
			case 'f':
				key = 0;
				drawstatus();
				strcpy(dira,dir);
				strcat(dira,"*.raf");
				globreturn = glob(dira,0,NULL,&fonts);
				if(globreturn != 0)
				{
					mvwprintw(statusw,1,30,"|>!NO FONTS FOUND!<|");
					globfree(&fonts);
					while(key == 0) key = wgetch(statusw);
					drawstatus();
					break;
				}
				drawhelppage(42);
				wnoutrefresh(helpw);
				wnoutrefresh(statusw);
				doupdate();
				i = 0;
				for(j=0;j<fonts.gl_pathc;j++)
					if(!strcmp(fonts.gl_pathv[j],font)) i = j;
				while(key == 0)
				{
					strcpy(fntname,fonts.gl_pathv[i]);
					drawstatus();
					mvwprintw(statusw,1,2,"%3d: %s",i,basename(fntname));
					strcpy(font,fonts.gl_pathv[i]);
					readfont(font);
					drawfreq(ffreq);
					wrefresh(freqw);
					key = wgetch(statusw);
					if(key == KEY_UP)
					{
						i++;
						key = 0;
					}
					if(key == KEY_DOWN)
					{
						i--;
						key = 0;
					}
					if(i < 0) i = 0;
					if(i >= fonts.gl_pathc) i = fonts.gl_pathc - 1;
				}
				drawhelppage(actualhelppage);
				drawstatus();
				globfree(&fonts);
				break;
			case 'i':
				infow = newwin(10,38,7,21);
				init_pair(7,radiocolors.info_foreground,radiocolors.info_background);
				wbkgd(infow,(radiocolors.info_bold ? A_BOLD : 0) | COLOR_PAIR(7));
				wborder(infow,0,0,0,0,0,0,0,0);
				mvwprintw(infow,0,1," Informations about your radio-card ");
				mvwprintw(infow,2,2,"Range        : %.2f - %.2f",freqmin,freqmax);
				mvwprintw(infow,3,2,"MONO         : %d", radio_getmono());
				mvwprintw(infow,4,2,"STEREO       : %d", radio_getstereo());
				mvwprintw(infow,5,2,"CanSetVolume : %d", radio_cansetvolume());
				mvwprintw(infow,6,2,"CanSetBass   : %d", radio_cansetbass());
				mvwprintw(infow,7,2,"CanSetTreble : %d", radio_cansettreble());
				mvwprintw(infow,9,2,"(False = 0; True = 1)");
				wrefresh(infow);
				key = 0;
				while (key == 0) key = wgetch(infow);
				delwin(infow);
				touchwin(stdscr);
				touchwin(stationw);
				touchwin(freqw);
				touchwin(helpw);
				touchwin(statusw);
				wnoutrefresh(stdscr);
				wnoutrefresh(stationw);
				wnoutrefresh(statusw);
				wnoutrefresh(freqw);
				wnoutrefresh(helpw);
				doupdate();
				break;
			case '': case 'q': case 'e':
				radio_mute(TRUE);
				break;
		}
		drawfreq(ffreq);
		wnoutrefresh(freqw);
		wnoutrefresh(helpw);
		wnoutrefresh(statusw);
		wnoutrefresh(stationw);
		doupdate();
	}
	return 0;
}	

int deinit(void)
{
	radio_deinit();
	pthread_cancel((pthread_t) &scrollthread);
	close(fd);
	refresh();
	endwin();
	printf("[?25h");
	return 0;
}
