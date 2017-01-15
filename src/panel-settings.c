/*
 * This file is part of graphene-desktop, the desktop environment of VeltOS.
 * Copyright (C) 2016 Velt Technologies, Aidan Shafran <zelbrium@gmail.com>
 * Licensed under the Apache License 2 <www.apache.org/licenses/LICENSE-2.0>.
 */

#include "panel-internal.h"
#include "cmk/cmk-label.h"
#include "cmk/button.h"
#include "cmk/cmk-icon.h"
#include "cmk/shadow.h"

struct _GrapheneSettingsPopup
{
	CmkWidget parent;
	
	CmkShadowContainer *sdc;
	CmkWidget *window;
	ClutterScrollActor *scroll;
	
	gdouble scrollAmount;
};


G_DEFINE_TYPE(GrapheneSettingsPopup, graphene_settings_popup, CMK_TYPE_WIDGET)


static void graphene_settings_popup_dispose(GObject *self_);
static void graphene_settings_popup_allocate(ClutterActor *self_, const ClutterActorBox *box, ClutterAllocationFlags flags);
static gboolean on_scroll(ClutterScrollActor *scroll, ClutterScrollEvent *event, GrapheneSettingsPopup *self);
static void enum_settings_widgets(GrapheneSettingsPopup *self);

GrapheneSettingsPopup * graphene_settings_popup_new(void)
{
	return GRAPHENE_SETTINGS_POPUP(g_object_new(GRAPHENE_TYPE_SETTINGS_POPUP, NULL));
}

static void graphene_settings_popup_class_init(GrapheneSettingsPopupClass *class)
{
	G_OBJECT_CLASS(class)->dispose = graphene_settings_popup_dispose;
	CLUTTER_ACTOR_CLASS(class)->allocate = graphene_settings_popup_allocate;
}

static void graphene_settings_popup_init(GrapheneSettingsPopup *self)
{
	self->sdc = cmk_shadow_container_new();
	cmk_shadow_container_set_blur(self->sdc, 40);
	clutter_actor_add_child(CLUTTER_ACTOR(self), CLUTTER_ACTOR(self->sdc));

	self->window = cmk_widget_new();
	cmk_widget_set_draw_background_color(self->window, TRUE);
	cmk_widget_set_background_color_name(self->window, "background");
	clutter_actor_add_child(CLUTTER_ACTOR(self), CLUTTER_ACTOR(self->window));

	self->scroll = CLUTTER_SCROLL_ACTOR(clutter_scroll_actor_new());
	clutter_scroll_actor_set_scroll_mode(self->scroll, CLUTTER_SCROLL_VERTICALLY);
	ClutterBoxLayout *listLayout = CLUTTER_BOX_LAYOUT(clutter_box_layout_new());
	clutter_box_layout_set_orientation(listLayout, CLUTTER_ORIENTATION_VERTICAL); 
	clutter_actor_set_layout_manager(CLUTTER_ACTOR(self->scroll), CLUTTER_LAYOUT_MANAGER(listLayout));
	clutter_actor_set_reactive(CLUTTER_ACTOR(self->scroll), TRUE);
	g_signal_connect(self->scroll, "scroll-event", G_CALLBACK(on_scroll), self);
	clutter_actor_add_child(CLUTTER_ACTOR(self), CLUTTER_ACTOR(self->scroll));

	enum_settings_widgets(self);
}

static void graphene_settings_popup_dispose(GObject *self_)
{
	GrapheneSettingsPopup *self = GRAPHENE_SETTINGS_POPUP(self_);
	G_OBJECT_CLASS(graphene_settings_popup_parent_class)->dispose(self_);
}

static void graphene_settings_popup_allocate(ClutterActor *self_, const ClutterActorBox *box, ClutterAllocationFlags flags)
{
	GrapheneSettingsPopup *self = GRAPHENE_SETTINGS_POPUP(self_);
	
	ClutterActorBox windowBox = {MAX(box->x2-600, box->x1+(box->x2-box->x1)/2), box->y1, box->x2, box->y2};
	ClutterActorBox sdcBox = {windowBox.x1-40, windowBox.y1-40, windowBox.x2 + 40, box->y2 + 40};
	ClutterActorBox scrollBox = {windowBox.x1, windowBox.y1, windowBox.x2, windowBox.y2};

	clutter_actor_allocate(CLUTTER_ACTOR(self->window), &windowBox, flags);
	clutter_actor_allocate(CLUTTER_ACTOR(self->sdc), &sdcBox, flags);
	clutter_actor_allocate(CLUTTER_ACTOR(self->scroll), &scrollBox, flags);

	CLUTTER_ACTOR_CLASS(graphene_settings_popup_parent_class)->allocate(self_, box, flags);
}

static gboolean on_scroll(ClutterScrollActor *scroll, ClutterScrollEvent *event, GrapheneSettingsPopup *self)
{
	// TODO: Disable button highlight when scrolling, so it feels smoother
	if(event->direction == CLUTTER_SCROLL_SMOOTH)
	{
		gdouble dx, dy;
		clutter_event_get_scroll_delta((ClutterEvent *)event, &dx, &dy);
		self->scrollAmount += dy*50; // TODO: Not magic number for multiplier
		if(self->scrollAmount < 0)
			self->scrollAmount = 0;
	
		gfloat min, nat;
		clutter_layout_manager_get_preferred_height(clutter_actor_get_layout_manager(CLUTTER_ACTOR(scroll)), CLUTTER_CONTAINER(scroll), -1, &min, &nat);

		gfloat height = clutter_actor_get_height(CLUTTER_ACTOR(scroll));
		gfloat maxScroll = nat - height;

		if(self->scrollAmount > maxScroll)
			self->scrollAmount = maxScroll;

		ClutterPoint p = {0, self->scrollAmount};
		clutter_scroll_actor_scroll_to_point(scroll, &p);
	}
	return TRUE;
}

