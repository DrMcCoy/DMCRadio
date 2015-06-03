/* DMCRadio 1.0.2
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

#define FALSE 0
#define TRUE 1

int mixer_fd;
extern int errno;

int mixer_getnrdevices(void)
{
	return SOUND_MIXER_NRDEVICES;
}

int mixer_init(char *device)
{
	mixer_fd = open(device,O_RDWR);
	if(mixer_fd == -1)
		return -1;
	return 0;
}

int mixer_hasdev(char *device)
{
	int mask, i;
	char *l_label[] = SOUND_DEVICE_NAMES;

	if(ioctl(mixer_fd,SOUND_MIXER_READ_DEVMASK,&mask) < 0) return -1;
	for(i=0;i<SOUND_MIXER_NRDEVICES;i++)
		if(!strcmp(l_label[i],device))
		{
			if(mask&(int)pow(2,i)) return i+1;
			break;
	}
	return 0;
}

char *mixer_getdevname(int device)
{
	char *l_label[] = SOUND_DEVICE_NAMES;

	return l_label[device];
}

int mixer_getvol(char *device)
{
	int devnr, mask;
		
	devnr = mixer_hasdev(device);
	if(!devnr) return -1;
	if(ioctl(mixer_fd,MIXER_READ(devnr-1),&mask) < 0) return -1;
	mask >>= 7;
	return mask << 7;
	return mask;
}

int mixer_setvol(char *device, int volume)
{
	int devnr;
		
	devnr = mixer_hasdev(device);
	if(!devnr) return -1;
	volume = volume + (volume >> 8);
	if(volume > (25600 + (25600 >> 8))) volume = 25600 + (25600 >> 8);
	if(volume < 0) volume = 0;
	if(ioctl(mixer_fd,MIXER_WRITE(devnr-1),&volume) < 0) return -1;
	return mixer_getvol(device);
}

int mixer_incvol(char *device, int value)
{
	int devnr, volume;

	devnr = mixer_hasdev(device);
	if(!devnr) return -1;
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

	devnr = mixer_hasdev(device);
	if(!devnr) return -1;
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
