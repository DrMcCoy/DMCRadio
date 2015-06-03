/* DMCRadio 1.1.4
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
#ifdef __FreeBSD__
#include <sys/param.h>
#if __FreeBSD_version >= 502100
#include <dev/bktr/ioctl_bt848.h>
#else
#include <machine/ioctl_bt848.h>
#endif
#else
#include <linux/videodev.h>
#endif

#define FALSE 0
#define TRUE 1

static int radiodev;
extern int errno;

#ifdef __FreeBSD__
static int tuner, audio;
#else
static struct video_tuner tuner;
static struct video_audio audio;
#endif

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
#ifdef __FreeBSD__
	ioctl(radiodev, TVTUNER_GETSTATUS, &tuner);
	ioctl(radiodev, RADIO_GETMODE, &audio);
	AMONO = ((audio & RADIO_MONO) ? TRUE : FALSE);
	ASTEREO = (! AMONO);
	ioctl(radiodev, BT848_GAUDIO, &audio);
	SMUTE = ((audio & AUDIO_MUTE) ? TRUE : FALSE);
	SMUTEA = TRUE;
	SVOLUME = FALSE;
	radiofreqmin = 87.50;
	radiofreqmax = 108.00;
	return 0;
#else
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
#endif
}

int radio_getaudiomode(void)
{
	if(ASTEREO) return 2;
	else if(AMONO) return 1;
	else return 0;
}

int radio_getsignal(void)
{
#ifdef __FreeBSD__
	ioctl(radiodev, TVTUNER_GETSTATUS, &tuner);
	return tuner & 0x07;
#else
	ioctl(radiodev,VIDIOCGTUNER,&tuner);
	return tuner.signal>>13;
#endif
}

int radio_tune(double frequency)
{
	unsigned long freq;

#ifdef __FreeBSD__
	freq = frequency * 100;
	ioctl(radiodev,RADIO_SETFREQ,&freq);
#else
	freq = frequency * (LOW ? 16000 : 16);
	ioctl(radiodev,VIDIOCSFREQ,&freq);
#endif
	return 0;
}

int radio_mute(int mute)
{
#ifdef __FreeBSD__
	if(mute)
	{
		audio = AUDIO_MUTE;
		ioctl(radiodev, BT848_SAUDIO, &audio);
	}
	else
	{
		audio = AUDIO_UNMUTE;
		ioctl(radiodev, BT848_SAUDIO, &audio);
	}
#else
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
#endif
	return 0;
}

int radio_deinit(void)
{
	close(radiodev);
	return 0;
}
