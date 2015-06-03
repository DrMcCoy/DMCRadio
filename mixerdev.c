/* DMCRadio 1.1.0
 * Copyright (c) 2003 Sven Hesse (DrMcCoy)
 *
 * This file is part of DMCRadio and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define FALSE 0
#define TRUE 1

static int mixer_fd;
extern int errno;

int mixer_getnrdevices(void)
{
	return SOUND_MIXER_NRDEVICES;
}

int mixer_init(char *device)
{
	if((mixer_fd = open(device,O_RDWR)) == -1) return errno;
	return 0;
}

int mixer_getdevnr(char *device)
{
	char *l_names[] = SOUND_DEVICE_NAMES;
	int i;

	for(i=0;i<SOUND_MIXER_NRDEVICES;i++)
		if(!strcmp(l_names[i],device)) return i;
	return -1;
}

int mixer_hasdevnr(int device)
{
	int mask;

	if(ioctl(mixer_fd,SOUND_MIXER_READ_DEVMASK,&mask) < 0) return -1;
	return (mask & (int)pow(2, device));
}

int mixer_getfirstsupdevnr(void)
{
	int mask, i;

	if(ioctl(mixer_fd,SOUND_MIXER_READ_DEVMASK,&mask) < 0) return -1;
	for(i=0;i<SOUND_MIXER_NRDEVICES;i++)
		if(mask & (int)pow(2, i)) return i;
	return -1;
}

int mixer_hasdev(char *device)
{
	int mask, i;
	char *l_names[] = SOUND_DEVICE_NAMES;

	if(ioctl(mixer_fd,SOUND_MIXER_READ_DEVMASK,&mask) < 0) return -1;
	for(i=0;i<SOUND_MIXER_NRDEVICES;i++)
		if((!strcmp(l_names[i],device)) && (mask&(int)pow(2,i)))
			return i+1;
	return 0;
}

char **mixer_getdevnames(void)
{
	char *l_names[] = SOUND_DEVICE_NAMES, **devices;
	int i;

	devices = malloc(SOUND_MIXER_NRDEVICES * sizeof(char *));
	for(i=0;i<SOUND_MIXER_NRDEVICES;i++)
		devices[i] = strdup(l_names[i]);
	return devices;
}

char *mixer_getdevname(int device)
{
	char *l_names[] = SOUND_DEVICE_NAMES;

	return l_names[device];
}

char **mixer_getdevlabels(void)
{
	char *l_labels[] = SOUND_DEVICE_LABELS, **devices;
	int i;

	devices = malloc(SOUND_MIXER_NRDEVICES * sizeof(char *));
	for(i=0;i<SOUND_MIXER_NRDEVICES;i++)
		devices[i] = strdup(l_labels[i]);
	return devices;
}

char *mixer_getdevlabel(int device)
{
	char *l_labels[] = SOUND_DEVICE_LABELS;

	return l_labels[device];
}

int mixer_getvol(char *device)
{
	int devnr, mask;
		
	if(!(devnr = mixer_hasdev(device))) return -1;
	if(ioctl(mixer_fd,MIXER_READ(devnr-1),&mask) < 0) return -1;
	mask >>= 7;
	return mask << 7;
}

int mixer_setvol(char *device, int volume)
{
	int devnr;
		
	if(!(devnr = mixer_hasdev(device))) return -1;
	volume += (volume >> 8);
	if(volume > (25600 + (25600 >> 8))) volume = 25600 + (25600 >> 8);
	if(volume < 0) volume = 0;
	if(ioctl(mixer_fd,MIXER_WRITE(devnr-1),&volume) < 0) return -1;
	return mixer_getvol(device);
}

int mixer_incvol(char *device, int value)
{
	int devnr, volume;

	if(!(devnr = mixer_hasdev(device))) return -1;
	volume = mixer_getvol(device);
	volume = (volume + (volume >> 8)) + (value + (value >> 8));
	if(volume > (25600 + (25600 >> 8))) volume = 25600 + (25600 >> 8);
	if(volume < 0) volume = 0;
	if(ioctl(mixer_fd,MIXER_WRITE(devnr-1),&volume) < 0) return -1;
	return mixer_getvol(device);
}

int mixer_decvol(char *device, int value)
{
	int devnr, volume;

	if(!(devnr = mixer_hasdev(device))) return -1;
	volume = mixer_getvol(device);
	volume = (volume + (volume >> 8)) - (value + (value >> 8));
	if(volume > (25600 + (25600 >> 8))) volume = 25600 + (25600 >> 8);
	if(volume < 0) volume = 0;
	if(ioctl(mixer_fd,MIXER_WRITE(devnr-1),&volume) < 0) return -1;
	return mixer_getvol(device);
}

int mixer_deinit(void)
{
	close(mixer_fd);
	return 0;
}
