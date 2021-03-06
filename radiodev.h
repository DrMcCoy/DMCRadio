/* DMCRadio 1.1.4
 * Copyright (c) 2003-2004 Sven Hesse (DrMcCoy)
 *
 * This file is part of DMCRadio and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

#ifndef RADIODEV_H
#define RADIODEV_H

int radio_init(char *device);
int radio_getaudiomode(void);
int radio_getflags(void);
double radio_getlowestfrequency(void);
double radio_gethighestfrequency(void);
int radio_getsignal(void);
int radio_tune(double frequency);
int radio_mute(int mute);
int radio_deinit(void);

#endif // RADIODEV_H
