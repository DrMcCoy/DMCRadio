/* DMCRadio 1.1.2
 * Copyright (c) 2003-2004 Sven Hesse (DrMcCoy)
 *
 * This file is part of DMCRadio and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <linux/videodev.h>

#define FALSE 0
#define TRUE 1

static int radiodev;
extern int errno;
static struct video_tuner tuner;
static struct video_audio audio;

int LOW, AMONO, ASTEREO, SMUTE, SMUTEA, SVOLUME, SBASS, STREBLE;
double radiofreqmin, radiofreqmax;

int radio_getflags(void);

int radio_init(char *device)
{
	if((radiodev = open(device,O_RDONLY)) == -1) return errno;
	radio_getflags();
	return 0;
}

int radio_getflags(void)
{
	ioctl(radiodev, VIDIOCGTUNER, &tuner);
	ioctl(radiodev, VIDIOCGAUDIO, &audio);
	tuner.tuner = 0;
	LOW = (((tuner.flags & 8) == 8) ? TRUE : FALSE);
	AMONO = (((audio.mode & 1) == 1) ? TRUE : FALSE);
	ASTEREO = (((audio.mode & 2) == 2) ? TRUE : FALSE);
	SMUTE = (((audio.flags & 1) == 1) ? TRUE : FALSE);
	SMUTEA = (((audio.flags & 2) == 2) ? TRUE : FALSE);
	SVOLUME = (((audio.flags & 4) == 4) ? TRUE : FALSE);
	SBASS = (((audio.flags & 8) == 8) ? TRUE : FALSE);
	STREBLE = (((audio.flags & 16) == 16) ? TRUE : FALSE);
	radiofreqmin = tuner.rangelow / (LOW ? 16000 : 16);
	radiofreqmax = tuner.rangehigh / (LOW ? 16000 : 16);
	return 0;
}

int radio_getaudiomode(void)
{
	if(ASTEREO) return 2;
	else if(AMONO) return 1;
	else return 0;
}

int radio_getsignal(void)
{
	ioctl(radiodev,VIDIOCGTUNER,&tuner);
	return tuner.signal>>13;
}

int radio_tune(double frequency)
{
	unsigned long freq;

	freq = frequency * (LOW ? 16000 : 16);
	ioctl(radiodev,VIDIOCSFREQ,&freq);
	return 0;
}

int radio_mute(int mute)
{
	if(mute)
	{
		audio.flags = VIDEO_AUDIO_MUTE;
		audio.volume = 0;
		ioctl(radiodev,VIDIOCSAUDIO,&audio);
	}
	else
	{
		audio.flags = VIDEO_AUDIO_VOLUME;
		audio.volume = 65535;
		ioctl(radiodev, VIDIOCSAUDIO,&audio);
	}
	return 0;
}

int radio_deinit(void)
{
	close(radiodev);
	return 0;
}
