/* DMCRadio 1.1.3
 * Copyright (c) 2003-2004 Sven Hesse (DrMcCoy)
 *
 * This file is part of DMCRadio and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

int mixer_getnrdevices(void);
int mixer_init(char *device);
int mixer_hasdev(char *device);
int mixer_getdevnr(char *device);
int mixer_getfirstsupdevnr(void);
int mixer_hasdevnr(int device);
char **mixer_getdevnames(void);
char *mixer_getdevname(int device);
char **mixer_getdevlabels(void);
char *mixer_getdevlabel(int device);
int mixer_getvol(char *device);
int mixer_setvol(char *device, int volume);
int mixer_incvol(char *device, int value);
int mixer_decvol(char *device, int value);
int mixer_deinit(void);