static ClutterActor * separator_new()
{
	ClutterActor *sep = clutter_actor_new();
	ClutterColor c = {0,0,0,25};
	clutter_actor_set_background_color(sep, &c);
	clutter_actor_set_x_expand(sep, TRUE);
	clutter_actor_set_height(sep, 2);
	return sep;
}

static void on_settings_widget_clicked(GrapheneSettingsPopup *self, CmkButton *button)
{
	const gchar *args = clutter_actor_get_name(CLUTTER_ACTOR(button));
	clutter_actor_destroy(CLUTTER_ACTOR(self));

	gchar **argsSplit = g_new0(gchar *, 3);
	argsSplit[0] = g_strdup("gnome-control-center");
	argsSplit[1] = g_strdup(clutter_actor_get_name(CLUTTER_ACTOR(button)));
	g_spawn_async(NULL, argsSplit, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
	g_strfreev(argsSplit);
}

static void add_setting_widget(GrapheneSettingsPopup *self, const gchar *title, const gchar *iconName, gboolean toggleable, const gchar *panel, gboolean bottomSeparator)
{	
	clutter_actor_add_child(CLUTTER_ACTOR(self->scroll), separator_new());

	CmkButton *button = cmk_button_new();
	CmkIcon *icon = cmk_icon_new_from_name(iconName);
	cmk_icon_set_size(icon, 48);
	cmk_button_set_content(button, CMK_WIDGET(icon));
	cmk_button_set_text(button, title);
	cmk_widget_set_style_parent(CMK_WIDGET(button), CMK_WIDGET(self));
	clutter_actor_set_x_expand(CLUTTER_ACTOR(button), TRUE);
	clutter_actor_add_child(CLUTTER_ACTOR(self->scroll), CLUTTER_ACTOR(button));
	
	clutter_actor_set_name(CLUTTER_ACTOR(button), panel);
	g_signal_connect_swapped(button, "activate", G_CALLBACK(on_settings_widget_clicked), self);
	
	if(bottomSeparator)
		clutter_actor_add_child(CLUTTER_ACTOR(self->scroll), separator_new());
}

static void add_settings_category_label(GrapheneSettingsPopup *self, const gchar *title)
{
	CmkLabel *label = cmk_label_new_with_text(title);
	cmk_widget_set_style_parent(CMK_WIDGET(label), CMK_WIDGET(self));
	clutter_actor_set_x_expand(CLUTTER_ACTOR(label), TRUE);
	clutter_actor_set_x_align(CLUTTER_ACTOR(label), CLUTTER_ACTOR_ALIGN_START);
	ClutterMargin margin = {50, 40, 20, 20};
	clutter_actor_set_margin(CLUTTER_ACTOR(label), &margin);
	clutter_actor_add_child(CLUTTER_ACTOR(self->scroll), CLUTTER_ACTOR(label));

	//ClutterActor *sep = separator_new();
	//clutter_actor_add_child(CLUTTER_ACTOR(self->scroll), sep);
}

static void enum_settings_widgets(GrapheneSettingsPopup *self)
{
	add_settings_category_label(self, "Personal");
	add_setting_widget(self, "Background",       "preferences-desktop-wallpaper",    TRUE,  "background",     FALSE);
	add_setting_widget(self, "Notifications",    "preferences-system-notifications", TRUE,  "notifications",  FALSE);
	add_setting_widget(self, "Privacy",          "preferences-system-privacy",       FALSE, "privacy",        FALSE);
	add_setting_widget(self, "Region & Language","preferences-desktop-locale",       FALSE, "region",         FALSE);
	add_setting_widget(self, "Search",           "preferences-system-search",        FALSE, "search",         TRUE);
	add_settings_category_label(self, "Hardware");
	add_setting_widget(self, "Bluetooth",        "bluetooth",                        TRUE,  "bluetooth",      FALSE);
	add_setting_widget(self, "Color",            "preferences-color",                FALSE, "color",          FALSE);
	add_setting_widget(self, "Displays",         "preferences-desktop-display",      FALSE, "display",        FALSE);
	add_setting_widget(self, "Keyboard",         "input-keyboard",                   FALSE, "keyboard",       FALSE);
	add_setting_widget(self, "Mouse & Touchpad", "input-mouse",                      FALSE, "mouse",          FALSE);
	add_setting_widget(self, "Network",          "network-workgroup",                TRUE,  "network",        FALSE);
	add_setting_widget(self, "Power",            "gnome-power-manager",              FALSE, "power",          FALSE);
	add_setting_widget(self, "Printers",         "printer",                          FALSE, "printers",       FALSE);
	add_setting_widget(self, "Sound",            "multimedia-volume-control",        TRUE,  "sound",          FALSE);
	add_setting_widget(self, "Wacom Tablet",     "input-tablet",                     FALSE, "wacom",          TRUE);
	add_settings_category_label(self, "System");
	add_setting_widget(self, "Date & Time",      "preferences-system-time",          FALSE, "datetime",       FALSE);
	add_setting_widget(self, "Details",          "applications-system",              FALSE, "info",           FALSE);
	add_setting_widget(self, "Sharing",          "preferences-system-sharing",       FALSE, "sharing",        FALSE);
	add_setting_widget(self, "Universal",        "preferences-desktop-accessibility",FALSE, "universal-access",FALSE);
	add_setting_widget(self, "Users",            "system-users",                     FALSE, "user-accounts",  TRUE);
}
