/*
 * Geeqie
 * Copyright (C) 2008 The Geeqie Team
 *
 * Authors: Vladimir Nadvornik / Laurent Monin
 * 
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#include "main.h"
#include "compat.h"

/* gtk_radio_action_set_current_value() replacement for GTK+ < 2.10 */
void radio_action_set_current_value(GtkRadioAction *action, gint current_value)
{
#if GTK_CHECK_VERSION(2, 10, 0)
	gtk_radio_action_set_current_value(action, current_value);
#else
	GSList *group;
	gint value;

	group = gtk_radio_action_get_group(action);
	while (group)
		{
		action = GTK_RADIO_ACTION(group->data);
		g_object_get(G_OBJECT(action), "value", &value, NULL);
		if (value == current_value)
			{
			gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), TRUE);
			return;
			}
		group = g_slist_next(group);
		}
#endif
}
