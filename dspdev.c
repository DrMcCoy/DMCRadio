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

int dsp_blocksize, dsp_chn, dsp_rate;
static int dsp_fd;
static int16_t *buffer=NULL;
extern int errno;

int dsp_init(char *device)
{
	if((dsp_fd = open(device, O_RDWR)) == -1) return errno;
	return 0;
}

int dsp_setrec(int chn, int rate)
{
	int i;

	i = 0x7fff000d;
	ioctl(dsp_fd, SNDCTL_DSP_SETFRAGMENT, &i);
	i = AFMT_S16_LE;
	if((ioctl(dsp_fd, SNDCTL_DSP_SETFMT, &i)) == -1) return -1;
	dsp_chn = chn;
	ioctl(dsp_fd, SNDCTL_DSP_CHANNELS, &dsp_chn);
	dsp_rate = rate;
	ioctl(dsp_fd, SNDCTL_DSP_SPEED, &dsp_rate);
	ioctl(dsp_fd, SNDCTL_DSP_GETBLKSIZE,  &dsp_blocksize);
	buffer = malloc(dsp_blocksize);
	return 0;
}

int16_t *dsp_read(void)
{
	read(dsp_fd, buffer, dsp_blocksize);
	return buffer;
}

int dsp_deinit(void)
{
	free(buffer);
	return close(dsp_fd);
}
