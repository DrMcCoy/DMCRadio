/* DMCRadio 1.0.0
 * Copyright (c) 2002 Sven Hesse (DrMcCoy)
 *
 * This file is part of DMCRadio and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

int radio_init(char *device);
int radio_getflags(void);
int radio_getaudiomode(void);
int radio_getmono(void);
int radio_getstereo(void);
int radio_cansetvolume(void);
int radio_cansetbass(void);
int radio_cansettreble(void);
double radio_getlowestfrequency(void);
double radio_gethighestfrequency(void);
int radio_getsignal(void);
int radio_tune(double frequency);
int radio_mute(int mute);
int radio_deinit(void);
