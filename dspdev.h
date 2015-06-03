/* DMCRadio 1.1.4
 * Copyright (c) 2003-2004 Sven Hesse (DrMcCoy)
 *
 * This file is part of DMCRadio and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

#ifndef DSPDEV_H
#define DSPDEV_H

int dsp_init(char *device);
int dsp_setrec(int chn, int rate);
int16_t *dsp_read(void);
int dsp_deinit(void);

#endif // DSPDEV_H
