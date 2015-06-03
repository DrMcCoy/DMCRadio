/* DMCRadio 1.0.2
 * Copyright (c) 2003 Sven Hesse (DrMcCoy)
 *
 * This file is part of DMCRadio and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

int radio_init(char *device);
int radio_getaudiomode(void);
double radio_getlowestfrequency(void);
double radio_gethighestfrequency(void);
int radio_getsignal(void);
int radio_tune(double frequency);
int radio_mute(int mute);
int radio_deinit(void);
