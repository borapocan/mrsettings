/*
 * mrsettings.c — MrRobotOS System Settings Application
 * Copyright (C) 2026 Merih Bora Poçan (MrRobotOS)
 * https://github.com/borapocan/mrsettings
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/x11/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>


#define AVATAR_SIDEBAR_SIZE 112
#define AVATAR_DRAW_AREA_SIZE 280

/* ================================================================== */
/* Forward declarations                                                 */
/* ================================================================== */
static void       activate(GtkApplication *app, gpointer user_data);
static GtkWidget *wifi_settings(void);
static GtkWidget *bluetooth_settings(void);
static GtkWidget *battery_settings(void);
static GtkWidget *displays_settings(void);
static GtkWidget *sound_settings(void);
static GtkWidget *keyboard_settings(void);
static GtkWidget *keybindings_settings(void);
static GtkWidget *clicks_settings(void);
static GtkWidget *terminal_settings(void);
static GtkWidget *vpn_settings(void);
static GtkWidget *appearance_settings (void);
static GtkWidget *wallpaper_settings(void);
static GtkWidget *brightness_settings(void);
static GtkWidget *users_settings(void);
static GtkWidget *datetime_settings(void);
static GtkWidget *region_settings(void);
static GtkWidget *applications_settings(void);
static GtkWidget *about_settings(void);
static GtkWidget *notifications_settings(void);
static GtkWidget *updates_settings(void);
static GtkWidget *sharing_settings(void);
static char      *get_home_env(void);
static char      *get_current_wallpaper(void);
static gboolean   sidebar_filter_func(GtkListBoxRow *row, gpointer user_data);
static void       sidebar_search_changed(GtkSearchEntry *entry, gpointer user_data);
static gboolean   sidebar_keynav(GtkWidget *widget, guint keyval, guint keycode,
				 GdkModifierType state, gpointer user_data);
static void       append_page_row(GtkListBox *lb, const char *title,
				  const char *icon_name, int icon_size, int pad_v);
static void       sidebar_row_selected(GtkListBox *box, GtkListBoxRow *row,
				       gpointer user_data);
static void       append_separator(GtkListBox *lb);
static void       append_group_label(GtkListBox *lb, const char *text);
static void open_picker(GtkWidget *btn, gpointer ud);
static void av_circle_set_path (GtkWidget *da, const char *path, int size);

static GtkWidget *stack_global = NULL;

typedef struct {
	const char *name;
	GtkWidget *(*builder)(void);
	gboolean   built;
} LazyPage;

static GtkWidget *window          = NULL;
static GtkWidget *sidebar_listbox = NULL;

static GtkWidget *sidebar_av_global  = NULL;  /* sidebar avatar picture/image */
static GtkWidget *account_av_global  = NULL;  /* account page avatar picture/image */

static LazyPage lazy_pages[] = {
	{ "Wi-Fi",              wifi_settings,          FALSE },
	{ "Bluetooth",          bluetooth_settings,     FALSE },
	{ "VPN",                vpn_settings,           FALSE },
	{ "Displays",           displays_settings,      FALSE },
	{ "Sound",              sound_settings,         FALSE },
	{ "Keyboard",           keyboard_settings,      FALSE },
	{ "Battery",            battery_settings,       FALSE },
	{ "Keybindings",	    keybindings_settings,   FALSE },
	{ "Clicks & Buttons",   clicks_settings,        FALSE },
	{ "Shortcuts",          terminal_settings,      FALSE },
	{ "Appearance",         appearance_settings,    FALSE },
	{ "Wallpaper",          wallpaper_settings,     FALSE },
	{ "Brightness",         brightness_settings,    FALSE },
	{ "Notifications",      notifications_settings, FALSE },
	{ "Users & Groups",     users_settings,         FALSE },
	{ "Date & Time",        datetime_settings,      FALSE },
	{ "Region & Language",  region_settings,        FALSE },
	{ "Software & Updates", updates_settings,       FALSE },
	{ "Sharing",            sharing_settings,       FALSE },
	{ "Applications",       applications_settings,  FALSE },
	{ "About",              about_settings,         FALSE },
	{ NULL, NULL, FALSE }
};

static GtkWidget *make_placeholder_page(const char *name) {
	GtkWidget *b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
	gtk_widget_set_halign(b, GTK_ALIGN_CENTER);
	GtkWidget *spinner = gtk_spinner_new();
	gtk_spinner_start(GTK_SPINNER(spinner));
	gtk_widget_set_size_request(spinner, 32, 32);
	gtk_box_append(GTK_BOX(b), spinner);
	GtkWidget *lbl = gtk_label_new(name);
	gtk_widget_add_css_class(lbl, "dim-label");
	gtk_box_append(GTK_BOX(b), lbl);
	return b;
}

/* ================================================================== */
/* Shared utility                                                       */
/* ================================================================== */
static char *run_cmd_str(const char *cmd) {
	FILE *f = popen(cmd, "r");
	if (!f) return g_strdup("");
	GString *out = g_string_new(NULL);
	char buf[256];
	while (fgets(buf, sizeof(buf), f))
		g_string_append(out, buf);
	pclose(f);
	return g_string_free(out, FALSE);
}

static char *get_home_env(void) {
	char *h = getenv("HOME");
	return h ? strdup(h) : NULL;
}

static char *get_current_wallpaper(void) {
	char *home = get_home_env();
	char path[512];
	snprintf(path, sizeof(path), "%s/.fehbg", home);
	free(home);
	FILE *f = fopen(path, "r");
	if (!f) return NULL;
	char line[1024], *result = NULL;
	while (fgets(line, sizeof(line), f)) {
		char *o = strchr(line, '\'');
		if (!o) o = strchr(line, '"');
		if (!o) continue;
		char q = *o;
		char *c = strrchr(line, q);
		if (c && c > o) { *c = '\0'; result = strdup(o + 1); break; }
	}
	fclose(f);
	return result;
}

static GtkWidget *make_section_box(const char *title) {
	GtkWidget *frame = gtk_frame_new(NULL);
	gtk_widget_add_css_class(frame, "bat-box");
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_frame_set_child(GTK_FRAME(frame), box);
	if (title) {
		GtkWidget *lbl = gtk_label_new(title);
		gtk_widget_add_css_class(lbl, "title-4");
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_widget_set_margin_start(lbl, 16);
		gtk_widget_set_margin_end(lbl, 16);
		gtk_widget_set_margin_top(lbl, 14);
		gtk_widget_set_margin_bottom(lbl, 10);
		gtk_box_append(GTK_BOX(box), lbl);
		gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	}
	g_object_set_data(G_OBJECT(frame), "inner-box", box);
	return frame;
}
/* make_bat_box is an alias for make_section_box used throughout */
#define make_bat_box make_section_box

static GtkWidget *make_page_header(const char *icon_name, const char *title) {
	GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_start(hdr, 20); gtk_widget_set_margin_end(hdr, 20);
	gtk_widget_set_margin_top(hdr, 16);   gtk_widget_set_margin_bottom(hdr, 12);
	GtkWidget *ic = gtk_image_new_from_icon_name(icon_name);
	gtk_image_set_pixel_size(GTK_IMAGE(ic), 24);
	gtk_box_append(GTK_BOX(hdr), ic);
	GtkWidget *tl = gtk_label_new(title);
	gtk_widget_add_css_class(tl, "title-2");
	gtk_widget_set_hexpand(tl, TRUE);
	gtk_widget_set_halign(tl, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(hdr), tl);
	return hdr;
}

typedef struct {
	GtkWidget *fullname, *email, *phone, *org,
		  *addr,     *city,  *country, *website;
	char       dir[512];
	char       staged_avatar_src[600];
} PiData;

typedef struct {
	GtkWidget *av;
	char       apath[600];
} AvData;

typedef struct {
	GtkWidget *dest_av;          /* widget to update after selection */
	char       dest_path[600];   /* where to copy the chosen file    */
	char       staged_src[600];
	GtkWidget *picker_window;
	GtkWidget *picker_preview;
	GtkWidget *path_bar;
	GtkWidget *file_flow;
	char       current_dir[4096];
	char       selected_path[4096];
} AvPickerData;

/* ================================================================== */
/* Wallpaper picker                                                     */
/* ================================================================== */
typedef struct {
	GtkWidget *preview_picture;
	GtkWidget *picker_preview;
	GtkWidget *picker_window;
	GtkWidget *path_bar;
	GtkWidget *file_flow;
	char       current_dir[4096];
	char       selected_path[4096];
	gboolean   start_in_home;
} WPData;

static void load_dir(WPData *wd, const char *path);

static int cmp_strp(const void *a, const void *b) {
	return strcasecmp(*(const char **)a, *(const char **)b);
}
static int is_img(const char *n) {
	const char *e = strrchr(n, '.'); if (!e) return 0; e++;
	return !strcasecmp(e,"png")||!strcasecmp(e,"jpg")||!strcasecmp(e,"jpeg")||
		!strcasecmp(e,"webp")||!strcasecmp(e,"bmp")||!strcasecmp(e,"gif");
}
static void mkpath(char *out, size_t sz, const char *dir, const char *name) {
	if (!strcmp(dir,"/")) snprintf(out,sz,"/%s",name);
	else                  snprintf(out,sz,"%s/%s",dir,name);
}
static void set_wallpaper(const char *p) {
	char cmd[4096];
	snprintf(cmd, sizeof(cmd), "feh --bg-scale '%s'", p);
	system(cmd);
}

static void flowbox_clear(GtkFlowBox *fb) {
	GSList *to_remove = NULL;
	GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(fb));
	while (child) {
		to_remove = g_slist_prepend(to_remove, child);
		child = gtk_widget_get_next_sibling(child);
	}
	for (GSList *l = to_remove; l; l = l->next)
		gtk_flow_box_remove(fb, GTK_WIDGET(l->data));
	g_slist_free(to_remove);
}


static void path_btn_cb(GtkWidget *btn, gpointer unused) {
	WPData     *wd = g_object_get_data(G_OBJECT(btn), "wd");
	const char *p  = g_object_get_data(G_OBJECT(btn), "path");
	load_dir(wd, p);
}

static void build_path_bar(WPData *wd) {
	GtkWidget *c;
	while ((c = gtk_widget_get_first_child(wd->path_bar)))
		gtk_widget_unparent(c);
	char buf[4096];
	strncpy(buf, wd->current_dir, sizeof(buf)-1);
	buf[sizeof(buf)-1] = '\0';
	char *names[64], *paths[64];
	int   n = 0;
	names[n] = strdup("/"); paths[n] = strdup("/"); n++;
	char accum[4096] = "";
	char *p = buf;
	while (*p == '/') p++;
	while (*p && n < 63) {
		char *sl = strchr(p, '/');
		size_t len = sl ? (size_t)(sl - p) : strlen(p);
		if (len == 0) { p++; continue; }
		char seg[256] = "";
		if (len >= sizeof(seg)) len = sizeof(seg)-1;
		memcpy(seg, p, len); seg[len] = '\0';
		size_t al = strlen(accum);
		snprintf(accum+al, sizeof(accum)-al, "/%s", seg);
		names[n] = strdup(seg); paths[n] = strdup(accum); n++;
		p += len; if (*p == '/') p++;
	}
	for (int i = 0; i < n; i++) {
		if (i > 0) gtk_box_append(GTK_BOX(wd->path_bar), gtk_label_new(" › "));
		GtkWidget *btn = gtk_button_new_with_label(names[i]);
		gtk_widget_add_css_class(btn, "flat");
		gtk_widget_add_css_class(btn, "path-btn");
		g_object_set_data_full(G_OBJECT(btn), "path", g_strdup(paths[i]), g_free);
		g_object_set_data(G_OBJECT(btn), "wd", wd);
		g_signal_connect(btn, "clicked", G_CALLBACK(path_btn_cb), NULL);
		gtk_box_append(GTK_BOX(wd->path_bar), btn);
		free(names[i]); free(paths[i]);
	}
}

static void folder_cb(GtkWidget *btn, gpointer unused) {
	WPData     *wd   = g_object_get_data(G_OBJECT(btn), "wd");
	const char *path = g_object_get_data(G_OBJECT(btn), "path");
	load_dir(wd, path);
}

static void image_cb(GtkWidget *btn, gpointer unused) {
	WPData     *wd   = g_object_get_data(G_OBJECT(btn), "wd");
	const char *path = g_object_get_data(G_OBJECT(btn), "path");
	strncpy(wd->selected_path, path, sizeof(wd->selected_path)-1);
	GFile *gf = g_file_new_for_path(path);
	gtk_picture_set_file(GTK_PICTURE(wd->picker_preview), gf);
	g_object_unref(gf);
}

static void load_dir(WPData *wd, const char *path) {
	char real[4096];
	if (!realpath(path, real)) strncpy(real, path, sizeof(real)-1);
	strncpy(wd->current_dir, real, sizeof(wd->current_dir)-1);
	build_path_bar(wd);
	flowbox_clear(GTK_FLOW_BOX(wd->file_flow));
	DIR *d = opendir(real);
	if (!d) {
		gtk_flow_box_insert(GTK_FLOW_BOX(wd->file_flow),
				    gtk_label_new("Cannot open directory"), -1);
		return;
	}
	char **dirs = NULL, **fils = NULL;
	int    nd=0, dc=0, nf=0, fc=0;
	struct dirent *de;
	while ((de = readdir(d))) {
		if (!strcmp(de->d_name,".") || !strcmp(de->d_name,"..")) continue;
		char fp[4096]; mkpath(fp,sizeof(fp),real,de->d_name);
		struct stat st; if (lstat(fp,&st)) continue;
		if (S_ISDIR(st.st_mode)) {
			if (nd>=dc){dc=dc?dc*2:64;dirs=realloc(dirs,dc*sizeof*dirs);}
			dirs[nd++]=strdup(de->d_name);
		} else {
			if (nf>=fc){fc=fc?fc*2:64;fils=realloc(fils,fc*sizeof*fils);}
			fils[nf++]=strdup(de->d_name);
		}
	}
	closedir(d);
	if (nd) qsort(dirs,nd,sizeof*dirs,cmp_strp);
	if (nf) qsort(fils,nf,sizeof*fils,cmp_strp);

	for (int i = 0; i < nd; i++) {
		char fp[4096]; mkpath(fp,sizeof(fp),real,dirs[i]);
		GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
		gtk_widget_set_size_request(vb, 110, 95);
		GtkWidget *ico = gtk_image_new_from_icon_name("folder");
		gtk_image_set_pixel_size(GTK_IMAGE(ico), 52);
		gtk_widget_set_halign(ico, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(vb), ico);
		GtkWidget *lbl = gtk_label_new(dirs[i]);
		gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_MIDDLE);
		gtk_label_set_max_width_chars(GTK_LABEL(lbl), 12);
		gtk_widget_set_halign(lbl, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(vb), lbl);
		GtkWidget *btn = gtk_button_new();
		gtk_widget_add_css_class(btn, "flat");
		gtk_widget_add_css_class(btn, "picker-item");
		gtk_button_set_child(GTK_BUTTON(btn), vb);
		g_object_set_data_full(G_OBJECT(btn), "path", g_strdup(fp), g_free);
		g_object_set_data(G_OBJECT(btn), "wd", wd);
		g_signal_connect(btn, "clicked", G_CALLBACK(folder_cb), NULL);
		gtk_flow_box_insert(GTK_FLOW_BOX(wd->file_flow), btn, -1);
	}

	for (int i = 0; i < nf; i++) {
		char fp[4096]; mkpath(fp,sizeof(fp),real,fils[i]);
		GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
		gtk_widget_set_size_request(vb, 130, 115);
		GtkWidget *ico;
		if (is_img(fils[i])) {
			GFile *_gf = g_file_new_for_path(fp);
			ico = gtk_picture_new_for_file(_gf);
			g_object_unref(_gf);
			gtk_picture_set_content_fit(GTK_PICTURE(ico), GTK_CONTENT_FIT_COVER);
			gtk_widget_set_size_request(ico, 110, 72);
			gtk_widget_set_overflow(ico, GTK_OVERFLOW_HIDDEN);
		} else {
			ico = gtk_image_new_from_icon_name("text-x-generic");
			gtk_image_set_pixel_size(GTK_IMAGE(ico),48);
		}
		gtk_widget_set_halign(ico, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(vb), ico);
		GtkWidget *lbl = gtk_label_new(fils[i]);
		gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_MIDDLE);
		gtk_label_set_max_width_chars(GTK_LABEL(lbl), 13);
		gtk_widget_set_halign(lbl, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(vb), lbl);
		GtkWidget *btn = gtk_button_new();
		gtk_widget_add_css_class(btn, "flat");
		gtk_widget_add_css_class(btn, "picker-item");
		gtk_button_set_child(GTK_BUTTON(btn), vb);
		g_object_set_data_full(G_OBJECT(btn), "path", g_strdup(fp), g_free);
		g_object_set_data(G_OBJECT(btn), "wd", wd);
		if (is_img(fils[i]))
			g_signal_connect(btn, "clicked", G_CALLBACK(image_cb), NULL);
		gtk_flow_box_insert(GTK_FLOW_BOX(wd->file_flow), btn, -1);
	}

	for(int i=0;i<nd;i++) free(dirs[i]); free(dirs);
	for(int i=0;i<nf;i++) free(fils[i]); free(fils);
}

// Callback — called asynchronously, no blocking:
	static void
picker_confirm_cb (GObject *src, GAsyncResult *res, gpointer ud)
{
	WPData *wd  = ud;
	int     btn = gtk_alert_dialog_choose_finish (GTK_ALERT_DIALOG (src), res, NULL);
	if (btn != 1) return;   /* 0 = Cancel */

	set_wallpaper (wd->selected_path);
	GFile *gf = g_file_new_for_path (wd->selected_path);
	gtk_picture_set_file (GTK_PICTURE (wd->preview_picture), gf);
	g_object_unref (gf);
	if (wd->picker_window) {
		gtk_window_destroy (GTK_WINDOW (wd->picker_window));
		wd->picker_window = NULL;
	}
}

	static void
picker_apply (GtkWidget *btn, gpointer ud)
{
	WPData *wd = ud;
	if (!wd->selected_path[0]) return;

	char msg[4096 + 32];
	snprintf (msg, sizeof (msg), "Change wallpaper to:\n%s", wd->selected_path);

	GtkAlertDialog *dlg = gtk_alert_dialog_new ("%s", msg);
	gtk_alert_dialog_set_buttons (dlg,
				      (const char *[]){"Cancel", "Set Wallpaper", NULL});
	gtk_alert_dialog_set_default_button (dlg, 1);
	gtk_alert_dialog_set_cancel_button  (dlg, 0);
	gtk_alert_dialog_choose (dlg,
				 wd->picker_window ? GTK_WINDOW (wd->picker_window) : GTK_WINDOW (window),
				 NULL, picker_confirm_cb, wd);
	g_object_unref (dlg);
}

static void picker_cancel(GtkWidget *btn, gpointer ud) {
	WPData *wd = ud;
	gtk_window_destroy(GTK_WINDOW(wd->picker_window));
	wd->picker_window = NULL;
}

static void open_picker(GtkWidget *btn, gpointer ud) {
	WPData *wd = ud;
	if (wd->picker_window) { gtk_window_present(GTK_WINDOW(wd->picker_window)); return; }
	wd->picker_window = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(wd->picker_window),"Choose Wallpaper");
	gtk_window_set_transient_for(GTK_WINDOW(wd->picker_window),GTK_WINDOW(window));
	gtk_window_set_modal(GTK_WINDOW(wd->picker_window),TRUE);
	gtk_window_set_default_size(GTK_WINDOW(wd->picker_window),980,660);
	g_signal_connect_swapped(wd->picker_window,"destroy",
				 G_CALLBACK(g_nullify_pointer),&wd->picker_window);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	GtkWidget *bscr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bscr),GTK_POLICY_AUTOMATIC,GTK_POLICY_NEVER);
	gtk_widget_set_margin_start(bscr,8); gtk_widget_set_margin_end(bscr,8);
	gtk_widget_set_margin_top(bscr,8);   gtk_widget_set_margin_bottom(bscr,4);
	wd->path_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(bscr),wd->path_bar);
	gtk_box_append(GTK_BOX(root),bscr);
	gtk_box_append(GTK_BOX(root),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_widget_set_vexpand(row,TRUE);
	GtkWidget *fscr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(fscr),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
	gtk_widget_set_hexpand(fscr,TRUE); gtk_widget_set_vexpand(fscr,TRUE);
	wd->file_flow = gtk_flow_box_new();
	gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(wd->file_flow),GTK_SELECTION_NONE);
	gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(wd->file_flow),20);
	gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(wd->file_flow),2);
	gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(wd->file_flow),6);
	gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(wd->file_flow),6);
	gtk_widget_set_margin_start(wd->file_flow,10); gtk_widget_set_margin_end(wd->file_flow,10);
	gtk_widget_set_margin_top(wd->file_flow,10);   gtk_widget_set_margin_bottom(wd->file_flow,10);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(fscr),wd->file_flow);
	gtk_box_append(GTK_BOX(row),fscr);
	gtk_box_append(GTK_BOX(row),gtk_separator_new(GTK_ORIENTATION_VERTICAL));
	GtkWidget *pane = gtk_box_new(GTK_ORIENTATION_VERTICAL,12);
	gtk_widget_set_size_request(pane,340,-1);
	gtk_widget_set_margin_start(pane,14); gtk_widget_set_margin_end(pane,14);
	gtk_widget_set_margin_top(pane,14);   gtk_widget_set_margin_bottom(pane,14);
	GtkWidget *pt = gtk_label_new("Preview");
	gtk_widget_add_css_class(pt,"title-4");
	gtk_widget_set_halign(pt,GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(pane),pt);
	wd->picker_preview = gtk_picture_new();
	gtk_picture_set_content_fit(GTK_PICTURE(wd->picker_preview), GTK_CONTENT_FIT_COVER);
	gtk_widget_set_hexpand(wd->picker_preview, TRUE);
	gtk_widget_set_vexpand(wd->picker_preview, TRUE);
	gtk_widget_set_overflow(wd->picker_preview, GTK_OVERFLOW_HIDDEN);
	gtk_widget_add_css_class(wd->picker_preview, "picker-preview");
	gtk_box_append(GTK_BOX(pane), wd->picker_preview);
	GtkWidget *hint = gtk_label_new("Click an image to preview");
	gtk_widget_add_css_class(hint,"dim-label");
	gtk_widget_set_halign(hint,GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(pane),hint);
	GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	gtk_widget_set_vexpand(sp,TRUE);
	gtk_box_append(GTK_BOX(pane),sp);
	GtkWidget *apply = gtk_button_new_with_label("Set Wallpaper");
	gtk_widget_add_css_class(apply,"suggested-action");
	g_signal_connect(apply,"clicked",G_CALLBACK(picker_apply),wd);
	gtk_box_append(GTK_BOX(pane),apply);
	GtkWidget *cancel = gtk_button_new_with_label("Cancel");
	g_signal_connect(cancel,"clicked",G_CALLBACK(picker_cancel),wd);
	gtk_box_append(GTK_BOX(pane),cancel);
	gtk_box_append(GTK_BOX(row),pane);
	gtk_box_append(GTK_BOX(root),row);
	gtk_window_set_child(GTK_WINDOW(wd->picker_window),root);
	//char *home = get_home_env();
	//load_dir(wd, home ? home : "/");
	//free(home);
	//load_dir(wd, "/usr/share/mrrobotos/mrsettings/Wallpapers");
	if (wd->start_in_home) {
		wd->start_in_home = FALSE;
		char *home = get_home_env();
		load_dir(wd, home ? home : "/");
		free(home);
	} else {
		load_dir(wd, "/usr/share/mrrobotos/mrsettings/Wallpapers");
	}
	gtk_window_present(GTK_WINDOW(wd->picker_window));
}

static void open_picker_home(GtkWidget *btn, gpointer ud) {
	WPData *wd = ud;
	wd->start_in_home = TRUE;
	open_picker(btn, ud);
}

/* ================================================================== */
/* Wi-Fi                                                                */
/* ================================================================== */
typedef struct {
	GtkWidget *network_list;
	GtkWidget *status_label;
	guint      refresh_id;
	gboolean   destroyed;
	GMutex     lock;
} WifiData;

typedef struct {
	char      ssid[256];
	char      security[64];
	WifiData *wd;
} ConnectData;

typedef struct {
	char      ssid[256];
	char      security[64];
	char      password[256];
	WifiData *wd;
} ConnectThreadData;

typedef struct {
	char      cmd[2048];
	WifiData *wd;
	gboolean  do_status_after;
	gboolean  do_refresh_after;
} CmdThreadData;

static void     wifi_refresh_internal(WifiData *wd, gboolean rescan);
static void     wifi_refresh_status_only(WifiData *wd);
static gboolean wifi_refresh_once(gpointer ud);
static gboolean wifi_refresh_rescan_once(gpointer ud);
static gboolean wifi_status_once(gpointer ud);
static gboolean wifi_auto_refresh(gpointer ud);
static void     do_connect(GtkWidget *btn, gpointer ud);
static void     do_disconnect(GtkWidget *btn, gpointer ud);


static gboolean _idle_wifi_status(gpointer ud)  { return wifi_status_once(ud); }
static gboolean _idle_wifi_refresh(gpointer ud) { return wifi_refresh_once(ud); }

static gpointer wifi_cmd_thread(gpointer ud) {
	CmdThreadData *td = ud;
	system(td->cmd);
	g_mutex_lock(&td->wd->lock);
	gboolean dead = td->wd->destroyed;
	g_mutex_unlock(&td->wd->lock);
	if (!dead) {
		if (td->do_status_after)  g_idle_add(_idle_wifi_status,  td->wd);
		if (td->do_refresh_after) g_idle_add(_idle_wifi_refresh, td->wd);
	}
	g_free(td);
	return NULL;
}

static void wifi_run_async(const char *cmd, WifiData *wd,
			   gboolean status_after, gboolean refresh_after) {
	CmdThreadData *td = g_new0(CmdThreadData, 1);
	strncpy(td->cmd, cmd, sizeof(td->cmd) - 1);
	td->wd = wd;
	td->do_status_after  = status_after;
	td->do_refresh_after = refresh_after;
	g_thread_unref(g_thread_new("nmcli", wifi_cmd_thread, td));
}

static void do_disconnect(GtkWidget *btn, gpointer ud) {
	WifiData *wd = ud;
	if (!wd || wd->destroyed) return;
	wifi_run_async(
		       "nmcli dev disconnect wlo1 2>/dev/null || "
		       "nmcli dev disconnect wlan0 2>/dev/null",
		       wd, TRUE, TRUE);
}

static gpointer wifi_connect_thread(gpointer ud) {
	ConnectThreadData *td = ud;
	char cmd[1024];
	snprintf(cmd, sizeof(cmd), "nmcli con up '%s' 2>/dev/null", td->ssid);
	int ret = system(cmd);
	if (ret != 0) {
		if (td->password[0])
			snprintf(cmd, sizeof(cmd),
				 "nmcli dev wifi connect '%s' password '%s' 2>/dev/null",
				 td->ssid, td->password);
		else
			snprintf(cmd, sizeof(cmd),
				 "nmcli dev wifi connect '%s' 2>/dev/null", td->ssid);
		system(cmd);
	}
	g_mutex_lock(&td->wd->lock);
	gboolean dead = td->wd->destroyed;
	g_mutex_unlock(&td->wd->lock);
	if (!dead) {
		g_idle_add(_idle_wifi_status,  td->wd);
		g_idle_add(_idle_wifi_refresh, td->wd);
	}
	g_free(td);
	return NULL;
}


static GtkWidget *make_signal_bars(int strength) {
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	int bars = strength >= 75 ? 4 : strength >= 50 ? 3 :
		strength >= 25 ? 2 : strength >  0  ? 1 : 0;
	for (int i = 0; i < 4; i++) {
		GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_set_size_request(bar, 4, 6 + i * 4);
		gtk_widget_set_valign(bar, GTK_ALIGN_END);
		gtk_widget_add_css_class(bar, i < bars ? "signal-bar-active" : "signal-bar-dim");
		gtk_box_append(GTK_BOX(box), bar);
	}
	return box;
}

static void wifi_refresh_status_only(WifiData *wd) {
	if (!wd || wd->destroyed) return;
	char *radio = run_cmd_str("nmcli radio wifi 2>/dev/null | tr -d '\\n'");
	g_strstrip(radio);
	gboolean wifi_on = g_str_has_prefix(radio, "enabled");
	g_free(radio);
	if (!wifi_on) { gtk_label_set_text(GTK_LABEL(wd->status_label), "Wi-Fi is disabled"); return; }
	char *connected = run_cmd_str(
				      "nmcli --escape no -t -f active,ssid dev wifi 2>/dev/null"
				      " | grep '^yes:' | cut -d: -f2- | tr -d '\\n'");
	g_strstrip(connected);
	if (strlen(connected) > 0) {
		char *msg = g_strdup_printf("Connected to %s", connected);
		gtk_label_set_text(GTK_LABEL(wd->status_label), msg);
		g_free(msg);
	} else {
		gtk_label_set_text(GTK_LABEL(wd->status_label), "Not connected");
	}
	GtkWidget *row = gtk_widget_get_first_child(wd->network_list);
	while (row) {
		GtkWidget *next = gtk_widget_get_next_sibling(row);
		GtkWidget *hbox = gtk_list_box_row_get_child(GTK_LIST_BOX_ROW(row));
		if (!hbox) { row = next; continue; }
		GtkWidget *signal_bars = gtk_widget_get_first_child(hbox);
		GtkWidget *info     = signal_bars ? gtk_widget_get_next_sibling(signal_bars) : NULL;
		GtkWidget *name_lbl = info ? gtk_widget_get_first_child(info) : NULL;
		GtkWidget *btn      = info ? gtk_widget_get_next_sibling(info) : NULL;
		if (!name_lbl || !btn) { row = next; continue; }
		const char *ssid = gtk_label_get_text(GTK_LABEL(name_lbl));
		gboolean is_active = strlen(connected) > 0 && strcmp(ssid, connected) == 0;
		if (is_active) gtk_widget_add_css_class(name_lbl, "wifi-active-name");
		else           gtk_widget_remove_css_class(name_lbl, "wifi-active-name");
		const char *cur_label = gtk_button_get_label(GTK_BUTTON(btn));
		gboolean showing_disconnect = cur_label && strcmp(cur_label, "Disconnect") == 0;
		gboolean showing_connecting = cur_label && strcmp(cur_label, "Connecting\xe2\x80\xa6") == 0;
		if (is_active && !showing_disconnect) {
			gtk_button_set_label(GTK_BUTTON(btn), "Disconnect");
			gtk_widget_set_sensitive(btn, TRUE);
			gtk_widget_remove_css_class(btn, "suggested-action");
			gtk_widget_add_css_class(btn, "destructive-action");
			g_signal_handlers_disconnect_matched(btn, G_SIGNAL_MATCH_FUNC,
							     0, 0, NULL, G_CALLBACK(do_connect), NULL);
			g_signal_connect(btn, "clicked", G_CALLBACK(do_disconnect), wd);
		} else if (!is_active && showing_disconnect) {
			gtk_button_set_label(GTK_BUTTON(btn), "Connect");
			gtk_widget_set_sensitive(btn, TRUE);
			gtk_widget_remove_css_class(btn, "destructive-action");
			gtk_widget_add_css_class(btn, "suggested-action");
			g_signal_handlers_disconnect_matched(btn, G_SIGNAL_MATCH_FUNC,
							     0, 0, NULL, G_CALLBACK(do_disconnect), NULL);
		} else if (!is_active && showing_connecting) {
			gtk_button_set_label(GTK_BUTTON(btn), "Connect");
			gtk_widget_set_sensitive(btn, TRUE);
		}
		row = next;
	}
	g_free(connected);
}

static gboolean wifi_status_once(gpointer ud) {
	WifiData *wd = ud;
	if (!wd || wd->destroyed) return G_SOURCE_REMOVE;
	wifi_refresh_status_only(wd);
	return G_SOURCE_REMOVE;
}

typedef struct {
	char     ssid[256];
	char     security[64];
	int      signal;
	gboolean active;
} NetEntry;

static int cmp_net(gconstpointer a, gconstpointer b) {
	const NetEntry *na = a, *nb = b;
	if (na->active != nb->active) return na->active ? -1 : 1;
	return nb->signal - na->signal;
}

static void wifi_refresh_internal(WifiData *wd, gboolean rescan) {
	if (!wd || wd->destroyed) return;
	GtkWidget *c;
	while ((c = gtk_widget_get_first_child(wd->network_list)))
		gtk_list_box_remove(GTK_LIST_BOX(wd->network_list), c);
	char *radio = run_cmd_str("nmcli radio wifi 2>/dev/null | tr -d '\\n'");
	g_strstrip(radio);
	gboolean wifi_on = g_str_has_prefix(radio, "enabled");
	g_free(radio);
	gtk_widget_set_sensitive(wd->network_list, wifi_on);
	if (!wifi_on) { gtk_label_set_text(GTK_LABEL(wd->status_label), "Wi-Fi is disabled"); return; }
	char *connected = run_cmd_str(
				      "nmcli --escape no -t -f active,ssid dev wifi 2>/dev/null"
				      " | grep '^yes:' | cut -d: -f2- | tr -d '\\n'");
	g_strstrip(connected);
	if (strlen(connected) > 0) {
		char *msg = g_strdup_printf("Connected to %s", connected);
		gtk_label_set_text(GTK_LABEL(wd->status_label), msg); g_free(msg);
	} else {
		gtk_label_set_text(GTK_LABEL(wd->status_label), "Not connected");
	}
	char scan_cmd[256];
	snprintf(scan_cmd, sizeof(scan_cmd),
		 "nmcli --escape no -t -f ssid,signal,security,active"
		 " dev wifi list %s 2>/dev/null",
		 rescan ? "--rescan yes" : "--rescan no");
	char  *output = run_cmd_str(scan_cmd);
	char **lines  = g_strsplit(output, "\n", -1);
	g_free(output);
	GArray     *nets = g_array_new(FALSE, FALSE, sizeof(NetEntry));
	GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	for (int i = 0; lines[i]; i++) {
		if (!lines[i][0]) continue;
		char *line = g_strdup(lines[i]);
		char *p_active = strrchr(line, ':'); if (!p_active) { g_free(line); continue; }
		*p_active++ = '\0';
		char *p_security = strrchr(line, ':'); if (!p_security) { g_free(line); continue; }
		*p_security++ = '\0';
		char *p_signal = strrchr(line, ':'); if (!p_signal) { g_free(line); continue; }
		*p_signal++ = '\0';
		const char *ssid = line;
		int signal = atoi(p_signal);
		const char *security = p_security;
		gboolean active = g_str_has_prefix(p_active, "yes") ||
			(strlen(connected) > 0 && strcmp(ssid, connected) == 0);
		if (!ssid[0] || g_hash_table_contains(seen, ssid)) { g_free(line); continue; }
		g_hash_table_insert(seen, g_strdup(ssid), GINT_TO_POINTER(1));
		NetEntry ne = {0};
		strncpy(ne.ssid,     ssid,     sizeof(ne.ssid)     - 1);
		strncpy(ne.security, security, sizeof(ne.security) - 1);
		ne.signal = signal; ne.active = active;
		g_array_append_val(nets, ne);
		g_free(line);
	}
	g_strfreev(lines);
	g_hash_table_destroy(seen);
	g_array_sort(nets, cmp_net);
	for (guint i = 0; i < nets->len; i++) {
		NetEntry *ne = &g_array_index(nets, NetEntry, i);
		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
		gtk_widget_set_margin_start(row, 12); gtk_widget_set_margin_end(row, 12);
		gtk_widget_set_margin_top(row, 8);    gtk_widget_set_margin_bottom(row, 8);
		gtk_box_append(GTK_BOX(row), make_signal_bars(ne->signal));
		GtkWidget *info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		gtk_widget_set_hexpand(info, TRUE);
		GtkWidget *name_lbl = gtk_label_new(ne->ssid);
		gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
		if (ne->active) gtk_widget_add_css_class(name_lbl, "wifi-active-name");
		gtk_box_append(GTK_BOX(info), name_lbl);
		const char *sec_str = (ne->security[0] && strcmp(ne->security, "--") != 0)
			? "\xf0\x9f\x94\x92 Secured" : "Open";
		GtkWidget *sec_lbl = gtk_label_new(sec_str);
		gtk_widget_set_halign(sec_lbl, GTK_ALIGN_START);
		gtk_widget_add_css_class(sec_lbl, "dim-label");
		gtk_widget_add_css_class(sec_lbl, "caption");
		gtk_box_append(GTK_BOX(info), sec_lbl);
		char sig_str[32];
		snprintf(sig_str, sizeof(sig_str), "Signal: %d%%", ne->signal);
		GtkWidget *sig_lbl = gtk_label_new(sig_str);
		gtk_widget_set_halign(sig_lbl, GTK_ALIGN_START);
		gtk_widget_add_css_class(sig_lbl, "dim-label");
		gtk_widget_add_css_class(sig_lbl, "caption");
		gtk_box_append(GTK_BOX(info), sig_lbl);
		gtk_box_append(GTK_BOX(row), info);
		GtkWidget *btn;
		if (ne->active) {
			btn = gtk_button_new_with_label("Disconnect");
			gtk_widget_add_css_class(btn, "destructive-action");
			g_signal_connect(btn, "clicked", G_CALLBACK(do_disconnect), wd);
		} else {
			ConnectData *cd = g_new0(ConnectData, 1);
			strncpy(cd->ssid,     ne->ssid,     sizeof(cd->ssid)     - 1);
			strncpy(cd->security, ne->security, sizeof(cd->security) - 1);
			cd->wd = wd;
			btn = gtk_button_new_with_label("Connect");
			gtk_widget_add_css_class(btn, "suggested-action");
			g_signal_connect_data(btn, "clicked",
					      G_CALLBACK(do_connect), cd, (GClosureNotify)g_free, 0);
		}
		gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
		gtk_widget_set_size_request(btn, 110, -1);
		gtk_box_append(GTK_BOX(row), btn);
		gtk_list_box_append(GTK_LIST_BOX(wd->network_list), row);
	}
	g_array_free(nets, TRUE);
	g_free(connected);
}

static gboolean wifi_refresh_once(gpointer ud) {
	WifiData *wd = ud;
	if (!wd || wd->destroyed) return G_SOURCE_REMOVE;
	wifi_refresh_internal(wd, FALSE);
	return G_SOURCE_REMOVE;
}
static gboolean wifi_refresh_rescan_once(gpointer ud) {
	WifiData *wd = ud;
	if (!wd || wd->destroyed) return G_SOURCE_REMOVE;
	wifi_refresh_internal(wd, TRUE);
	return G_SOURCE_REMOVE;
}
static gboolean wifi_auto_refresh(gpointer ud) {
	WifiData *wd = ud;
	if (!wd || wd->destroyed) return G_SOURCE_REMOVE;
	wifi_refresh_internal(wd, FALSE);
	return G_SOURCE_CONTINUE;
}

static void wifi_toggle(WifiData *wd) {
	if (!wd || wd->destroyed) return;
	char *radio = run_cmd_str("nmcli radio wifi 2>/dev/null | tr -d '\\n'");
	g_strstrip(radio);
	gboolean on = g_str_has_prefix(radio, "enabled");
	g_free(radio);
	gtk_label_set_text(GTK_LABEL(wd->status_label),
			   on ? "Turning off\xe2\x80\xa6" : "Turning on\xe2\x80\xa6");
	wifi_run_async(on ? "nmcli radio wifi off" : "nmcli radio wifi on", wd, FALSE, TRUE);
}

static void wifi_page_destroyed(GtkWidget *widget, gpointer ud) {
	WifiData *wd = ud;
	g_mutex_lock(&wd->lock);
	wd->destroyed = TRUE;
	g_mutex_unlock(&wd->lock);
	if (wd->refresh_id) { g_source_remove(wd->refresh_id); wd->refresh_id = 0; }
	g_mutex_clear(&wd->lock);
	g_free(wd);
}

	static void
wifi_row_separator (GtkListBoxRow *row, GtkListBoxRow *before, gpointer ud)
{
	if (!before) return;
	gtk_list_box_row_set_header (row,
				     gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
}

GtkWidget *wifi_settings(void) {
	WifiData *wd = g_new0(WifiData, 1);
	g_mutex_init(&wd->lock);
	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand(root, TRUE); gtk_widget_set_vexpand(root, TRUE);
	GtkWidget *header = make_page_header("network-wireless-symbolic", "Wi-Fi");
	//char *radio = run_cmd_str("nmcli radio wifi 2>/dev/null | tr -d '\\n'");
	//g_strstrip(radio);
	//GtkWidget *sw = gtk_switch_new();
	//gtk_switch_set_active(GTK_SWITCH(sw), g_str_has_prefix(radio, "enabled"));
	//g_free(radio);
	GtkWidget *sw = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(sw), TRUE);
	gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
	g_signal_connect_swapped(sw, "notify::active", G_CALLBACK(wifi_toggle), wd);
	gtk_box_append(GTK_BOX(header), sw);
	GtkWidget *refresh_btn = gtk_button_new_from_icon_name("view-refresh-symbolic");
	gtk_widget_add_css_class(refresh_btn, "flat");
	gtk_widget_set_valign(refresh_btn, GTK_ALIGN_CENTER);
	g_signal_connect_swapped(refresh_btn, "clicked", G_CALLBACK(wifi_refresh_rescan_once), wd);
	gtk_box_append(GTK_BOX(header), refresh_btn);
	gtk_box_append(GTK_BOX(root), header);
	wd->status_label = gtk_label_new("Scanning\xe2\x80\xa6");
	gtk_widget_add_css_class(wd->status_label, "dim-label");
	gtk_widget_set_halign(wd->status_label, GTK_ALIGN_START);
	gtk_widget_set_margin_start(wd->status_label, 20);
	gtk_widget_set_margin_bottom(wd->status_label, 8);
	gtk_box_append(GTK_BOX(root), wd->status_label);
	gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget *scr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr, TRUE);
	wd->network_list = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(wd->network_list), GTK_SELECTION_NONE);

	//gtk_widget_add_css_class(wd->network_list, "boxed-list");
	//gtk_widget_set_margin_start(wd->network_list, 16); gtk_widget_set_margin_end(wd->network_list, 16);

	gtk_widget_add_css_class(wd->network_list, "wifi-net-list");
	gtk_list_box_set_header_func(GTK_LIST_BOX(wd->network_list),
				     wifi_row_separator, NULL, NULL);
	gtk_widget_set_margin_start (wd->network_list, 16);
	gtk_widget_set_margin_end   (wd->network_list, 16);
	gtk_widget_set_margin_top   (wd->network_list, 12);
	gtk_widget_set_margin_bottom(wd->network_list, 12);

	//gtk_widget_set_margin_top(wd->network_list, 12);
	//gtk_widget_set_margin_bottom(wd->network_list, 12);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr), wd->network_list);
	gtk_box_append(GTK_BOX(root), scr);
	//g_timeout_add(300, wifi_refresh_once, wd);
	g_idle_add(wifi_refresh_once, wd);
	wd->refresh_id = g_timeout_add_seconds(5, wifi_auto_refresh, wd);
	g_signal_connect(root, "destroy", G_CALLBACK(wifi_page_destroyed), wd);
	return root;
}

/* ================================================================== */
/* Bluetooth                                                            */
/* ================================================================== */
typedef struct {
	GtkWidget *device_list;
	GtkWidget *status_label;
	GtkWidget *scan_btn;
	guint      refresh_id;
	gboolean   destroyed;
	gboolean   scanning;
	GPid       scan_pid;
	gint       bt_stdin_fd;
	GMutex     lock;
} BtData;

typedef struct {
	char     mac[64];
	char     name[256];
	char     type[64];
	gboolean paired;
	gboolean connected;
	gboolean trusted;
} BtEntry;

static void     bt_refresh_internal(BtData *bd);
static gboolean bt_refresh_once(gpointer ud);
static gboolean bt_auto_refresh(gpointer ud);

typedef struct { char cmd[2048]; BtData *bd; } BtCmdData;

static gboolean _idle_bt_refresh(gpointer ud) { return bt_refresh_once(ud); }

static gpointer bt_cmd_thread(gpointer ud) {
	BtCmdData *td = ud;
	system(td->cmd);
	g_mutex_lock(&td->bd->lock);
	gboolean dead = td->bd->destroyed;
	g_mutex_unlock(&td->bd->lock);
	if (!dead) g_idle_add(_idle_bt_refresh, td->bd);
	g_free(td);
	return NULL;
}

static void bt_run_async(const char *cmd, BtData *bd) {
	BtCmdData *td = g_new0(BtCmdData, 1);
	strncpy(td->cmd, cmd, sizeof(td->cmd) - 1);
	td->bd = bd;
	g_thread_unref(g_thread_new("btctl", bt_cmd_thread, td));
}

typedef struct { char mac[64]; BtData *bd; } BtActionData;

static void bt_do_connect(GtkWidget *btn, gpointer ud) {
	BtActionData *ad = ud;
	if (!ad->bd || ad->bd->destroyed) return;
	char cmd[256];
	snprintf(cmd, sizeof(cmd), "bluetoothctl connect %s 2>/dev/null", ad->mac);
	gtk_widget_set_sensitive(btn, FALSE);
	gtk_button_set_label(GTK_BUTTON(btn), "Connecting\xe2\x80\xa6");
	bt_run_async(cmd, ad->bd);
}
static void bt_do_disconnect(GtkWidget *btn, gpointer ud) {
	BtActionData *ad = ud;
	if (!ad->bd || ad->bd->destroyed) return;
	char cmd[256];
	snprintf(cmd, sizeof(cmd), "bluetoothctl disconnect %s 2>/dev/null", ad->mac);
	bt_run_async(cmd, ad->bd);
}

typedef struct { char mac[64]; BtData *bd; GtkWidget *btn; } BtPairData;

static gpointer bt_pair_thread(gpointer ud) {
	BtPairData *pd = ud;
	BtData     *bd = pd->bd;
	g_mutex_lock(&bd->lock);
	gint sfd = bd->bt_stdin_fd; gboolean dead = bd->destroyed;
	g_mutex_unlock(&bd->lock);
	if (!dead && sfd >= 0) {
		char cmd[128];
		snprintf(cmd, sizeof(cmd), "pair %s\n", pd->mac);
		write(sfd, cmd, strlen(cmd));
		g_usleep(8000000);
		g_mutex_lock(&bd->lock);
		sfd = bd->bt_stdin_fd; dead = bd->destroyed;
		g_mutex_unlock(&bd->lock);
		if (!dead && sfd >= 0) {
			snprintf(cmd, sizeof(cmd), "trust %s\n", pd->mac);
			write(sfd, cmd, strlen(cmd));
			g_usleep(500000);
			snprintf(cmd, sizeof(cmd), "connect %s\n", pd->mac);
			write(sfd, cmd, strlen(cmd));
		}
	} else if (!dead) {
		char cmd[512];
		snprintf(cmd, sizeof(cmd),
			 "bluetoothctl -- pair %s 2>/dev/null && "
			 "bluetoothctl -- trust %s 2>/dev/null && "
			 "bluetoothctl -- connect %s 2>/dev/null",
			 pd->mac, pd->mac, pd->mac);
		system(cmd);
	}
	g_mutex_lock(&bd->lock); dead = bd->destroyed; g_mutex_unlock(&bd->lock);
	if (!dead) g_idle_add(_idle_bt_refresh, bd);
	g_free(pd);
	return NULL;
}

static void bt_do_pair(GtkWidget *btn, gpointer ud) {
	BtActionData *ad = ud;
	if (!ad->bd || ad->bd->destroyed) return;
	gtk_widget_set_sensitive(btn, FALSE);
	gtk_button_set_label(GTK_BUTTON(btn), "Pairing\xe2\x80\xa6");
	BtPairData *pd = g_new0(BtPairData, 1);
	strncpy(pd->mac, ad->mac, sizeof(pd->mac) - 1);
	pd->bd = ad->bd; pd->btn = btn;
	g_thread_unref(g_thread_new("bt-pair", bt_pair_thread, pd));
}

static void bt_do_remove(GtkWidget *btn, gpointer ud) {
	BtActionData *ad = ud;
	if (!ad->bd || ad->bd->destroyed) return;
	char cmd[256];
	snprintf(cmd, sizeof(cmd), "bluetoothctl remove %s 2>/dev/null", ad->mac);
	bt_run_async(cmd, ad->bd);
}

static const char *bt_icon(const char *type) {
	if (!type || !type[0])                     return "bluetooth-symbolic";
	if (strstr(type, "phone"))                 return "phone-symbolic";
	if (strstr(type, "computer"))              return "computer-symbolic";
	if (strstr(type, "audio") || strstr(type, "headset") ||
	    strstr(type, "headphone") || strstr(type, "speaker"))
		return "audio-headphones-symbolic";
	if (strstr(type, "input-keyboard"))        return "input-keyboard-symbolic";
	if (strstr(type, "input-mouse"))           return "input-mouse-symbolic";
	if (strstr(type, "input-gaming"))          return "input-gaming-symbolic";
	if (strstr(type, "printer"))               return "printer-symbolic";
	return "bluetooth-symbolic";
}

static void bt_refresh_internal(BtData *bd) {
	if (!bd || bd->destroyed) return;
	GtkWidget *c;
	while ((c = gtk_widget_get_first_child(bd->device_list)))
		gtk_list_box_remove(GTK_LIST_BOX(bd->device_list), c);
	char *powered_raw = run_cmd_str(
					"bluetoothctl show 2>/dev/null | grep 'Powered:' | awk '{print $2}' | tr -d '\\n'");
	g_strstrip(powered_raw);
	gboolean bt_on = (strcmp(powered_raw, "yes") == 0);
	g_free(powered_raw);
	gtk_widget_set_sensitive(bd->device_list, bt_on);
	if (!bt_on) { gtk_label_set_text(GTK_LABEL(bd->status_label), "Bluetooth is off"); return; }
	gtk_label_set_text(GTK_LABEL(bd->status_label),
			   bd->scanning ? "Scanning for devices\xe2\x80\xa6" : "Bluetooth on");
	char *devlist = run_cmd_str("bluetoothctl devices 2>/dev/null");
	char **lines  = g_strsplit(devlist, "\n", -1);
	g_free(devlist);
	GArray *devs = g_array_new(FALSE, FALSE, sizeof(BtEntry));
	for (int i = 0; lines[i]; i++) {
		if (!g_str_has_prefix(lines[i], "Device ")) continue;
		const char *rest = lines[i] + 7;
		if (strlen(rest) < 17) continue;
		char mac[64]; strncpy(mac, rest, 17); mac[17] = '\0';
		char info_cmd[128];
		snprintf(info_cmd, sizeof(info_cmd), "bluetoothctl info %s 2>/dev/null", mac);
		char *info = run_cmd_str(info_cmd);
		BtEntry be = {0};
		strncpy(be.mac, mac, sizeof(be.mac) - 1);
		char *nl = strstr(info, "\tName: ");
		if (nl) {
			nl += 7; char *end = strchr(nl, '\n');
			size_t len = end ? (size_t)(end - nl) : strlen(nl);
			if (len >= sizeof(be.name)) len = sizeof(be.name) - 1;
			memcpy(be.name, nl, len);
		} else strncpy(be.name, mac, sizeof(be.name) - 1);
		char *il = strstr(info, "\tIcon: ");
		if (il) {
			il += 7; char *end = strchr(il, '\n');
			size_t len = end ? (size_t)(end - il) : strlen(il);
			if (len >= sizeof(be.type)) len = sizeof(be.type) - 1;
			memcpy(be.type, il, len);
		}
		be.paired    = strstr(info, "\tPaired: yes")    != NULL;
		be.connected = strstr(info, "\tConnected: yes") != NULL;
		be.trusted   = strstr(info, "\tTrusted: yes")   != NULL;
		g_free(info);
		g_array_append_val(devs, be);
	}
	g_strfreev(lines);
	/* sort: connected > paired > rest */
	for (guint i = 0; i + 1 < devs->len; i++)
		for (guint j = i + 1; j < devs->len; j++) {
			BtEntry *a = &g_array_index(devs, BtEntry, i);
			BtEntry *b = &g_array_index(devs, BtEntry, j);
			int sa = (a->connected?2:0)+(a->paired?1:0);
			int sb = (b->connected?2:0)+(b->paired?1:0);
			if (sb > sa) { BtEntry tmp = *a; *a = *b; *b = tmp; }
		}
	for (guint i = 0; i < devs->len; i++) {
		BtEntry *be = &g_array_index(devs, BtEntry, i);
		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
		gtk_widget_set_margin_start(row, 12); gtk_widget_set_margin_end(row, 12);
		gtk_widget_set_margin_top(row, 8);    gtk_widget_set_margin_bottom(row, 8);
		GtkWidget *ico = gtk_image_new_from_icon_name(bt_icon(be->type));
		gtk_image_set_pixel_size(GTK_IMAGE(ico), 24);
		gtk_widget_set_valign(ico, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(row), ico);
		GtkWidget *info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		gtk_widget_set_hexpand(info_box, TRUE);
		GtkWidget *name_lbl = gtk_label_new(be->name);
		gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
		if (be->connected) gtk_widget_add_css_class(name_lbl, "wifi-active-name");
		gtk_box_append(GTK_BOX(info_box), name_lbl);
		const char *st = be->connected ? "Connected" :
			be->paired    ? "Paired, not connected" : "Not paired";
		GtkWidget *sl = gtk_label_new(st);
		gtk_widget_set_halign(sl, GTK_ALIGN_START);
		gtk_widget_add_css_class(sl, "dim-label"); gtk_widget_add_css_class(sl, "caption");
		gtk_box_append(GTK_BOX(info_box), sl);
		GtkWidget *ml = gtk_label_new(be->mac);
		gtk_widget_set_halign(ml, GTK_ALIGN_START);
		gtk_widget_add_css_class(ml, "dim-label"); gtk_widget_add_css_class(ml, "caption");
		gtk_box_append(GTK_BOX(info_box), ml);
		gtk_box_append(GTK_BOX(row), info_box);
		GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_widget_set_valign(btn_box, GTK_ALIGN_CENTER);
		if (be->paired) {
			BtActionData *ad = g_new0(BtActionData, 1);
			strncpy(ad->mac, be->mac, sizeof(ad->mac)-1); ad->bd = bd;
			GtkWidget *btn = gtk_button_new_with_label(be->connected ? "Disconnect" : "Connect");
			gtk_widget_add_css_class(btn, be->connected ? "destructive-action" : "suggested-action");
			gtk_widget_set_size_request(btn, 110, -1);
			g_signal_connect_data(btn, "clicked",
					      be->connected ? G_CALLBACK(bt_do_disconnect) : G_CALLBACK(bt_do_connect),
					      ad, (GClosureNotify)g_free, 0);
			gtk_box_append(GTK_BOX(btn_box), btn);
			BtActionData *adr = g_new0(BtActionData, 1);
			strncpy(adr->mac, be->mac, sizeof(adr->mac)-1); adr->bd = bd;
			GtkWidget *rbtn = gtk_button_new_from_icon_name("user-trash-symbolic");
			gtk_widget_add_css_class(rbtn, "flat");
			gtk_widget_set_tooltip_text(rbtn, "Remove device");
			g_signal_connect_data(rbtn, "clicked", G_CALLBACK(bt_do_remove), adr,
					      (GClosureNotify)g_free, 0);
			gtk_box_append(GTK_BOX(btn_box), rbtn);
		} else {
			BtActionData *ad = g_new0(BtActionData, 1);
			strncpy(ad->mac, be->mac, sizeof(ad->mac)-1); ad->bd = bd;
			GtkWidget *btn = gtk_button_new_with_label("Pair");
			gtk_widget_add_css_class(btn, "suggested-action");
			gtk_widget_set_size_request(btn, 110, -1);
			g_signal_connect_data(btn, "clicked", G_CALLBACK(bt_do_pair), ad,
					      (GClosureNotify)g_free, 0);
			gtk_box_append(GTK_BOX(btn_box), btn);
		}
		gtk_box_append(GTK_BOX(row), btn_box);
		gtk_list_box_append(GTK_LIST_BOX(bd->device_list), row);
	}
	g_array_free(devs, TRUE);
}

static gboolean bt_refresh_once(gpointer ud) {
	BtData *bd = ud;
	if (!bd || bd->destroyed) return G_SOURCE_REMOVE;
	bt_refresh_internal(bd); return G_SOURCE_REMOVE;
}
static gboolean bt_auto_refresh(gpointer ud) {
	BtData *bd = ud;
	if (!bd || bd->destroyed) return G_SOURCE_REMOVE;
	bt_refresh_internal(bd); return G_SOURCE_CONTINUE;
}

static void bt_toggle(BtData *bd) {
	if (!bd || bd->destroyed) return;
	char *pr = run_cmd_str(
			       "bluetoothctl show 2>/dev/null | grep 'Powered:' | awk '{print $2}' | tr -d '\\n'");
	g_strstrip(pr);
	gboolean on = (strcmp(pr, "yes") == 0);
	g_free(pr);
	gtk_label_set_text(GTK_LABEL(bd->status_label),
			   on ? "Turning off\xe2\x80\xa6" : "Turning on\xe2\x80\xa6");
	bt_run_async(on ? "bluetoothctl power off 2>/dev/null"
		     : "bluetoothctl power on  2>/dev/null", bd);
}

static gpointer bt_scan_thread(gpointer ud) {
	BtData *bd = ud;
	char *argv[] = { "bluetoothctl", NULL };
	GError *err = NULL;
	gint stdin_fd, stdout_fd;
	GPid pid;
	if (!g_spawn_async_with_pipes(NULL, argv, NULL,
				      G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
				      NULL, NULL, &pid, &stdin_fd, &stdout_fd, NULL, &err)) {
		if (err) g_error_free(err);
		return NULL;
	}
	g_mutex_lock(&bd->lock);
	bd->scan_pid = pid; bd->bt_stdin_fd = stdin_fd;
	g_mutex_unlock(&bd->lock);
	write(stdin_fd, "agent NoInputNoOutput\n", 21); g_usleep(300000);
	write(stdin_fd, "default-agent\n", 14); g_usleep(300000);
	write(stdin_fd, "scan on\n", 8);
	FILE *out = fdopen(stdout_fd, "r");
	char line[512];
	while (out && fgets(line, sizeof(line), out)) {
		g_mutex_lock(&bd->lock);
		gboolean dead = bd->destroyed, stopped = (bd->scan_pid == 0);
		gint sfd = bd->bt_stdin_fd;
		g_mutex_unlock(&bd->lock);
		if (dead || stopped) break;
		if (strstr(line, "(yes/no)") || strstr(line, "Confirm passkey") ||
		    strstr(line, "Request confirmation"))
			if (sfd >= 0) write(sfd, "yes\n", 4);
		if (strstr(line, "[NEW] Device") || strstr(line, "[CHG] Device") ||
		    strstr(line, "Paired: yes")  || strstr(line, "Connected: yes"))
			g_idle_add(_idle_bt_refresh, bd);
	}
	if (out) fclose(out);
	g_spawn_close_pid(pid);
	return NULL;
}

static void bt_scan_toggle(GtkWidget *btn, gpointer ud) {
	BtData *bd = ud;
	if (!bd || bd->destroyed) return;
	g_mutex_lock(&bd->lock);
	gboolean scanning = bd->scanning;
	g_mutex_unlock(&bd->lock);
	if (scanning) {
		g_mutex_lock(&bd->lock);
		GPid pid = bd->scan_pid; gint sfd = bd->bt_stdin_fd;
		bd->scanning = FALSE; bd->scan_pid = 0; bd->bt_stdin_fd = -1;
		g_mutex_unlock(&bd->lock);
		if (sfd >= 0) { write(sfd, "scan off\n", 9); g_usleep(300000); close(sfd); }
		if (pid) kill((pid_t)pid, SIGTERM);
		gtk_button_set_label(GTK_BUTTON(btn), "Scan");
		gtk_widget_remove_css_class(btn, "suggested-action");
		gtk_label_set_text(GTK_LABEL(bd->status_label), "Bluetooth on");
		g_idle_add(_idle_bt_refresh, bd);
	} else {
		g_mutex_lock(&bd->lock); bd->scanning = TRUE; g_mutex_unlock(&bd->lock);
		gtk_button_set_label(GTK_BUTTON(btn), "Stop");
		gtk_widget_add_css_class(btn, "suggested-action");
		gtk_label_set_text(GTK_LABEL(bd->status_label), "Scanning for devices\xe2\x80\xa6");
		g_thread_unref(g_thread_new("bt-scan", bt_scan_thread, bd));
	}
}

static void bt_page_destroyed(GtkWidget *widget, gpointer ud) {
	BtData *bd = ud;
	g_mutex_lock(&bd->lock);
	bd->destroyed = TRUE;
	GPid pid = bd->scan_pid; gint sfd = bd->bt_stdin_fd;
	bd->scan_pid = 0; bd->bt_stdin_fd = -1; bd->scanning = FALSE;
	g_mutex_unlock(&bd->lock);
	if (sfd >= 0) { write(sfd, "scan off\n", 9); g_usleep(200000); close(sfd); }
	if (pid) kill((pid_t)pid, SIGTERM);
	if (bd->refresh_id) { g_source_remove(bd->refresh_id); bd->refresh_id = 0; }
	g_mutex_clear(&bd->lock);
	g_free(bd);
}

//
GtkWidget *bluetooth_settings(void) {
	BtData *bd = g_new0(BtData, 1);
	g_mutex_init(&bd->lock);
	bd->bt_stdin_fd = -1;
	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand(root, TRUE); gtk_widget_set_vexpand(root, TRUE);
	GtkWidget *header = make_page_header("bluetooth-symbolic", "Bluetooth");
	char *pr = run_cmd_str(
			       "bluetoothctl show 2>/dev/null | grep 'Powered:' | awk '{print $2}' | tr -d '\\n'");
	g_strstrip(pr);
	GtkWidget *sw = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(sw), strcmp(pr, "yes") == 0);
	g_free(pr);
	gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
	g_signal_connect_swapped(sw, "notify::active", G_CALLBACK(bt_toggle), bd);
	gtk_box_append(GTK_BOX(header), sw);
	GtkWidget *scan_btn = gtk_button_new_with_label("Scan");
	gtk_widget_add_css_class(scan_btn, "flat");
	gtk_widget_set_valign(scan_btn, GTK_ALIGN_CENTER);
	g_signal_connect(scan_btn, "clicked", G_CALLBACK(bt_scan_toggle), bd);
	gtk_box_append(GTK_BOX(header), scan_btn);
	bd->scan_btn = scan_btn;
	GtkWidget *ref_btn = gtk_button_new_from_icon_name("view-refresh-symbolic");
	gtk_widget_add_css_class(ref_btn, "flat");
	gtk_widget_set_valign(ref_btn, GTK_ALIGN_CENTER);
	g_signal_connect_swapped(ref_btn, "clicked", G_CALLBACK(bt_refresh_once), bd);
	gtk_box_append(GTK_BOX(header), ref_btn);
	gtk_box_append(GTK_BOX(root), header);
	bd->status_label = gtk_label_new("Loading\xe2\x80\xa6");
	gtk_widget_add_css_class(bd->status_label, "dim-label");
	gtk_widget_set_halign(bd->status_label, GTK_ALIGN_START);
	gtk_widget_set_margin_start(bd->status_label, 20);
	gtk_widget_set_margin_bottom(bd->status_label, 8);
	gtk_box_append(GTK_BOX(root), bd->status_label);
	gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget *scr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr, TRUE);
	bd->device_list = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(bd->device_list), GTK_SELECTION_NONE);
	gtk_widget_add_css_class(bd->device_list, "boxed-list");
	gtk_widget_set_margin_start(bd->device_list, 16); gtk_widget_set_margin_end(bd->device_list, 16);
	gtk_widget_set_margin_top(bd->device_list, 12);   gtk_widget_set_margin_bottom(bd->device_list, 12);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr), bd->device_list);
	gtk_box_append(GTK_BOX(root), scr);
	g_timeout_add(400, bt_refresh_once, bd);
	bd->refresh_id = g_timeout_add_seconds(10, bt_auto_refresh, bd);
	g_signal_connect(root, "destroy", G_CALLBACK(bt_page_destroyed), bd);
	return root;
}

/* ================================================================== */
/* Battery                                                              */
/* ================================================================== */
typedef struct { int capacity; char status[32]; } BatGaugeData;

static void bat_gauge_draw(GtkDrawingArea *da, cairo_t *cr, int w, int h, gpointer ud) {
	BatGaugeData *gd = ud;
	double cx = w/2.0, cy = h/2.0, r = MIN(w,h)/2.0-16, lw = 16.0;
	cairo_set_line_width(cr, lw);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_set_source_rgba(cr, 0,0,0, 0.08);
	cairo_arc(cr, cx, cy, r, G_PI*0.75, G_PI*2.25); cairo_stroke(cr);
	double angle_end = G_PI*0.75 + (gd->capacity/100.0)*G_PI*1.5;
	double red, green, blue;
	if      (gd->capacity > 60) { red=0.11; green=0.72; blue=0.40; }
	else if (gd->capacity > 25) { red=1.00; green=0.60; blue=0.00; }
	else                        { red=0.90; green=0.11; blue=0.14; }
	cairo_set_source_rgb(cr, red, green, blue);
	cairo_arc(cr, cx, cy, r, G_PI*0.75, angle_end); cairo_stroke(cr);
	char pct[8]; snprintf(pct, sizeof(pct), "%d%%", gd->capacity);
	cairo_set_source_rgb(cr, 0.1,0.1,0.1);
	cairo_select_font_face(cr,"Sans",CAIRO_FONT_SLANT_NORMAL,CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, r*0.42);
	cairo_text_extents_t te;
	cairo_text_extents(cr, pct, &te);
	cairo_move_to(cr, cx-te.width/2-te.x_bearing, cy-te.height/2-te.y_bearing);
	cairo_show_text(cr, pct);
	cairo_set_font_size(cr, r*0.18);
	cairo_set_source_rgba(cr, 0.1,0.1,0.1, 0.55);
	cairo_text_extents(cr, gd->status, &te);
	cairo_move_to(cr, cx-te.width/2-te.x_bearing, cy+r*0.34);
	cairo_show_text(cr, gd->status);
}

static GtkWidget *make_bat_card(const char *title, const char *value) {
	GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_set_size_request(card, 150, 68);
	gtk_widget_add_css_class(card, "bat-card");
	gtk_widget_set_margin_start(card,6); gtk_widget_set_margin_end(card,6);
	gtk_widget_set_margin_top(card,6);   gtk_widget_set_margin_bottom(card,6);
	GtkWidget *val = gtk_label_new(value);
	gtk_widget_add_css_class(val, "title-3");
	gtk_widget_set_halign(val, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(card), val);
	GtkWidget *lbl = gtk_label_new(title);
	gtk_widget_add_css_class(lbl, "dim-label"); gtk_widget_add_css_class(lbl, "caption");
	gtk_widget_set_halign(lbl, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(card), lbl);
	return card;
}
static void card_set_value(GtkWidget *card, const char *value) {
	GtkWidget *val = gtk_widget_get_first_child(card);
	if (val && GTK_IS_LABEL(val)) gtk_label_set_text(GTK_LABEL(val), value);
}

typedef struct {
	GtkWidget    *gauge, *devlbl;
	GtkWidget    *card_health, *card_power, *card_time;
	GtkWidget    *card_cycles, *card_voltage, *card_temp;
	GtkWidget    *gov_performance, *gov_balanced, *gov_powersave;
	BatGaugeData *gauge_data;
	guint         timer_id;
	gboolean      destroyed;
} BatData;

static const char *bat_node(void) {
	if (g_file_test("/sys/class/power_supply/BAT0/capacity", G_FILE_TEST_EXISTS)) return "BAT0";
	if (g_file_test("/sys/class/power_supply/BAT1/capacity", G_FILE_TEST_EXISTS)) return "BAT1";
	return NULL;
}

#define BREAD(bat, field, out, sz) do { \
	char _p[128]; snprintf(_p,sizeof(_p),"/sys/class/power_supply/%s/%s",bat,field); \
	FILE *_f=fopen(_p,"r"); \
	if(_f){fgets(out,sz,_f);fclose(_f);char*_n=strchr(out,'\n');if(_n)*_n='\0';} \
	else strncpy(out,"N/A",sz); } while(0)

static void bat_read_and_update(BatData *bd) {
	if (!bd || bd->destroyed) return;
	const char *bat = bat_node();
	char s_capacity[16]="N/A", s_status[32]="N/A", s_charge_full[32]="N/A";
	char s_charge_design[32]="N/A", s_charge_now[32]="N/A", s_current[32]="N/A";
	char s_voltage[32]="N/A", s_cycles[16]="N/A", s_temp[16]="N/A";
	char s_manufacturer[64]="N/A", s_model[64]="N/A", s_tech[32]="N/A";
	if (bat) {
		BREAD(bat,"capacity",          s_capacity,    sizeof(s_capacity));
		BREAD(bat,"status",            s_status,      sizeof(s_status));
		BREAD(bat,"charge_full",       s_charge_full, sizeof(s_charge_full));
		BREAD(bat,"charge_full_design",s_charge_design,sizeof(s_charge_design));
		BREAD(bat,"charge_now",        s_charge_now,  sizeof(s_charge_now));
		BREAD(bat,"current_now",       s_current,     sizeof(s_current));
		BREAD(bat,"voltage_now",       s_voltage,     sizeof(s_voltage));
		BREAD(bat,"cycle_count",       s_cycles,      sizeof(s_cycles));
		BREAD(bat,"temp",              s_temp,        sizeof(s_temp));
		BREAD(bat,"manufacturer",      s_manufacturer,sizeof(s_manufacturer));
		BREAD(bat,"model_name",        s_model,       sizeof(s_model));
		BREAD(bat,"technology",        s_tech,        sizeof(s_tech));
	}
	int  capacity = bat ? atoi(s_capacity) : 0;
	long cf=atol(s_charge_full), cd=atol(s_charge_design);
	long cnow=atol(s_charge_now), curr=atol(s_current), volt=atol(s_voltage);
	char s_health[16]="N/A";
	if (cf>0 && cd>0) snprintf(s_health,sizeof(s_health),"%ld%%",cf*100/cd);
	char s_power[32]="N/A";
	if (volt>0 && curr!=0)
		snprintf(s_power,sizeof(s_power),"%.2f W",
			 (double)(volt/1000.0)*(double)(curr<0?-curr:curr)/1000.0/1e6);
	else if (bat && curr==0) strncpy(s_power,"0.00 W",sizeof(s_power));
	char s_time[32]="N/A";
	if (strcmp(s_status,"Full")==0) strncpy(s_time,"Full",sizeof(s_time));
	else if (curr!=0 && cnow>0) {
		long denom=curr<0?-curr:curr;
		long delta=strcmp(s_status,"Discharging")==0?cnow:(cf>cnow?cf-cnow:0);
		if (denom>0 && delta>0) {
			long mins=(delta*60)/denom;
			snprintf(s_time,sizeof(s_time),"%ldh %ldm",mins/60,mins%60);
		}
	}
	char s_temp_fmt[32]="N/A";
	if (strcmp(s_temp,"N/A")!=0)
		snprintf(s_temp_fmt,sizeof(s_temp_fmt),"%.1f °C",atof(s_temp)/10.0);
	char s_volt_fmt[32]="N/A";
	if (strcmp(s_voltage,"N/A")!=0)
		snprintf(s_volt_fmt,sizeof(s_volt_fmt),"%.2f V",volt/1e6);
	bd->gauge_data->capacity = capacity;
	strncpy(bd->gauge_data->status, s_status, sizeof(bd->gauge_data->status)-1);
	gtk_widget_queue_draw(bd->gauge);
	char devinfo[192];
	snprintf(devinfo,sizeof(devinfo),"%s %s · %s",
		 strcmp(s_manufacturer,"N/A")?s_manufacturer:"",
		 strcmp(s_model,"N/A")?s_model:"",
		 strcmp(s_tech,"N/A")?s_tech:"Li-ion");
	gtk_label_set_text(GTK_LABEL(bd->devlbl), devinfo);
	card_set_value(bd->card_health,  s_health);
	card_set_value(bd->card_power,   s_power);
	card_set_value(bd->card_time,    s_time);
	card_set_value(bd->card_cycles,  s_cycles);
	card_set_value(bd->card_voltage, s_volt_fmt);
	card_set_value(bd->card_temp,    s_temp_fmt);
	char gov[32]="N/A", epp[32]="N/A";
	FILE *gf=fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor","r");
	if(gf){fgets(gov,sizeof(gov),gf);fclose(gf);char*n=strchr(gov,'\n');if(n)*n='\0';}
	FILE *ef=fopen("/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference","r");
	if(ef){fgets(epp,sizeof(epp),ef);fclose(ef);char*n=strchr(epp,'\n');if(n)*n='\0';}
	if (bd->gov_performance)
		gtk_widget_set_sensitive(bd->gov_performance, strcmp(gov,"performance")!=0);
	if (bd->gov_balanced)
		gtk_widget_set_sensitive(bd->gov_balanced,
					 !(strcmp(gov,"powersave")==0 && strcmp(epp,"balance_performance")==0));
	if (bd->gov_powersave)
		gtk_widget_set_sensitive(bd->gov_powersave,
					 !(strcmp(gov,"powersave")==0 &&
					   (strcmp(epp,"power")==0 || strcmp(epp,"balance_power")==0)));
}

static gboolean bat_timer_cb(gpointer ud) {
	BatData *bd = ud;
	if (!bd || bd->destroyed) return G_SOURCE_REMOVE;
	bat_read_and_update(bd); return G_SOURCE_CONTINUE;
}
static void bat_page_destroyed(GtkWidget *w, gpointer ud) {
	BatData *bd = ud;
	bd->destroyed = TRUE;
	if (bd->timer_id) { g_source_remove(bd->timer_id); bd->timer_id = 0; }
	g_free(bd);
}
static void gov_set(GtkWidget *btn, gpointer ud) {
	const char *mode = ud, *gov, *epp;
	if      (strcmp(mode,"performance")==0) { gov="performance"; epp="performance"; }
	else if (strcmp(mode,"balanced")==0)    { gov="powersave";   epp="balance_performance"; }
	else                                    { gov="powersave";   epp="power"; }
	char cmd[512];
	//snprintf(cmd,sizeof(cmd),
	//    "for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do "
	//    "echo %s | pkexec tee \"$f\" >/dev/null 2>&1; done && "
	//    "for f in /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference; do "
	//    "echo %s | pkexec tee \"$f\" >/dev/null 2>&1; done",
	//    gov, epp);
	snprintf(cmd, sizeof(cmd),
		 "for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do "
		 "echo %s | tee \"$f\" >/dev/null 2>&1; done && "
		 "for f in /sys/devices/system/cpu/cpu*/cpufreq/energy_performance_preference; do "
		 "echo %s | tee \"$f\" >/dev/null 2>&1; done",
		 gov, epp);
	system(cmd);
}
static GtkWidget *make_gov_btn(const char *icon_name, const char *label_str,
			       const char *sub_str,   const char *mode_str) {
	GtkWidget *btn = gtk_button_new();
	gtk_widget_add_css_class(btn, "bat-gov-btn");
	GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_size_request(vb, 148, -1);
	gtk_widget_set_margin_start(vb,12); gtk_widget_set_margin_end(vb,12);
	gtk_widget_set_margin_top(vb,12);   gtk_widget_set_margin_bottom(vb,12);
	GtkWidget *ico = gtk_image_new_from_icon_name(icon_name);
	gtk_image_set_pixel_size(GTK_IMAGE(ico),28);
	gtk_widget_set_halign(ico,GTK_ALIGN_CENTER); gtk_box_append(GTK_BOX(vb),ico);
	GtkWidget *lbl = gtk_label_new(label_str);
	gtk_widget_add_css_class(lbl,"title-4");
	gtk_widget_set_halign(lbl,GTK_ALIGN_CENTER); gtk_box_append(GTK_BOX(vb),lbl);
	GtkWidget *sub = gtk_label_new(sub_str);
	gtk_widget_add_css_class(sub,"dim-label"); gtk_widget_add_css_class(sub,"caption");
	gtk_widget_set_halign(sub,GTK_ALIGN_CENTER);
	gtk_label_set_wrap(GTK_LABEL(sub),TRUE);
	gtk_label_set_justify(GTK_LABEL(sub),GTK_JUSTIFY_CENTER);
	gtk_box_append(GTK_BOX(vb),sub);
	gtk_button_set_child(GTK_BUTTON(btn),vb);
	g_signal_connect(btn,"clicked",G_CALLBACK(gov_set),(gpointer)mode_str);
	return btn;
}

GtkWidget *battery_settings(void) {
	BatData *bd = g_new0(BatData, 1);
	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand(root,TRUE); gtk_widget_set_vexpand(root,TRUE);
	gtk_box_append(GTK_BOX(root), make_page_header("battery-symbolic","Battery"));
	gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget *scr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr,TRUE);
	GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);
	gtk_widget_set_margin_start(content,24); gtk_widget_set_margin_end(content,24);
	gtk_widget_set_margin_top(content,20);   gtk_widget_set_margin_bottom(content,24);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr),content);
	gtk_box_append(GTK_BOX(root),scr);
	BatGaugeData *gd = g_new0(BatGaugeData,1);
	gd->capacity=0; strncpy(gd->status,"…",sizeof(gd->status)-1); bd->gauge_data=gd;
	GtkWidget *gauge = gtk_drawing_area_new();
	gtk_widget_set_size_request(gauge,220,220);
	gtk_widget_set_halign(gauge,GTK_ALIGN_CENTER);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(gauge),bat_gauge_draw,gd,g_free);
	bd->gauge=gauge; gtk_box_append(GTK_BOX(content),gauge);
	bd->devlbl = gtk_label_new("…");
	gtk_widget_add_css_class(bd->devlbl,"dim-label");
	gtk_widget_set_halign(bd->devlbl,GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(content),bd->devlbl);
	/* details */
	GtkWidget *box1_frame = make_section_box("Battery Details");
	GtkWidget *box1 = g_object_get_data(G_OBJECT(box1_frame),"inner-box");
	GtkWidget *grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(grid),0); gtk_grid_set_row_spacing(GTK_GRID(grid),0);
	gtk_widget_set_halign(grid,GTK_ALIGN_CENTER);
	gtk_widget_set_margin_start(grid,12); gtk_widget_set_margin_end(grid,12);
	gtk_widget_set_margin_top(grid,12);   gtk_widget_set_margin_bottom(grid,12);
	bd->card_health  = make_bat_card("Health",         "…");
	bd->card_power   = make_bat_card("Power Draw",     "…");
	bd->card_time    = make_bat_card("Time Remaining", "…");
	bd->card_cycles  = make_bat_card("Cycle Count",    "…");
	bd->card_voltage = make_bat_card("Voltage",        "…");
	bd->card_temp    = make_bat_card("Temperature",    "…");
	gtk_grid_attach(GTK_GRID(grid),bd->card_health, 0,0,1,1);
	gtk_grid_attach(GTK_GRID(grid),bd->card_power,  1,0,1,1);
	gtk_grid_attach(GTK_GRID(grid),bd->card_time,   2,0,1,1);
	gtk_grid_attach(GTK_GRID(grid),bd->card_cycles, 0,1,1,1);
	gtk_grid_attach(GTK_GRID(grid),bd->card_voltage,1,1,1,1);
	gtk_grid_attach(GTK_GRID(grid),bd->card_temp,   2,1,1,1);
	gtk_box_append(GTK_BOX(box1),grid);
	gtk_box_append(GTK_BOX(content),box1_frame);
	/* power mode */
	GtkWidget *box2_frame = make_section_box("Power Mode");
	GtkWidget *box2 = g_object_get_data(G_OBJECT(box2_frame),"inner-box");
	GtkWidget *gov_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
	gtk_widget_set_halign(gov_row,GTK_ALIGN_CENTER);
	gtk_widget_set_margin_start(gov_row,16); gtk_widget_set_margin_end(gov_row,16);
	gtk_widget_set_margin_top(gov_row,14);   gtk_widget_set_margin_bottom(gov_row,14);
	bd->gov_performance = make_gov_btn("starred-symbolic","Performance","Max CPU frequency","performance");
	bd->gov_balanced    = make_gov_btn("battery-good-symbolic","Balanced","Speed and efficiency","balanced");
	bd->gov_powersave   = make_gov_btn("battery-low-symbolic","Power Saver","Extend battery life","powersave");
	gtk_box_append(GTK_BOX(gov_row),bd->gov_performance);
	gtk_box_append(GTK_BOX(gov_row),bd->gov_balanced);
	gtk_box_append(GTK_BOX(gov_row),bd->gov_powersave);
	gtk_box_append(GTK_BOX(box2),gov_row);
	gtk_box_append(GTK_BOX(content),box2_frame);
	bat_read_and_update(bd);
	bd->timer_id = g_timeout_add_seconds(5,bat_timer_cb,bd);
	g_signal_connect(root,"destroy",G_CALLBACK(bat_page_destroyed),bd);
	return root;
}

/* ================================================================== */
/* Displays                                                             */
/* ================================================================== */
typedef struct {
	char name[64], label[64];
	int  x, y, w, h;
	double refresh;
	gboolean primary, connected, active;
	char   **modes;
	int      n_modes, cur_mode_idx;
} DispMonitor;

typedef struct {
	GtkWidget   *root, *canvas, *detail_box, *mon_stack;
	GtkWidget  **tab_btns;
	DispMonitor *monitors;
	int          n_monitors, selected;
	gboolean     destroyed;
} DispData;

static void     disp_select_monitor(DispData *dd, int idx);
static void     disp_set_primary(GtkWidget *btn, gpointer ud);
static void     disp_apply(GtkWidget *btn, gpointer ud);
static void     disp_canvas_draw(GtkDrawingArea *da, cairo_t *cr, int w, int h, gpointer ud);
static void     disp_canvas_click(GtkGestureClick *gc, int n_press, double ex, double ey, gpointer ud);
static void     updates_apply_single(GtkWidget *btn, gpointer ud);

static void disp_free_monitors(DispMonitor *m, int n) {
	if (!m) return;
	for (int i=0;i<n;i++){for(int j=0;j<m[i].n_modes;j++) g_free(m[i].modes[j]); g_free(m[i].modes);}
	g_free(m);
}

static DispMonitor *disp_parse_xrandr(int *out_n) {
	char *raw = run_cmd_str("xrandr --query 2>/dev/null");
	char **lines = g_strsplit(raw, "\n", -1);
	g_free(raw);
	DispMonitor *mons = NULL; int n=0, cap=0, cur=-1;
	for (int i=0; lines[i]; i++) {
		const char *l = lines[i];
		if (!g_str_has_prefix(l," ") && !g_str_has_prefix(l,"\t") && strstr(l," connected")) {
			if (n>=cap){cap=cap?cap*2:8;mons=g_realloc(mons,cap*sizeof(DispMonitor));}
			cur=n++; DispMonitor *m=&mons[cur]; memset(m,0,sizeof(*m));
			char *sp=strchr(l,' '); size_t nlen=sp?(size_t)(sp-l):strlen(l);
			if (nlen>=sizeof(m->name)) nlen=sizeof(m->name)-1;
			memcpy(m->name,l,nlen);
			m->connected=TRUE; m->primary=strstr(l," primary")!=NULL;
			if      (strncmp(m->name,"eDP",3)==0)  snprintf(m->label,sizeof(m->label),"Built-in Display");
			else if (strncmp(m->name,"HDMI",4)==0) snprintf(m->label,sizeof(m->label),"HDMI Monitor");
			else if (strncmp(m->name,"DP",2)==0)   snprintf(m->label,sizeof(m->label),"DisplayPort Monitor");
			else                                   snprintf(m->label,sizeof(m->label),"%s",m->name);
			const char *geom=strstr(l," connected ");
			if (geom) {
				geom+=11; if (strncmp(geom,"primary ",8)==0) geom+=8;
				int W,H,X,Y;
				if (sscanf(geom,"%dx%d+%d+%d",&W,&H,&X,&Y)==4)
				{ m->w=W;m->h=H;m->x=X;m->y=Y;m->active=TRUE; }
			}
			continue;
		}
		if (!g_str_has_prefix(l," ") && !g_str_has_prefix(l,"\t") && strstr(l," disconnected"))
		{ cur=-1; continue; }
		if (cur>=0 && (l[0]==' '||l[0]=='\t')) {
			const char *p=l; while(*p==' '||*p=='\t') p++;
			if (!(*p>='0'&&*p<='9')) continue;
			int W,H; if (sscanf(p,"%dx%d",&W,&H)!=2) continue;
			const char *rp=strchr(p,' ');
			while (rp&&*rp) {
				while(*rp==' ') rp++;
				if (!*rp) break;
				double rr=0; int consumed=0;
				if (sscanf(rp,"%lf%n",&rr,&consumed)==1&&rr>0) {
					gboolean cf=FALSE;
					for (int k=0;k<consumed+2&&rp[k];k++) if(rp[k]=='*'){cf=TRUE;break;}
					DispMonitor *m=&mons[cur];
					char ms[32]; snprintf(ms,sizeof(ms),"%dx%d @ %.2f Hz",W,H,rr);
					m->modes=g_realloc(m->modes,(m->n_modes+1)*sizeof(char*));
					m->modes[m->n_modes]=g_strdup(ms);
					if (cf||(m->w==W&&m->h==H&&fabs(rr-m->refresh)<1.0))
					{ m->cur_mode_idx=m->n_modes; m->refresh=rr; }
					m->n_modes++;
					rp+=consumed;
					while(*rp&&(*rp=='*'||*rp=='+'||*rp==' ')) rp++;
				} else break;
			}
		}
	}
	g_strfreev(lines); *out_n=n; return mons;
}


/* ================================================================== */
/* DISPLAY CANVAS - FIXED VERSION                                      */
/* Key fixes:                                                          */
/*  1. Inactive monitors placed right of active ones in canvas layout  */
/*  2. All monitors use the SAME reference resolution for the canvas   */
/*     so they appear at equal visual size (proportional to resolution)*/
/*  3. Centering uses geometric center of each monitor rectangle       */
/* ================================================================== */

/* Helper: get effective width/height for canvas (placeholder if off) */
static void disp_eff_size(DispMonitor *m, int *ew, int *eh) {
	if (m->active) {
		*ew = m->w > 0 ? m->w : 1920;
		*eh = m->h > 0 ? m->h : 1080;
	} else if (m->n_modes > 0) {
		/* parse first available mode for inactive monitors */
		int W = 1920, H = 1080;
		sscanf(m->modes[0], "%dx%d", &W, &H);
		*ew = W; *eh = H;
	} else {
		/* disconnected/no modes: show as small placeholder so it's visible */
		*ew = 1280; *eh = 720;
	}
}

/* Place inactive monitors to the right of the active arrangement.
 * Called before drawing so canvas coordinates are always valid. */
static void disp_normalize_coords(DispData *dd) {
	/* Shift all active monitor coords so min_x=0, min_y=0.
	 * This prevents negative coords (from "Left of" / "Above")
	 * from being clipped by the canvas. */
	int min_x = INT_MAX, min_y = INT_MAX;
	for (int i = 0; i < dd->n_monitors; i++) {
		DispMonitor *m = &dd->monitors[i];
		if (!m->active) continue;
		if (m->x < min_x) min_x = m->x;
		if (m->y < min_y) min_y = m->y;
	}
	if (min_x == INT_MAX || min_y == INT_MAX) return;
	if (min_x == 0 && min_y == 0) return;
	for (int i = 0; i < dd->n_monitors; i++) {
		DispMonitor *m = &dd->monitors[i];
		if (!m->active) continue;
		m->x -= min_x;
		m->y -= min_y;
	}
}

static void disp_layout_inactive(DispData *dd) {
	/* Inactive monitors are only shown in tabs, NOT in the canvas.
	 * We normalize active monitor coordinates so they always start at 0,0. */
	disp_normalize_coords(dd);
}

static void disp_canvas_draw_v2(GtkDrawingArea *da, cairo_t *cr,
				int w, int h, gpointer ud) {
	DispData *dd = ud;
	if (!dd || dd->n_monitors == 0) return;

	/* Layout inactive monitors into canvas space first */
	disp_layout_inactive(dd);

	/* Compute bounding box of all visible monitors */
	int total_w = 0, total_h = 0;
	for (int i = 0; i < dd->n_monitors; i++) {
		DispMonitor *m = &dd->monitors[i];
		int ew, eh; disp_eff_size(m, &ew, &eh);
		if (ew == 0) continue;
		if (m->x + ew > total_w) total_w = m->x + ew;
		if (m->y + eh > total_h) total_h = m->y + eh;
	}
	if (total_w == 0 || total_h == 0) {
		/* All monitors inactive — show message */
		cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
		cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cr, 14.0);
		const char *msg = "All displays are off";
		cairo_text_extents_t te;
		cairo_text_extents(cr, msg, &te);
		cairo_move_to(cr, w/2.0 - te.width/2.0 - te.x_bearing,
			      h/2.0 - te.height/2.0 - te.y_bearing);
		cairo_show_text(cr, msg);
		return;
	}

	double pad = 28.0;
	double scale = MIN((w - pad * 2) / (double)total_w,
			   (h - pad * 2) / (double)total_h);
	double off_x = (w - total_w * scale) / 2.0;
	double off_y = (h - total_h * scale) / 2.0;

	static const double cols[][3] = {
		{ 0.21, 0.52, 0.89 },
		{ 0.18, 0.64, 0.47 },
		{ 0.75, 0.36, 0.20 },
		{ 0.56, 0.27, 0.68 },
	};

	for (int i = 0; i < dd->n_monitors; i++) {
		DispMonitor *m = &dd->monitors[i];
		int ew, eh; disp_eff_size(m, &ew, &eh);
		if (ew == 0) continue;

		double rx = off_x + m->x * scale;
		double ry = off_y + m->y * scale;
		double rw = ew * scale;
		double rh = eh * scale;
		double radius = 8.0;
		const double *c = cols[i % 4];
		gboolean sel = (i == dd->selected);

		if (!m->active) continue; /* inactive monitors shown only in tabs */

		/* Shadow */
		cairo_set_source_rgba(cr, 0, 0, 0, sel ? 0.18 : 0.07);
		cairo_rectangle(cr, rx + 3, ry + 3, rw, rh);
		cairo_fill(cr);

		/* Filled rounded rect */
		cairo_new_path(cr);
		cairo_arc(cr, rx+radius,    ry+radius,    radius, G_PI,    G_PI*1.5);
		cairo_arc(cr, rx+rw-radius, ry+radius,    radius, G_PI*1.5, 0);
		cairo_arc(cr, rx+rw-radius, ry+rh-radius, radius, 0,       G_PI*0.5);
		cairo_arc(cr, rx+radius,    ry+rh-radius, radius, G_PI*0.5, G_PI);
		cairo_close_path(cr);
		cairo_set_source_rgba(cr, c[0], c[1], c[2], sel ? 0.88 : 0.60);
		cairo_fill_preserve(cr);
		if (sel) {
			cairo_set_source_rgb(cr, c[0]*0.65, c[1]*0.65, c[2]*0.65);
			cairo_set_line_width(cr, 2.8);
		} else {
			cairo_set_source_rgba(cr, 0, 0, 0, 0.18);
			cairo_set_line_width(cr, 1.0);
		}
		cairo_stroke(cr);

		/* Number — centered */
		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
				       sel ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
		char num[4]; snprintf(num, sizeof(num), "%d", i + 1);
		cairo_set_font_size(cr, MIN(rw, rh) * 0.32);
		cairo_text_extents_t te;
		cairo_text_extents(cr, num, &te);
		cairo_move_to(cr,
			      rx + (rw - te.width)  / 2.0 - te.x_bearing,
			      ry + (rh - te.height) / 2.0 - te.y_bearing - rh * 0.06);
		cairo_show_text(cr, num);

		/* Label — centered below number */
		cairo_set_font_size(cr, MIN(rw, rh) * 0.10);
		cairo_text_extents(cr, m->label, &te);
		if (te.width < rw - 8) {
			cairo_move_to(cr,
				      rx + (rw - te.width) / 2.0 - te.x_bearing,
				      ry + (rh - te.height) / 2.0 - te.y_bearing + rh * 0.18);
			cairo_show_text(cr, m->label);
		}

		/* Primary badge */
		if (m->primary) {
			const char *badge = "Primary";
			cairo_set_font_size(cr, MIN(rw, rh) * 0.09);
			cairo_text_extents(cr, badge, &te);
			double bx = rx + (rw - te.width) / 2.0 - te.x_bearing - 6;
			double by = ry + 8;
			double bw = te.width + 12, bh = te.height + 6;
			cairo_set_source_rgba(cr, 1, 1, 1, 0.28);
			cairo_rectangle(cr, bx, by, bw, bh);
			cairo_fill(cr);
			cairo_set_source_rgb(cr, 1, 1, 1);
			cairo_move_to(cr, bx + 6, by + bh - 3 - te.y_bearing - te.height);
			cairo_show_text(cr, badge);
		}
	}
}

static void disp_canvas_click_v2(GtkGestureClick *gc, int n_press,
				 double ex, double ey, gpointer ud) {
	DispData *dd = ud;
	if (!dd || dd->n_monitors == 0) return;

	disp_layout_inactive(dd);

	int cw = gtk_widget_get_width(dd->canvas);
	int ch = gtk_widget_get_height(dd->canvas);
	int total_w = 0, total_h = 0;
	for (int i = 0; i < dd->n_monitors; i++) {
		DispMonitor *m = &dd->monitors[i];
		int ew, eh; disp_eff_size(m, &ew, &eh);
		if (ew == 0) continue;
		if (m->x + ew > total_w) total_w = m->x + ew;
		if (m->y + eh > total_h) total_h = m->y + eh;
	}
	if (total_w == 0 || total_h == 0) return;

	double pad   = 28.0;
	double scale = MIN((cw - pad * 2) / (double)total_w,
			   (ch - pad * 2) / (double)total_h);
	double off_x = (cw - total_w * scale) / 2.0;
	double off_y = (ch - total_h * scale) / 2.0;

	for (int i = 0; i < dd->n_monitors; i++) {
		DispMonitor *m = &dd->monitors[i];
		int ew, eh; disp_eff_size(m, &ew, &eh);
		if (ew == 0) continue;
		double rx = off_x + m->x * scale;
		double ry = off_y + m->y * scale;
		double rw = ew * scale;
		double rh = eh * scale;
		if (ex >= rx && ex <= rx + rw && ey >= ry && ey <= ry + rh) {
			disp_select_monitor(dd, i);
			return;
		}
	}
}

/* Fixed position change: centers new monitor relative to reference */

/* ------------------------------------------------------------------ */
/* structs for monitor panel callbacks                                  */
/* ------------------------------------------------------------------ */
typedef struct { DispData *dd; int idx; gboolean initialized; } DispPosData;

typedef struct {
	DispData  *dd;
	int        idx;
	GtkWidget *settings_box;  /* the controls box to enable/disable */
} DispEnableData;

static void disp_enable_toggled(GtkSwitch *sw, GParamSpec *ps, gpointer ud) {
	DispEnableData *ed = ud;
	DispData       *dd = ed->dd;
	DispMonitor    *m  = &dd->monitors[ed->idx];
	gboolean enable    = gtk_switch_get_active(sw);
	m->active = enable;

	if (enable && m->n_modes > 0 && m->w == 0) {
		int W = 0, H = 0;
		sscanf(m->modes[0], "%dx%d", &W, &H);
		m->w = W; m->h = H;
		/* place to the right of the rightmost active monitor */
		int rx = 0;
		for (int i = 0; i < dd->n_monitors; i++) {
			if (i == ed->idx || !dd->monitors[i].active) continue;
			int ew, eh; disp_eff_size(&dd->monitors[i], &ew, &eh);
			if (dd->monitors[i].x + ew > rx) rx = dd->monitors[i].x + ew;
		}
		m->x = rx; m->y = 0;
	}
	gtk_widget_set_sensitive(ed->settings_box, enable);
	gtk_widget_queue_draw(dd->canvas);
}

/* ------------------------------------------------------------------ */
/* apply xrandr changes for selected monitor                           */
/* ------------------------------------------------------------------ */
typedef struct { DispData *dd; int idx; } DispApplyData;

static gpointer disp_apply_thread(gpointer ud) {
	DispApplyData *ad = ud;
	DispData      *dd = ad->dd;
	int            idx = ad->idx;
	g_free(ad);
	if (idx < 0 || idx >= dd->n_monitors) return NULL;
	DispMonitor *m = &dd->monitors[idx];
	GString *cmd = g_string_new("xrandr");
	g_string_append_printf(cmd, " --output %s", m->name);
	if (m->active) {
		int W = m->w, H = m->h; double RR = m->refresh;
		if (m->n_modes > 0 && m->cur_mode_idx >= 0 && m->cur_mode_idx < m->n_modes)
			sscanf(m->modes[m->cur_mode_idx], "%dx%d @ %lf Hz", &W, &H, &RR);
		g_string_append_printf(cmd, " --mode %dx%d --rate %.2f --pos %dx%d", W, H, RR, m->x, m->y);
		if (m->primary) g_string_append(cmd, " --primary");
	} else {
		g_string_append(cmd, " --off");
	}
	system(cmd->str);
	g_string_free(cmd, TRUE);
	return NULL;
}

static void disp_apply(GtkWidget *btn, gpointer ud) {
	DispData *dd = ud;
	DispApplyData *ad = g_new0(DispApplyData, 1);
	ad->dd = dd; ad->idx = dd->selected;
	g_thread_unref(g_thread_new("xrandr", disp_apply_thread, ad));
}

/* ------------------------------------------------------------------ */
/* set primary monitor                                                  */
/* ------------------------------------------------------------------ */
static void disp_set_primary(GtkWidget *btn, gpointer ud) {
	DispData *dd = ud;
	for (int i = 0; i < dd->n_monitors; i++) dd->monitors[i].primary = FALSE;
	dd->monitors[dd->selected].primary = TRUE;
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "xrandr --output %s --primary",
		 dd->monitors[dd->selected].name);
	system(cmd);
	gtk_widget_queue_draw(dd->canvas);
	gtk_widget_set_sensitive(btn, FALSE);
}

/* ------------------------------------------------------------------ */
/* canvas draw/click wrappers                                           */
/* ------------------------------------------------------------------ */
static void disp_canvas_draw(GtkDrawingArea *da, cairo_t *cr, int w, int h, gpointer ud) {
	disp_canvas_draw_v2(da, cr, w, h, ud);
}
static void disp_canvas_click(GtkGestureClick *gc, int n_press, double ex, double ey, gpointer ud) {
	disp_canvas_click_v2(gc, n_press, ex, ey, ud);
}


static void disp_position_changed_v2(GtkDropDown *dd_w, GParamSpec *ps, gpointer ud) {
	DispPosData *pd = ud;
	if (!pd->initialized) return; /* not yet ready */
	DispData    *dd = pd->dd;
	DispMonitor *m  = &dd->monitors[pd->idx];
	if (!m->active) return;

	GObject *item = g_list_model_get_item(
					      gtk_drop_down_get_model(dd_w),
					      gtk_drop_down_get_selected(dd_w));
	if (!item) return;
	const char *txt = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
	g_object_unref(item);

	char dir[16] = "", tgt[64] = "";
	if      (sscanf(txt, "Right of %63s", tgt) == 1) strcpy(dir, "right");
	else if (sscanf(txt, "Left of %63s",  tgt) == 1) strcpy(dir, "left");
	else if (sscanf(txt, "Above %63s",    tgt) == 1) strcpy(dir, "above");
	else if (sscanf(txt, "Below %63s",    tgt) == 1) strcpy(dir, "below");
	else return;

	DispMonitor *ref = NULL;
	for (int i = 0; i < dd->n_monitors; i++) {
		if (i == pd->idx) continue;
		if (strcmp(dd->monitors[i].name, tgt) == 0) { ref = &dd->monitors[i]; break; }
	}
	if (!ref || !ref->active) return;

	int mw, mh; disp_eff_size(m,   &mw, &mh);
	int rw, rh; disp_eff_size(ref, &rw, &rh);

	if (strcmp(dir, "right") == 0) {
		m->x = ref->x + rw;
		m->y = ref->y + (rh - mh) / 2;
	} else if (strcmp(dir, "left") == 0) {
		m->x = ref->x - mw;
		m->y = ref->y + (rh - mh) / 2;
	} else if (strcmp(dir, "above") == 0) {
		/* Centre both monitors: align their horizontal midpoints */
		int combined_center = ref->x + rw / 2;
		m->x   = combined_center - mw / 2;
		ref->x = combined_center - rw / 2;
		m->y   = ref->y - mh;
	} else {  /* below */
		int combined_center = ref->x + rw / 2;
		m->x   = combined_center - mw / 2;
		ref->x = combined_center - rw / 2;
		m->y   = ref->y + rh;
	}
	/* Normalize so no negative coords appear in canvas */
	disp_normalize_coords(dd);
	gtk_widget_queue_draw(dd->canvas);
}

/* ------------------------------------------------------------------ */
/* build detail panel for one monitor                                   */
/* ------------------------------------------------------------------ */
static GtkWidget *disp_build_monitor_panel(DispData *dd, int idx);

static GtkWidget *disp_build_monitor_panel(DispData *dd, int idx) {
	DispMonitor *m = &dd->monitors[idx];

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
	gtk_widget_set_margin_start(root, 20); gtk_widget_set_margin_end(root, 20);
	gtk_widget_set_margin_top(root, 16);   gtk_widget_set_margin_bottom(root, 20);

	/* header: icon + label + enable switch */
	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	GtkWidget *ico = gtk_image_new_from_icon_name(
						      strncmp(m->name, "eDP", 3) == 0 ? "computer-symbolic" : "video-display-symbolic");
	gtk_image_set_pixel_size(GTK_IMAGE(ico), 20);
	gtk_box_append(GTK_BOX(hbox), ico);
	GtkWidget *nl = gtk_label_new(m->label);
	gtk_widget_add_css_class(nl, "title-3");
	gtk_widget_set_halign(nl, GTK_ALIGN_START);
	gtk_widget_set_hexpand(nl, TRUE);
	gtk_box_append(GTK_BOX(hbox), nl);

	/* Enable / Disable switch */
	GtkWidget *en_sw = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(en_sw), m->active);
	gtk_widget_set_valign(en_sw, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(hbox), en_sw);
	gtk_box_append(GTK_BOX(root), hbox);

	/* settings_box is disabled when monitor is off */
	GtkWidget *settings_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
	gtk_widget_set_sensitive(settings_box, m->active);

	/* ---- Position box (only when other active monitors exist) ---- */
	int n_active_other = 0;
	for (int i = 0; i < dd->n_monitors; i++)
		if (i != idx && dd->monitors[i].active) n_active_other++;

	if (n_active_other > 0) {
		GtkWidget *pos_frame = make_bat_box("Position");
		GtkWidget *pos_box   = g_object_get_data(G_OBJECT(pos_frame), "inner-box");

		GtkWidget *pos_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
		gtk_widget_set_margin_start(pos_row, 14); gtk_widget_set_margin_end(pos_row, 14);
		gtk_widget_set_margin_top(pos_row, 10);   gtk_widget_set_margin_bottom(pos_row, 10);

		GtkWidget *pos_lbl = gtk_label_new("Placement");
		gtk_widget_add_css_class(pos_lbl, "dim-label");
		gtk_widget_set_hexpand(pos_lbl, TRUE);
		gtk_widget_set_halign(pos_lbl, GTK_ALIGN_START);
		gtk_widget_set_valign(pos_lbl, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(pos_row), pos_lbl);

		GtkStringList *pos_sl = gtk_string_list_new(NULL);
		const char *dirs[] = { "Right of", "Left of", "Above", "Below", NULL };
		for (int i = 0; i < dd->n_monitors; i++) {
			if (i == idx || !dd->monitors[i].active) continue;
			for (int d = 0; dirs[d]; d++) {
				char entry[128];
				snprintf(entry, sizeof(entry), "%s %s", dirs[d], dd->monitors[i].name);
				gtk_string_list_append(pos_sl, entry);
			}
		}

		GtkWidget *pos_dd = gtk_drop_down_new(G_LIST_MODEL(pos_sl), NULL);
		gtk_widget_set_size_request(pos_dd, 200, -1);
		gtk_box_append(GTK_BOX(pos_row), pos_dd);

		/* Detect current relationship to pick correct default entry */
		guint default_pos = 0;
		{
			guint entry_idx = 0;
			const char *dirs2[] = { "Right of", "Left of", "Above", "Below", NULL };
			gboolean found = FALSE;
			for (int ii = 0; ii < dd->n_monitors && !found; ii++) {
				if (ii == idx || !dd->monitors[ii].active) continue;
				DispMonitor *ref2 = &dd->monitors[ii];
				int mw2, mh2; disp_eff_size(m, &mw2, &mh2);
				int rw2, rh2; disp_eff_size(ref2, &rw2, &rh2);
				/* determine relationship: which side of ref is m on? */
				int dir_guess = 0; /* 0=right,1=left,2=above,3=below */
				if (m->x >= ref2->x + rw2 - 10)       dir_guess = 0;
				else if (m->x + mw2 <= ref2->x + 10)  dir_guess = 1;
				else if (m->y + mh2 <= ref2->y + 10)  dir_guess = 2;
				else                                    dir_guess = 3;
				for (int d = 0; dirs2[d]; d++) {
					if (d == dir_guess) { default_pos = entry_idx; found = TRUE; break; }
					entry_idx++;
				}
				if (!found) entry_idx += 4;
			}
		}
		gtk_drop_down_set_selected(GTK_DROP_DOWN(pos_dd), default_pos);

		DispPosData *pd = g_new0(DispPosData, 1);
		pd->dd = dd; pd->idx = idx;
		pd->initialized = TRUE; /* don't skip — set AFTER selected so first real change works */
		g_signal_connect_data(pos_dd, "notify::selected",
				      G_CALLBACK(disp_position_changed_v2), pd, (GClosureNotify)g_free, 0);

		gtk_box_append(GTK_BOX(pos_box), pos_row);
		gtk_box_append(GTK_BOX(settings_box), pos_frame);
	}

	/* ---- Display Info ---- */
	GtkWidget *inf_frame = make_bat_box("Display Info");
	GtkWidget *ib = g_object_get_data(G_OBJECT(inf_frame), "inner-box");

#define IROW(lbl, val) do { \
	GtkWidget *r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); \
	gtk_widget_set_margin_start(r, 14); gtk_widget_set_margin_end(r, 14); \
	gtk_widget_set_margin_top(r, 8);    gtk_widget_set_margin_bottom(r, 8); \
	GtkWidget *_l = gtk_label_new(lbl); \
	gtk_widget_add_css_class(_l, "dim-label"); \
	gtk_widget_set_halign(_l, GTK_ALIGN_START); gtk_widget_set_hexpand(_l, TRUE); \
	gtk_box_append(GTK_BOX(r), _l); \
	GtkWidget *_v = gtk_label_new(val); \
	gtk_widget_set_halign(_v, GTK_ALIGN_END); \
	gtk_box_append(GTK_BOX(r), _v); \
	gtk_box_append(GTK_BOX(ib), r); \
	gtk_box_append(GTK_BOX(ib), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL)); \
} while(0)

	char res[32], rr_str[32], pos_str[32];
	snprintf(res,     sizeof(res),     "%d \xc3\x97 %d", m->w, m->h);
	snprintf(rr_str,  sizeof(rr_str),  "%.2f Hz", m->refresh);
	snprintf(pos_str, sizeof(pos_str), "%d, %d",  m->x, m->y);

	IROW("Output",  m->name);
	IROW("Status",  m->active ? "Active" : (m->connected ? "Connected" : "Disconnected"));
	if (m->active) {
		IROW("Resolution", res);
		IROW("Refresh",    rr_str);
		IROW("Position",   pos_str);
	}
IROW("Primary", m->primary ? "Yes" : "No");
#undef IROW
gtk_box_append(GTK_BOX(settings_box), inf_frame);

/* ---- Resolution + Refresh dropdown ---- */
if (m->n_modes > 0) {
	GtkWidget *ctrl_frame = make_bat_box("Settings");
	GtkWidget *cb = g_object_get_data(G_OBJECT(ctrl_frame), "inner-box");

	GtkWidget *rrow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_margin_start(rrow, 14); gtk_widget_set_margin_end(rrow, 14);
	gtk_widget_set_margin_top(rrow, 10);   gtk_widget_set_margin_bottom(rrow, 10);
	GtkWidget *rl = gtk_label_new("Resolution & Refresh");
	gtk_widget_add_css_class(rl, "dim-label");
	gtk_widget_set_halign(rl, GTK_ALIGN_START); gtk_widget_set_hexpand(rl, TRUE);
	gtk_box_append(GTK_BOX(rrow), rl);
	GtkStringList *sl = gtk_string_list_new(NULL);
	for (int j = 0; j < m->n_modes; j++)
		gtk_string_list_append(sl, m->modes[j]);
	GtkWidget *dd_w = gtk_drop_down_new(G_LIST_MODEL(sl), NULL);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(dd_w),
				   m->cur_mode_idx >= 0 ? m->cur_mode_idx : 0);
	gtk_widget_set_size_request(dd_w, 220, -1);
	g_object_set_data(G_OBJECT(dd_w), "disp-data", dd);
	g_object_set_data(G_OBJECT(dd_w), "mon-idx",   GINT_TO_POINTER(idx));
	gtk_box_append(GTK_BOX(rrow), dd_w);
	gtk_box_append(GTK_BOX(cb), rrow);
	gtk_box_append(GTK_BOX(cb), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	/* primary button */
	GtkWidget *prow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_margin_start(prow, 14); gtk_widget_set_margin_end(prow, 14);
	gtk_widget_set_margin_top(prow, 10);   gtk_widget_set_margin_bottom(prow, 10);
	GtkWidget *pl = gtk_label_new("Set as Primary");
	gtk_widget_add_css_class(pl, "dim-label");
	gtk_widget_set_halign(pl, GTK_ALIGN_START); gtk_widget_set_hexpand(pl, TRUE);
	gtk_box_append(GTK_BOX(prow), pl);
	GtkWidget *pb = gtk_button_new_with_label(m->primary ? "Primary" : "Set Primary");
	if (m->primary) {
		gtk_widget_add_css_class(pb, "suggested-action");
		gtk_widget_set_sensitive(pb, FALSE);
	} else {
		gtk_widget_add_css_class(pb, "flat");
	}
	g_signal_connect(pb, "clicked", G_CALLBACK(disp_set_primary), dd);
	gtk_box_append(GTK_BOX(prow), pb);
	gtk_box_append(GTK_BOX(cb), prow);
	gtk_box_append(GTK_BOX(settings_box), ctrl_frame);
}

/* apply button */
GtkWidget *apply = gtk_button_new_with_label("Apply Changes");
gtk_widget_add_css_class(apply, "suggested-action");
gtk_widget_add_css_class(apply, "pill");
gtk_widget_set_halign(apply, GTK_ALIGN_CENTER);
g_signal_connect(apply, "clicked", G_CALLBACK(disp_apply), dd);
gtk_box_append(GTK_BOX(settings_box), apply);

gtk_box_append(GTK_BOX(root), settings_box);

/* wire enable switch now that settings_box is built */
DispEnableData *ed = g_new0(DispEnableData, 1);
ed->dd = dd; ed->idx = idx; ed->settings_box = settings_box;
g_signal_connect_data(en_sw, "notify::active",
		      G_CALLBACK(disp_enable_toggled), ed, (GClosureNotify)g_free, 0);

return root;
}

static void disp_select_monitor(DispData *dd, int idx) {
	if (!dd||idx<0||idx>=dd->n_monitors) return;
	dd->selected=idx;
	for (int i=0;i<dd->n_monitors;i++) {
		GtkWidget *b=dd->tab_btns[i]; if(!b) continue;
		if(i==idx){
			gtk_widget_add_css_class(b,"suggested-action");
			gtk_widget_remove_css_class(b,"flat");
			gtk_widget_set_opacity(b, 1.0);
		} else {
			gtk_widget_remove_css_class(b,"suggested-action");
			gtk_widget_add_css_class(b,"flat");
			/* restore dim for inactive monitors */
			gtk_widget_set_opacity(b, dd->monitors[i].active ? 1.0 : 0.55);
		}
	}
	gtk_widget_queue_draw(dd->canvas);
	gtk_stack_set_visible_child_name(GTK_STACK(dd->mon_stack),dd->monitors[idx].name);
}
static void disp_tab_clicked(GtkWidget *btn, gpointer ud) {
	DispData *dd=g_object_get_data(G_OBJECT(btn),"disp-data");
	int idx=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn),"mon-idx"));
	disp_select_monitor(dd,idx);
}
static void disp_page_destroyed(GtkWidget *w, gpointer ud) {
	DispData *dd=ud; dd->destroyed=TRUE;
	disp_free_monitors(dd->monitors,dd->n_monitors);
	g_free(dd->tab_btns); g_free(dd);
}
static void disp_paned_map_cb(GtkWidget *paned, gpointer unused) {
	gtk_paned_set_position(GTK_PANED(paned),340);
}

GtkWidget *displays_settings(void) {
	DispData *dd=g_new0(DispData,1); dd->selected=0;
	dd->monitors=disp_parse_xrandr(&dd->n_monitors);
	GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	gtk_widget_set_hexpand(root,TRUE); gtk_widget_set_vexpand(root,TRUE); dd->root=root;
	gtk_box_append(GTK_BOX(root),make_page_header("video-display-symbolic","Displays"));
	gtk_box_append(GTK_BOX(root),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget *paned=gtk_paned_new(GTK_ORIENTATION_VERTICAL);
	gtk_widget_set_vexpand(paned,TRUE);
	GtkWidget *cw=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	gtk_widget_add_css_class(cw,"disp-canvas-wrap");
	GtkWidget *al=gtk_label_new("Arrangement");
	gtk_widget_add_css_class(al,"title-4"); gtk_widget_set_halign(al,GTK_ALIGN_START);
	gtk_widget_set_margin_start(al,16); gtk_widget_set_margin_top(al,12); gtk_widget_set_margin_bottom(al,8);
	gtk_box_append(GTK_BOX(cw),al);
	GtkWidget *canvas=gtk_drawing_area_new();
	gtk_widget_set_size_request(canvas,-1,280);
	gtk_widget_set_vexpand(canvas,TRUE); gtk_widget_set_hexpand(canvas,TRUE); dd->canvas=canvas;
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(canvas),disp_canvas_draw,dd,NULL);
	GtkGesture *click=gtk_gesture_click_new();
	g_signal_connect(click,"pressed",G_CALLBACK(disp_canvas_click),dd);
	gtk_widget_add_controller(canvas,GTK_EVENT_CONTROLLER(click));
	gtk_box_append(GTK_BOX(cw),canvas);
	GtkWidget *tab_row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
	gtk_widget_set_halign(tab_row,GTK_ALIGN_CENTER);
	gtk_widget_set_margin_top(tab_row,8); gtk_widget_set_margin_bottom(tab_row,12);
	dd->tab_btns=g_new0(GtkWidget*,dd->n_monitors);
	for (int i=0;i<dd->n_monitors;i++){
		DispMonitor *m=&dd->monitors[i];
		char tl[80];
		if (!m->active)
			snprintf(tl,sizeof(tl),"%d  %s  (Off)",i+1,m->label);
		else
			snprintf(tl,sizeof(tl),"%d  %s",i+1,m->label);
		GtkWidget *tb=gtk_button_new_with_label(tl);
		/* Active first monitor gets suggested-action, others flat;
		   inactive monitors always flat with reduced opacity hint */
		if (i==0 && m->active)
			gtk_widget_add_css_class(tb,"suggested-action");
		else
			gtk_widget_add_css_class(tb,"flat");
		gtk_widget_add_css_class(tb,"pill");
		if (!m->active) gtk_widget_set_opacity(tb, 0.55);
		g_object_set_data(G_OBJECT(tb),"disp-data",dd);
		g_object_set_data(G_OBJECT(tb),"mon-idx",GINT_TO_POINTER(i));
		g_signal_connect(tb,"clicked",G_CALLBACK(disp_tab_clicked),NULL);
		dd->tab_btns[i]=tb; gtk_box_append(GTK_BOX(tab_row),tb);
	}
	gtk_box_append(GTK_BOX(cw),tab_row);
	gtk_paned_set_start_child(GTK_PANED(paned),cw);
	GtkWidget *scr=gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr,TRUE);
	GtkWidget *ms=gtk_stack_new();
	gtk_stack_set_transition_type(GTK_STACK(ms),GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
	dd->mon_stack=ms;
	for (int i=0;i<dd->n_monitors;i++){
		GtkWidget *panel=disp_build_monitor_panel(dd,i);
		gtk_stack_add_named(GTK_STACK(ms),panel,dd->monitors[i].name);
	}
	if (dd->n_monitors>0)
		gtk_stack_set_visible_child_name(GTK_STACK(ms),dd->monitors[0].name);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr),ms);
	gtk_paned_set_end_child(GTK_PANED(paned),scr);
	gtk_paned_set_resize_start_child(GTK_PANED(paned),TRUE);
	gtk_paned_set_resize_end_child(GTK_PANED(paned),TRUE);
	gtk_paned_set_shrink_start_child(GTK_PANED(paned),FALSE);
	gtk_paned_set_shrink_end_child(GTK_PANED(paned),FALSE);
	g_signal_connect(paned,"map",G_CALLBACK(disp_paned_map_cb),NULL);
	gtk_box_append(GTK_BOX(root),paned);
	g_signal_connect(root,"destroy",G_CALLBACK(disp_page_destroyed),dd);
	return root;
}

/* ================================================================== */
/* Sound                                                                */
/* ================================================================== */
typedef struct {
	char   name[256], desc[256];
	uint32_t index;
	int    volume, muted;
	char   active_port[128];
	char **ports; int n_ports;
	int    is_default;
} SndDevice;

typedef struct {
	GtkWidget *root, *out_list, *in_list;
	guint timer_id; gboolean destroyed;
} SndData;

static SndDevice *snd_get_devices(const char *type, int *out_n)
{
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "pactl list %s 2>/dev/null", type);
	char *raw_orig = run_cmd_str(cmd);
	/* Prepend \n so the first entry is found by "\nSink #" / "\nSource #" */
	char *raw = g_strdup_printf("\n%s", raw_orig);
	g_free(raw_orig);

	char def_cmd[128];
	snprintf(def_cmd, sizeof(def_cmd), "%s",
		 strcmp(type, "sinks") == 0
		 ? "pactl get-default-sink 2>/dev/null"
		 : "pactl get-default-source 2>/dev/null");
	char *def_raw = run_cmd_str(def_cmd);
	g_strstrip(def_raw);

	const char *entry_hdr = strcmp(type, "sinks") == 0 ? "\nSink #" : "\nSource #";
	int n = 0;
	const char *p = raw;
	while ((p = strstr(p, entry_hdr))) { n++; p++; }
	if (n == 0) { g_free(raw); g_free(def_raw); *out_n = 0; return NULL; }

	SndDevice *devs = g_new0(SndDevice, n);
	p = raw;

	for (int i = 0; i < n; i++) {
		p = strstr(p, entry_hdr);
		if (!p) break;
		p++;

		const char *next = strstr(p + 1, entry_hdr);
		size_t block_len = next ? (size_t)(next - p) : strlen(p);
		char *block = g_strndup(p, block_len);

		SndDevice *d = &devs[i];

		/* Index */
		const char *idx_p = strchr(block, '#');
		if (idx_p) d->index = (uint32_t)atol(idx_p + 1);

		/* Name — "\n\tName: " = 8 chars */
		char *name_p = strstr(block, "\n\tName: ");
		if (name_p) {
			name_p += 8;
			char *end = strchr(name_p, '\n');
			size_t len = end ? (size_t)(end - name_p) : strlen(name_p);
			if (len >= sizeof(d->name)) len = sizeof(d->name) - 1;
			memcpy(d->name, name_p, len);
		}

		/* Description — "\n\tDescription: " = 15 chars */
		char *desc_p = strstr(block, "\n\tDescription: ");
		if (desc_p) {
			desc_p += 15;
			char *end = strchr(desc_p, '\n');
			size_t len = end ? (size_t)(end - desc_p) : strlen(desc_p);
			if (len >= sizeof(d->desc)) len = sizeof(d->desc) - 1;
			memcpy(d->desc, desc_p, len);
		}

		/* Volume */
		char *vol_p = strstr(block, "\n\tVolume:");
		if (vol_p) {
			char *pct = strstr(vol_p, "%");
			if (pct) {
				char *start = pct - 1;
				while (start > vol_p && *start >= '0' && *start <= '9') start--;
				d->volume = atoi(start + 1);
			}
		}

		/* Mute */
		char *mute_p = strstr(block, "\n\tMute: ");
		if (mute_p) d->muted = strncmp(mute_p + 8, "yes", 3) == 0;

		/* Active Port — "\n\tActive Port: " = 15 chars */
		char *port_p = strstr(block, "\n\tActive Port: ");
		if (port_p) {
			port_p += 15;
			char *end = strchr(port_p, '\n');
			size_t len = end ? (size_t)(end - port_p) : strlen(port_p);
			if (len >= sizeof(d->active_port)) len = sizeof(d->active_port) - 1;
			memcpy(d->active_port, port_p, len);
		}

		/* Ports list */
		char *ports_hdr = strstr(block, "\n\tPorts:\n");
		if (ports_hdr) {
			char *pp = ports_hdr + 9;
			while (*pp) {
				if (pp[0] != '\t' || pp[1] != '\t') break;
				char *colon = strchr(pp + 2, ':');
				char *nl    = strchr(pp + 2, '\n');
				if (!colon || (nl && colon > nl)) { pp = nl ? nl + 1 : pp + 2; continue; }
				size_t plen = (size_t)(colon - (pp + 2));
				char port_name[128] = "";
				if (plen >= sizeof(port_name)) plen = sizeof(port_name) - 1;
				memcpy(port_name, pp + 2, plen);
				d->ports = g_realloc(d->ports, (d->n_ports + 1) * sizeof(char *));
				d->ports[d->n_ports++] = g_strdup(port_name);
				pp = nl ? nl + 1 : pp + plen + 2;
			}
		}

		d->is_default = (d->name[0] && strcmp(d->name, def_raw) == 0);
		g_free(block);
	}

	g_free(raw);
	g_free(def_raw);
	*out_n = n;
	return devs;
}

//static SndDevice *snd_get_devices(const char *type, int *out_n)
//{
//    /* Single pactl call — no jq, no per-field subprocesses */
//    char cmd[128];
//    snprintf(cmd, sizeof(cmd), "pactl list %s 2>/dev/null", type);
//    char *raw = run_cmd_str(cmd);
//
//    char def_cmd[128];                          /* plain char array, not pointer array */
//    snprintf(def_cmd, sizeof(def_cmd), "%s",
//        strcmp(type, "sinks") == 0
//        ? "pactl get-default-sink 2>/dev/null"
//        : "pactl get-default-source 2>/dev/null");
//    char *def_raw = run_cmd_str(def_cmd);
//    g_strstrip(def_raw);
//
//    /* Count entries */
//    const char *entry_hdr = strcmp(type, "sinks") == 0 ? "\nSink #" : "\nSource #";
//    int n = 0;
//    const char *p = raw;
//    while ((p = strstr(p, entry_hdr))) { n++; p++; }
//    if (n == 0) { g_free(raw); g_free(def_raw); *out_n = 0; return NULL; }
//
//    SndDevice *devs = g_new0(SndDevice, n);
//    p = raw;
//    for (int i = 0; i < n; i++) {
//        SndDevice *d = &devs[i];
//
//        p = strstr(p, entry_hdr);
//        if (!p) break;
//        p++;
//
//        /* Find start of NEXT entry so we don't bleed into it */
//        const char *next = strstr(p + 1, entry_hdr);
//        size_t block_len = next ? (size_t)(next - p) : strlen(p);
//        char *block = g_strndup(p, block_len);
//
//        /* index — "Sink #42" or "Source #42" */
//        const char *idx_p = strchr(block, '#');
//        if (idx_p) d->index = (uint32_t)atol(idx_p + 1);
//
//        /* Name */
//        char *name_p = strstr(block, "\n\tName: ");
//        if (name_p) {
//            name_p += 8;
//            char *end = strchr(name_p, '\n');
//            size_t len = end ? (size_t)(end - name_p) : strlen(name_p);
//            if (len >= sizeof(d->name)) len = sizeof(d->name) - 1;
//            memcpy(d->name, name_p, len);
//        }
//
//	if (strcmp(type, "sources") == 0 && strstr(d->name, ".monitor")) {
//            g_free(block);
//            continue;
//        }
//
//        /* Description */
//        char *desc_p = strstr(block, "\n\tDescription: ");
//        if (desc_p) {
//            desc_p += 16;
//            char *end = strchr(desc_p, '\n');
//            size_t len = end ? (size_t)(end - desc_p) : strlen(desc_p);
//            if (len >= sizeof(d->desc)) len = sizeof(d->desc) - 1;
//            memcpy(d->desc, desc_p, len);
//        }
//
//        /* Volume — first percentage on the Volume: line */
//        char *vol_p = strstr(block, "\n\tVolume:");
//        if (vol_p) {
//            char *pct = strstr(vol_p, "%");
//            if (pct) {
//                char *start = pct - 1;
//                while (start > vol_p && *start >= '0' && *start <= '9') start--;
//                d->volume = atoi(start + 1);
//            }
//        }
//
//        /* Mute */
//        char *mute_p = strstr(block, "\n\tMute: ");
//        if (mute_p) d->muted = strncmp(mute_p + 8, "yes", 3) == 0;
//
//        /* Active Port */
//        char *port_p = strstr(block, "\n\tActive Port: ");
//        if (port_p) {
//            port_p += 15;
//            char *end = strchr(port_p, '\n');
//            size_t len = end ? (size_t)(end - port_p) : strlen(port_p);
//            if (len >= sizeof(d->active_port)) len = sizeof(d->active_port) - 1;
//            memcpy(d->active_port, port_p, len);
//        }
//
//        /* Ports list — lines under "Ports:" section, each "\t\tport-name:" */
//        char *ports_hdr = strstr(block, "\n\tPorts:\n");
//        if (ports_hdr) {
//            char *pp = ports_hdr + 9;
//            while (*pp) {
//                /* each port line starts with two tabs */
//                if (pp[0] != '\t' || pp[1] != '\t') break;
//                char *colon = strchr(pp + 2, ':');
//                char *nl    = strchr(pp + 2, '\n');
//                if (!colon || (nl && colon > nl)) { pp = nl ? nl + 1 : pp + 2; continue; }
//                size_t plen = (size_t)(colon - (pp + 2));
//                char port_name[128] = "";
//                if (plen >= sizeof(port_name)) plen = sizeof(port_name) - 1;
//                memcpy(port_name, pp + 2, plen);
//                d->ports = g_realloc(d->ports, (d->n_ports + 1) * sizeof(char *));
//                d->ports[d->n_ports++] = g_strdup(port_name);
//                pp = nl ? nl + 1 : pp + plen + 2;
//            }
//        }
//
//        d->is_default = (d->name[0] && strcmp(d->name, def_raw) == 0);
//        g_free(block);
//    }
//
//    g_free(raw);
//    g_free(def_raw);
//    *out_n = n;
//    return devs;
//}

static void snd_free_devices(SndDevice *devs, int n) {
	if (!devs) return;
	for(int i=0;i<n;i++){for(int j=0;j<devs[i].n_ports;j++)g_free(devs[i].ports[j]);g_free(devs[i].ports);}
	g_free(devs);
}

typedef struct { char name[256]; char type[16]; GtkWidget *mute_btn; SndData *sd; } SndVolData;
typedef struct { char name[256]; char type[16]; SndData *sd; } SndDefaultData;
typedef struct { char dev_name[256]; char type[16]; } SndPortData;

static void snd_vol_changed(GtkRange *range, gpointer ud) {
	SndVolData *vd=ud; int val=(int)gtk_range_get_value(range);
	char cmd[512]; snprintf(cmd,sizeof(cmd),"pactl set-%s-volume '%s' %d%% 2>/dev/null",vd->type,vd->name,val);
	system(cmd);
}
static void snd_mute_toggled(GtkToggleButton *btn, gpointer ud) {
	SndVolData *vd=ud; gboolean muted=gtk_toggle_button_get_active(btn);
	char cmd[512]; snprintf(cmd,sizeof(cmd),"pactl set-%s-mute '%s' %s 2>/dev/null",vd->type,vd->name,muted?"1":"0");
	system(cmd);
	gtk_button_set_label(GTK_BUTTON(btn),muted?"🔇 Muted":"🔊 Mute");
}
static void snd_set_default(GtkWidget *btn, gpointer ud) {
	SndDefaultData *dd=ud; char cmd[512];
	snprintf(cmd,sizeof(cmd),"pactl set-default-%s '%s' 2>/dev/null",dd->type,dd->name);
	system(cmd);
	gtk_widget_set_sensitive(btn,FALSE); gtk_button_set_label(GTK_BUTTON(btn),"Default");
	gtk_widget_add_css_class(btn,"suggested-action"); gtk_widget_remove_css_class(btn,"flat");
}
static void snd_port_changed(GtkDropDown *dd, GParamSpec *ps, gpointer ud) {
	SndPortData *pd=ud;
	GObject *item=g_list_model_get_item(gtk_drop_down_get_model(dd),gtk_drop_down_get_selected(dd));
	if(!item)return;
	const char *port=gtk_string_object_get_string(GTK_STRING_OBJECT(item)); g_object_unref(item);
	char cmd[512]; snprintf(cmd,sizeof(cmd),"pactl set-%s-port '%s' '%s' 2>/dev/null",pd->type,pd->dev_name,port);
	system(cmd);
}
static char *snd_format_vol(GtkScale *s, double v, gpointer ud) {
	return g_strdup_printf("%d%%",(int)v);
}

static GtkWidget *snd_build_device_row(SndDevice *d, const char *type) {
	GtkWidget *frame=gtk_frame_new(NULL);
	gtk_widget_add_css_class(frame,"bat-box");
	gtk_widget_set_margin_bottom(frame,8);
	GtkWidget *box=gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
	gtk_widget_set_margin_start(box,14); gtk_widget_set_margin_end(box,14);
	gtk_widget_set_margin_top(box,12);   gtk_widget_set_margin_bottom(box,12);
	gtk_frame_set_child(GTK_FRAME(frame),box);
	GtkWidget *hdr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);
	GtkWidget *ico=gtk_image_new_from_icon_name(strcmp(type,"sink")==0?"audio-speakers-symbolic":"audio-input-microphone-symbolic");
	gtk_image_set_pixel_size(GTK_IMAGE(ico),20); gtk_widget_set_valign(ico,GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(hdr),ico);
	GtkWidget *nb=gtk_box_new(GTK_ORIENTATION_VERTICAL,2); gtk_widget_set_hexpand(nb,TRUE);
	GtkWidget *dl=gtk_label_new(d->desc[0]?d->desc:d->name);
	gtk_widget_add_css_class(dl,"title-4"); gtk_widget_set_halign(dl,GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(nb),dl);
	GtkWidget *nl=gtk_label_new(d->name);
	gtk_widget_add_css_class(nl,"dim-label"); gtk_widget_add_css_class(nl,"caption");
	gtk_widget_set_halign(nl,GTK_ALIGN_START); gtk_box_append(GTK_BOX(nb),nl);
	gtk_box_append(GTK_BOX(hdr),nb);
	SndDefaultData *dd_data=g_new0(SndDefaultData,1);
	strncpy(dd_data->name,d->name,sizeof(dd_data->name)-1); strncpy(dd_data->type,type,sizeof(dd_data->type)-1);
	GtkWidget *def_btn=gtk_button_new_with_label(d->is_default?"Default":"Set Default");
	if(d->is_default){gtk_widget_add_css_class(def_btn,"suggested-action");gtk_widget_set_sensitive(def_btn,FALSE);}
	else gtk_widget_add_css_class(def_btn,"flat");
	gtk_widget_set_valign(def_btn,GTK_ALIGN_CENTER);
	g_signal_connect_data(def_btn,"clicked",G_CALLBACK(snd_set_default),dd_data,(GClosureNotify)g_free,0);
	gtk_box_append(GTK_BOX(hdr),def_btn);
	SndVolData *vd=g_new0(SndVolData,1);
	strncpy(vd->name,d->name,sizeof(vd->name)-1); strncpy(vd->type,type,sizeof(vd->type)-1);
	GtkWidget *mb=gtk_toggle_button_new_with_label(d->muted?"🔇 Muted":"🔊 Mute");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mb),d->muted);
	gtk_widget_set_valign(mb,GTK_ALIGN_CENTER); gtk_widget_add_css_class(mb,"flat");
	vd->mute_btn=mb;
	g_signal_connect_data(mb,"toggled",G_CALLBACK(snd_mute_toggled),vd,(GClosureNotify)g_free,0);
	gtk_box_append(GTK_BOX(hdr),mb); gtk_box_append(GTK_BOX(box),hdr);
	GtkWidget *vr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);
	GtkWidget *vl=gtk_label_new("Volume");
	gtk_widget_add_css_class(vl,"dim-label"); gtk_widget_set_size_request(vl,60,-1);
	gtk_widget_set_valign(vl,GTK_ALIGN_CENTER); gtk_box_append(GTK_BOX(vr),vl);
	GtkWidget *vs=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,0,100,1);
	gtk_range_set_value(GTK_RANGE(vs),d->volume);
	gtk_scale_set_draw_value(GTK_SCALE(vs),TRUE);
	gtk_scale_set_format_value_func(GTK_SCALE(vs),snd_format_vol,NULL,NULL);
	gtk_widget_set_hexpand(vs,TRUE); gtk_widget_set_valign(vs,GTK_ALIGN_CENTER);
	SndVolData *vd2=g_new0(SndVolData,1);
	strncpy(vd2->name,d->name,sizeof(vd2->name)-1); strncpy(vd2->type,type,sizeof(vd2->type)-1);
	g_signal_connect_data(vs,"value-changed",G_CALLBACK(snd_vol_changed),vd2,(GClosureNotify)g_free,0);
	gtk_box_append(GTK_BOX(vr),vs); gtk_box_append(GTK_BOX(box),vr);
	if (d->n_ports>1) {
		GtkWidget *pr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);
		GtkWidget *pl=gtk_label_new("Port");
		gtk_widget_add_css_class(pl,"dim-label"); gtk_widget_set_size_request(pl,60,-1);
		gtk_widget_set_valign(pl,GTK_ALIGN_CENTER); gtk_box_append(GTK_BOX(pr),pl);
		GtkStringList *sl=gtk_string_list_new(NULL); int ai=0;
		for(int j=0;j<d->n_ports;j++){gtk_string_list_append(sl,d->ports[j]);if(strcmp(d->ports[j],d->active_port)==0)ai=j;}
		GtkWidget *pd_w=gtk_drop_down_new(G_LIST_MODEL(sl),NULL);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(pd_w),ai); gtk_widget_set_hexpand(pd_w,TRUE);
		SndPortData *pd=g_new0(SndPortData,1);
		strncpy(pd->dev_name,d->name,sizeof(pd->dev_name)-1); strncpy(pd->type,type,sizeof(pd->type)-1);
		g_signal_connect_data(pd_w,"notify::selected",G_CALLBACK(snd_port_changed),pd,(GClosureNotify)g_free,0);
		gtk_box_append(GTK_BOX(pr),pd_w); gtk_box_append(GTK_BOX(box),pr);
	}
	return frame;
}

static void snd_rebuild_list(GtkWidget *box, const char *pa_type, const char *dev_type)
{
	/* Clear */
	GtkWidget *c;
	while ((c = gtk_widget_get_first_child(box)))
		gtk_box_remove(GTK_BOX(box), c);

	int n = 0;
	SndDevice *devs = snd_get_devices(pa_type, &n);
	for (int i = 0; i < n; i++)
		gtk_box_append(GTK_BOX(box), snd_build_device_row(&devs[i], dev_type));
	snd_free_devices(devs, n);
}

//static void snd_rebuild_list(GtkWidget *listbox, const char *pa_type, const char *dev_type) {
//    GtkWidget *c;
//    while ((c=gtk_widget_get_first_child(listbox))) gtk_list_box_remove(GTK_LIST_BOX(listbox),c);
//    int n=0; SndDevice *devs=snd_get_devices(pa_type,&n);
//    for(int i=0;i<n;i++) gtk_list_box_append(GTK_LIST_BOX(listbox),snd_build_device_row(&devs[i],dev_type));
//    snd_free_devices(devs,n);
//}

static gboolean snd_refresh_cb(gpointer ud) {
	SndData *sd=ud; if(!sd||sd->destroyed)return G_SOURCE_REMOVE;
	snd_rebuild_list(sd->out_list,"sinks","sink");
	snd_rebuild_list(sd->in_list,"sources","source");
	return G_SOURCE_CONTINUE;
}
static void snd_page_destroyed(GtkWidget *w, gpointer ud) {
	SndData *sd=ud; sd->destroyed=TRUE;
	if(sd->timer_id){g_source_remove(sd->timer_id);sd->timer_id=0;}
	g_free(sd);
}

GtkWidget *sound_settings(void)
{
	SndData *sd = g_new0(SndData, 1);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand(root, TRUE);
	gtk_widget_set_vexpand(root, TRUE);

	/* ── header ── */
	GtkWidget *hdr = make_page_header("audio-volume-high-symbolic", "Sound");
	GtkWidget *rb  = gtk_button_new_from_icon_name("view-refresh-symbolic");
	gtk_widget_add_css_class(rb, "flat");
	gtk_widget_set_valign(rb, GTK_ALIGN_CENTER);
	g_signal_connect_swapped(rb, "clicked", G_CALLBACK(snd_refresh_cb), sd);
	gtk_box_append(GTK_BOX(hdr), rb);
	gtk_box_append(GTK_BOX(root), hdr);
	gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	/* ── Output / Input tab stack ── */
	GtkWidget *stack = gtk_stack_new();
	gtk_stack_set_transition_type(GTK_STACK(stack),
				      GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);

	GtkWidget *sw = gtk_stack_switcher_new();
	gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(sw), GTK_STACK(stack));
	gtk_widget_set_halign      (sw, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_top  (sw, 10);
	gtk_widget_set_margin_bottom(sw, 4);
	gtk_box_append(GTK_BOX(root), sw);

	/* ── Output tab: plain scrolled GtkBox, no GtkListBox ── */
	GtkWidget *out_scr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(out_scr),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(out_scr, TRUE);

	GtkWidget *out_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_margin_start (out_box, 16);
	gtk_widget_set_margin_end   (out_box, 16);
	gtk_widget_set_margin_top   (out_box, 12);
	gtk_widget_set_margin_bottom(out_box, 12);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(out_scr), out_box);
	gtk_stack_add_titled(GTK_STACK(stack), out_scr, "output", "Output");
	sd->out_list = out_box;   /* GtkBox — snd_rebuild_list uses gtk_box_append */

	/* ── Input tab: same pattern ── */
	GtkWidget *in_scr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(in_scr),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(in_scr, TRUE);

	GtkWidget *in_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_margin_start (in_box, 16);
	gtk_widget_set_margin_end   (in_box, 16);
	gtk_widget_set_margin_top   (in_box, 12);
	gtk_widget_set_margin_bottom(in_box, 12);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(in_scr), in_box);
	gtk_stack_add_titled(GTK_STACK(stack), in_scr, "input", "Input");
	sd->in_list = in_box;

	gtk_widget_set_vexpand(stack, TRUE);
	gtk_box_append(GTK_BOX(root), stack);

	/* Populate */
	snd_rebuild_list(sd->out_list, "sinks",   "sink");
	snd_rebuild_list(sd->in_list,  "sources", "source");

	sd->timer_id = g_timeout_add_seconds(10, snd_refresh_cb, sd);
	g_signal_connect(root, "destroy", G_CALLBACK(snd_page_destroyed), sd);
	return root;
}

//GtkWidget *sound_settings(void) {
//    SndData *sd=g_new0(SndData,1);
//    GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
//    gtk_widget_set_hexpand(root,TRUE);
//    gtk_widget_set_vexpand(root,TRUE);
//
//    GtkWidget *hdr=make_page_header("audio-volume-high-symbolic","Sound");
//    GtkWidget *rb=gtk_button_new_from_icon_name("view-refresh-symbolic");
//    gtk_widget_add_css_class(rb,"flat"); gtk_widget_set_valign(rb,GTK_ALIGN_CENTER);
//    g_signal_connect_swapped(rb,"clicked",G_CALLBACK(snd_refresh_cb),sd);
//    gtk_box_append(GTK_BOX(hdr),rb);
//    gtk_box_append(GTK_BOX(root),hdr);
//    gtk_box_append(GTK_BOX(root),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
//    GtkWidget *inner=gtk_stack_new();
//    gtk_stack_set_transition_type(GTK_STACK(inner),GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
//    GtkWidget *switcher=gtk_stack_switcher_new();
//    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher),GTK_STACK(inner));
//    gtk_widget_set_halign(switcher,GTK_ALIGN_CENTER);
//    gtk_widget_set_margin_top(switcher,10); gtk_widget_set_margin_bottom(switcher,4);
//    gtk_box_append(GTK_BOX(root),switcher);
//    GtkWidget *os=gtk_scrolled_window_new();
//    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(os),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
//    gtk_widget_set_vexpand(os,TRUE);
//    sd->out_list=gtk_list_box_new();
//    gtk_list_box_set_selection_mode(GTK_LIST_BOX(sd->out_list),GTK_SELECTION_NONE);
//    gtk_widget_set_margin_start(sd->out_list,16); gtk_widget_set_margin_end(sd->out_list,16);
//    gtk_widget_set_margin_top(sd->out_list,12);   gtk_widget_set_margin_bottom(sd->out_list,12);
//    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(os),sd->out_list);
//    gtk_stack_add_titled(GTK_STACK(inner),os,"output","Output");
//    GtkWidget *is=gtk_scrolled_window_new();
//    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(is),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
//    gtk_widget_set_vexpand(is,TRUE);
//    sd->in_list=gtk_list_box_new();
//    gtk_list_box_set_selection_mode(GTK_LIST_BOX(sd->in_list),GTK_SELECTION_NONE);
//    gtk_widget_set_margin_start(sd->in_list,16); gtk_widget_set_margin_end(sd->in_list,16);
//    gtk_widget_set_margin_top(sd->in_list,12);   gtk_widget_set_margin_bottom(sd->in_list,12);
//    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(is),sd->in_list);
//    gtk_stack_add_titled(GTK_STACK(inner),is,"input","Input");
//    gtk_widget_set_vexpand(inner,TRUE);
//    gtk_box_append(GTK_BOX(root),inner);
//    snd_rebuild_list(sd->out_list,"sinks","sink");
//    snd_rebuild_list(sd->in_list,"sources","source");
//    sd->timer_id=g_timeout_add_seconds(10,snd_refresh_cb,sd);
//    g_signal_connect(root,"destroy",G_CALLBACK(snd_page_destroyed),sd);
//    return root;
//}

/* ================================================================== */
/* VPN                                                                  */
/* ================================================================== */

/* ================================================================== */
/* Wallpaper                                                            */
/* ================================================================== */
typedef struct {
	WPData    *wd;
	char       path[4096];
} WpThumbData;

	static void
wp_thumb_confirm_cb (GObject *src, GAsyncResult *res, gpointer ud)
{
	WpThumbData *td  = ud;
	int          btn = gtk_alert_dialog_choose_finish (GTK_ALERT_DIALOG (src), res, NULL);
	if (btn == 1) {
		set_wallpaper (td->path);
		GFile *gf = g_file_new_for_path (td->path);
		gtk_picture_set_file (GTK_PICTURE (td->wd->preview_picture), gf);
		g_object_unref (gf);
	}
	g_free (td);
}

	static void
wp_folder_thumb_cb (GtkWidget *btn, gpointer unused)
{
	WPData     *wd   = g_object_get_data (G_OBJECT (btn), "wd");
	const char *path = g_object_get_data (G_OBJECT (btn), "path");
	if (!path || !wd) return;

	WpThumbData *td = g_new0 (WpThumbData, 1);
	td->wd = wd;
	g_strlcpy (td->path, path, sizeof (td->path));

	GtkAlertDialog *dlg = gtk_alert_dialog_new (
						    "Set this image as your wallpaper?");
	gtk_alert_dialog_set_buttons (dlg,
				      (const char *[]){"Cancel", "Set Wallpaper", NULL});
	gtk_alert_dialog_set_default_button (dlg, 1);
	gtk_alert_dialog_set_cancel_button  (dlg, 0);
	gtk_alert_dialog_choose (dlg,
				 GTK_WINDOW (gtk_widget_get_root (btn)),
				 NULL, wp_thumb_confirm_cb, td);
	g_object_unref (dlg);
}

GtkWidget *wallpaper_settings(void) {
	WPData *wd = g_new0(WPData, 1);

	/* ── Root scroll container ── */
	GtkWidget *scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_hexpand(scroll, TRUE);
	gtk_widget_set_vexpand(scroll, TRUE);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_margin_start(root, 32);
	gtk_widget_set_margin_end(root, 32);
	gtk_widget_set_margin_top(root, 24);
	gtk_widget_set_margin_bottom(root, 32);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), root);

	/* ── Page title ── */
	GtkWidget *title = gtk_label_new("Wallpaper");
	gtk_widget_add_css_class(title, "title-1");
	gtk_widget_set_halign(title, GTK_ALIGN_START);
	gtk_widget_set_margin_bottom(title, 20);
	gtk_box_append(GTK_BOX(root), title);

	/* ══════════════════════════════════════════════════════════════
	   TOP CARD — current wallpaper preview + Browse button
	   ══════════════════════════════════════════════════════════════ */
	GtkWidget *card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(card, "wp-top-card");
	gtk_widget_set_margin_bottom(card, 28);

	char *wp = get_current_wallpaper();
	GtkWidget *pic;
	if (wp && g_file_test(wp, G_FILE_TEST_EXISTS)) {
		GFile *gf = g_file_new_for_path(wp);
		pic = gtk_picture_new_for_file(gf);
		g_object_unref(gf);
		free(wp);
	} else {
		pic = gtk_picture_new();
		if (wp) free(wp);
	}
	gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_COVER);
	gtk_widget_set_size_request(pic, -1, 260);
	gtk_widget_set_hexpand(pic, FALSE);
	gtk_widget_set_vexpand(pic, FALSE);
	gtk_widget_set_overflow(pic, GTK_OVERFLOW_HIDDEN);
	gtk_widget_add_css_class(pic, "wp-preview-img");
	wd->preview_picture = pic;
	gtk_box_append(GTK_BOX(card), pic);

	GtkWidget *info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_hexpand(info, TRUE);
	gtk_widget_set_valign(info, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_start(info, 20);
	gtk_widget_set_margin_end(info, 20);
	gtk_widget_set_margin_top(info, 14);
	gtk_widget_set_margin_bottom(info, 16);

	GtkWidget *lbl_cur = gtk_label_new("Current Wallpaper");
	gtk_widget_add_css_class(lbl_cur, "title-3");
	gtk_widget_set_halign(lbl_cur, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(info), lbl_cur);

	GtkWidget *lbl_sub = gtk_label_new("Choose from your Wallpapers folder below, or browse for any file.");
	gtk_widget_add_css_class(lbl_sub, "dim-label");
	gtk_widget_add_css_class(lbl_sub, "caption");
	gtk_label_set_wrap(GTK_LABEL(lbl_sub), TRUE);
	gtk_label_set_justify(GTK_LABEL(lbl_sub), GTK_JUSTIFY_CENTER);
	gtk_widget_set_halign(lbl_sub, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(info), lbl_sub);

	GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_vexpand(sp, TRUE);
	gtk_box_append(GTK_BOX(info), sp);

	GtkWidget *browse_btn = gtk_button_new_with_label("Browse\xe2\x80\xa6");
	gtk_widget_add_css_class(browse_btn, "suggested-action");
	gtk_widget_add_css_class(browse_btn, "pill");
	gtk_widget_set_halign(browse_btn, GTK_ALIGN_CENTER);
	g_signal_connect(browse_btn, "clicked", G_CALLBACK(open_picker_home), wd);
	gtk_box_append(GTK_BOX(info), browse_btn);

	gtk_box_append(GTK_BOX(card), info);
	gtk_box_append(GTK_BOX(root), card);

	/* ══════════════════════════════════════════════════════════════
	   WALLPAPERS FOLDER — single horizontal scrolling row
	   ══════════════════════════════════════════════════════════════ */
	//char *home = get_home_env();
	char wp_dir[512];
	//snprintf(wp_dir, sizeof(wp_dir), "%s/Wallpapers", home ? home : "/root");
	snprintf(wp_dir, sizeof(wp_dir), "/usr/share/mrrobotos/mrsettings/Wallpapers");
	//free(home);

	/* Section header */
	GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_hexpand(hdr, TRUE);
	gtk_widget_set_margin_bottom(hdr, 10);

	GtkWidget *sec_title = gtk_label_new("Wallpapers Folder");
	gtk_widget_add_css_class(sec_title, "title-4");
	gtk_widget_set_halign(sec_title, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(hdr), sec_title);
	gtk_box_append(GTK_BOX(root), hdr);

	/* Card containing the horizontal strip */
	GtkWidget *folder_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(folder_card, "wp-recents-card");
	gtk_widget_set_overflow(folder_card, GTK_OVERFLOW_HIDDEN);

	/* ── Horizontal scrolled window — one row, scrolls left/right ── */
	GtkWidget *hscroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(hscroll),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_widget_set_hexpand(hscroll, TRUE);
	gtk_widget_set_size_request(hscroll, -1, 190); /* card height = 190 + 28px margins = 218px */
	gtk_widget_set_vexpand(hscroll, FALSE);

	/* Plain HBox — never wraps, grows as wide as needed */
	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_margin_start(hbox, 14);
	gtk_widget_set_margin_end(hbox, 14);
	gtk_widget_set_margin_top(hbox, 14);
	gtk_widget_set_margin_bottom(hbox, 14);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(hscroll), hbox);

	/* ── Browse "+" card — always the first tile ── */
	GtkWidget *plus_vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_halign(plus_vb, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(plus_vb, GTK_ALIGN_CENTER);

	GtkWidget *plus_ico = gtk_image_new_from_icon_name("list-add-symbolic");
	gtk_image_set_pixel_size(GTK_IMAGE(plus_ico), 36);
	gtk_widget_set_halign(plus_ico, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(plus_vb), plus_ico);

	GtkWidget *plus_lbl = gtk_label_new("Browse\xe2\x80\xa6");
	gtk_widget_add_css_class(plus_lbl, "caption");
	gtk_widget_set_halign(plus_lbl, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(plus_vb), plus_lbl);

	GtkWidget *plus_btn = gtk_button_new();
	gtk_widget_add_css_class(plus_btn, "flat");
	gtk_widget_add_css_class(plus_btn, "wp-thumb-btn");
	gtk_widget_add_css_class(plus_btn, "wp-browse-card");
	gtk_widget_set_size_request(plus_btn, 300, 190);
	gtk_widget_set_hexpand(plus_btn, FALSE);
	gtk_widget_set_vexpand(plus_btn, FALSE);
	gtk_button_set_child(GTK_BUTTON(plus_btn), plus_vb);
	g_signal_connect(plus_btn, "clicked", G_CALLBACK(open_picker), wd);
	gtk_box_append(GTK_BOX(hbox), plus_btn);

	/* ── Scan ~/Wallpapers ── */
	if (!g_file_test(wp_dir, G_FILE_TEST_IS_DIR)) {
		/* folder missing — show Browse card alone with a hint label */
		//GtkWidget *hint = gtk_label_new("~/Wallpapers not found");
		GtkWidget *hint = gtk_label_new("/usr/share/mrrobotos/mrsettings/Wallpapers not found");
		gtk_widget_add_css_class(hint, "dim-label");
		gtk_widget_set_valign(hint, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(hbox), hint);
		gtk_box_append(GTK_BOX(folder_card), hscroll);
		gtk_box_append(GTK_BOX(root), folder_card);
		return scroll;
	}

	DIR *d = opendir(wp_dir);
	if (!d) {
		//GtkWidget *hint = gtk_label_new("Cannot open ~/Wallpapers");
		GtkWidget *hint = gtk_label_new("/usr/share/mrrobotos/mrsettings/Wallpapers not found");
		gtk_widget_add_css_class(hint, "dim-label");
		gtk_widget_set_valign(hint, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(hbox), hint);
		gtk_box_append(GTK_BOX(folder_card), hscroll);
		gtk_box_append(GTK_BOX(root), folder_card);
		return scroll;
	}

	char  **imgs = NULL;
	int     ni = 0, ic = 0;
	struct dirent *de;
	while ((de = readdir(d))) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
		if (!is_img(de->d_name)) continue;
		char fp[4096];
		mkpath(fp, sizeof(fp), wp_dir, de->d_name);
		struct stat st;
		if (lstat(fp, &st) || !S_ISREG(st.st_mode)) continue;
		if (ni >= ic) { ic = ic ? ic * 2 : 64; imgs = realloc(imgs, ic * sizeof *imgs); }
		imgs[ni++] = strdup(fp);
	}
	closedir(d);

	if (ni > 0) {
		qsort(imgs, ni, sizeof *imgs, cmp_strp);

		for (int i = 0; i < ni; i++) {
			/* Load thumbnail at exact display size */
			GtkWidget *pic;
			GFile *_gf = g_file_new_for_path(imgs[i]);
			pic = gtk_picture_new_for_file(_gf);
			g_object_unref(_gf);
			gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_COVER);
			/* Force exact size so every tile is uniform */
			gtk_widget_set_size_request(pic, 300, 190);
			gtk_widget_set_hexpand(pic, FALSE);
			gtk_widget_set_vexpand(pic, FALSE);
			gtk_widget_set_overflow(pic, GTK_OVERFLOW_HIDDEN);

			const char *slash = strrchr(imgs[i], '/');
			gtk_widget_set_tooltip_text(pic, slash ? slash + 1 : imgs[i]);

			GtkWidget *btn = gtk_button_new();
			gtk_widget_add_css_class(btn, "flat");
			gtk_widget_add_css_class(btn, "wp-thumb-btn");
			gtk_widget_set_size_request(btn, 300, 190);
			gtk_widget_set_hexpand(btn, FALSE);
			gtk_widget_set_vexpand(btn, FALSE);
			gtk_button_set_child(GTK_BUTTON(btn), pic);
			g_object_set_data_full(G_OBJECT(btn), "path", g_strdup(imgs[i]), g_free);
			g_object_set_data(G_OBJECT(btn), "wd", wd);
			g_signal_connect(btn, "clicked", G_CALLBACK(wp_folder_thumb_cb), NULL);

			gtk_box_append(GTK_BOX(hbox), btn);
			free(imgs[i]);
		}
	}
	free(imgs);

	gtk_box_append(GTK_BOX(folder_card), hscroll);
	gtk_box_append(GTK_BOX(root), folder_card);
	g_signal_connect_swapped(scroll, "destroy", G_CALLBACK(g_free), wd);
	return scroll;
}

/*
 * Password dialog using GtkAlertDialog with a custom entry.
 *
 * GtkAlertDialog does not natively support extra widgets, so we build
 * a small GtkWindow acting as the password prompt instead.
 * It is modal, transient for the main window, and never blocks.
 */

typedef struct {
	ConnectThreadData *td;
	GtkWidget         *connect_btn;  /* sidebar Connect button */
	GtkWidget         *entry;
	GtkWidget         *dlg_win;
} PwdWinData;

static void pwd_win_destroyed (GtkWidget *w, gpointer ud);

	static void
pwd_win_ok (GtkWidget *btn, gpointer ud)
{
	PwdWinData *pd = ud;
	const char *txt = gtk_editable_get_text (GTK_EDITABLE (pd->entry));
	strncpy (pd->td->password, txt, sizeof (pd->td->password) - 1);
	GtkWidget *win = pd->dlg_win;
	pd->dlg_win = NULL;          /* prevent double-action in destroy handler */
	gtk_window_destroy (GTK_WINDOW (win));
	g_thread_unref (g_thread_new ("nmcli-connect", wifi_connect_thread, pd->td));
	g_free (pd);
}

	static void
pwd_win_cancel (GtkWidget *btn, gpointer ud)
{
	PwdWinData *pd  = ud;
	GtkWidget  *win = pd->dlg_win;
	pd->dlg_win = NULL;
	/* re-enable Connect button */
	if (pd->connect_btn) {
		gtk_widget_set_sensitive (pd->connect_btn, TRUE);
		gtk_button_set_label (GTK_BUTTON (pd->connect_btn), "Connect");
	}
	gtk_window_destroy (GTK_WINDOW (win));
	g_free (pd->td);
	g_free (pd);
}

	static void
pwd_win_destroyed (GtkWidget *w, gpointer ud)
{
	/* Called if the window is closed via the title-bar X button */
	PwdWinData *pd = ud;
	if (!pd->dlg_win) return;   /* already handled by ok/cancel */
	pd->dlg_win = NULL;
	if (pd->connect_btn) {
		gtk_widget_set_sensitive (pd->connect_btn, TRUE);
		gtk_button_set_label (GTK_BUTTON (pd->connect_btn), "Connect");
	}
	g_free (pd->td);
	g_free (pd);
}

	static void
show_password_dialog (ConnectThreadData *td, GtkWidget *connect_btn)
{
	PwdWinData *pd = g_new0 (PwdWinData, 1);
	pd->td          = td;
	pd->connect_btn = connect_btn;

	GtkWidget *win = gtk_window_new ();
	pd->dlg_win = win;
	char title[320];
	snprintf (title, sizeof (title), "Wi-Fi Password — %s", td->ssid);
	gtk_window_set_title       (GTK_WINDOW (win), title);
	gtk_window_set_transient_for (GTK_WINDOW (win), GTK_WINDOW (window));
	gtk_window_set_modal       (GTK_WINDOW (win), TRUE);
	gtk_window_set_resizable   (GTK_WINDOW (win), FALSE);
	gtk_window_set_default_size (GTK_WINDOW (win), 360, -1);
	g_signal_connect (win, "destroy", G_CALLBACK (pwd_win_destroyed), pd);

	GtkWidget *vb = gtk_box_new (GTK_ORIENTATION_VERTICAL, 16);
	gtk_widget_set_margin_start  (vb, 24);
	gtk_widget_set_margin_end    (vb, 24);
	gtk_widget_set_margin_top    (vb, 20);
	gtk_widget_set_margin_bottom (vb, 20);

	char msg[320];
	snprintf (msg, sizeof (msg), "Enter password for \"%s\"", td->ssid);
	GtkWidget *lbl = gtk_label_new (msg);
	gtk_widget_set_halign (lbl, GTK_ALIGN_START);
	gtk_box_append (GTK_BOX (vb), lbl);

	GtkWidget *entry = gtk_entry_new ();
	gtk_entry_set_input_purpose (GTK_ENTRY (entry), GTK_INPUT_PURPOSE_PASSWORD);
	gtk_entry_set_visibility    (GTK_ENTRY (entry), FALSE);
	gtk_widget_set_hexpand (entry, TRUE);
	pd->entry = entry;
	gtk_box_append (GTK_BOX (vb), entry);

	GtkWidget *btn_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_set_halign (btn_row, GTK_ALIGN_END);

	GtkWidget *cancel_btn = gtk_button_new_with_label ("Cancel");
	g_signal_connect (cancel_btn, "clicked", G_CALLBACK (pwd_win_cancel), pd);
	gtk_box_append (GTK_BOX (btn_row), cancel_btn);

	GtkWidget *ok_btn = gtk_button_new_with_label ("Connect");
	gtk_widget_add_css_class (ok_btn, "suggested-action");
	g_signal_connect (ok_btn, "clicked", G_CALLBACK (pwd_win_ok), pd);
	gtk_box_append (GTK_BOX (btn_row), ok_btn);

	/* Enter key in the entry triggers Connect */
	g_signal_connect (entry, "activate",
			  G_CALLBACK (pwd_win_ok), pd);

	gtk_box_append (GTK_BOX (vb), btn_row);
	gtk_window_set_child (GTK_WINDOW (win), vb);
	gtk_window_present (GTK_WINDOW(win));
}

	static void
do_connect (GtkWidget *btn, gpointer ud)
{
	ConnectData *cd = ud;
	WifiData    *wd = cd->wd;
	if (!wd || wd->destroyed) return;

	gboolean secured = (cd->security[0] && strcmp (cd->security, "--") != 0);

	ConnectThreadData *td = g_new0 (ConnectThreadData, 1);
	strncpy (td->ssid,     cd->ssid,     sizeof (td->ssid)     - 1);
	strncpy (td->security, cd->security, sizeof (td->security) - 1);
	td->wd = wd;

	gtk_widget_set_sensitive (btn, FALSE);
	gtk_button_set_label (GTK_BUTTON (btn), "Connecting\xe2\x80\xa6");

	if (secured) {
		/* Open the async password window — do_connect returns immediately */
		show_password_dialog (td, btn);
	} else {
		g_thread_unref (g_thread_new ("nmcli-connect", wifi_connect_thread, td));
	}
}

/* ================================================================== */
/* Brightness                                                           */
/* ================================================================== */
static char *bright_format_pct(GtkScale *s, double v, gpointer ud) {
	return g_strdup_printf("%d%%",(int)v);
}

typedef struct { char output[64]; } BrightXrandrData;
static void bright_xrandr_changed(GtkRange *range, gpointer ud) {
	BrightXrandrData *bd=ud;
	double val=gtk_range_get_value(range)/100.0;
	char cmd[256];
	snprintf(cmd,sizeof(cmd),"xrandr --output %s --brightness %.2f 2>/dev/null",bd->output,val);
	system(cmd);
}

GtkWidget *brightness_settings(void) {
	GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	gtk_widget_set_hexpand(root,TRUE); gtk_widget_set_vexpand(root,TRUE);
	gtk_box_append(GTK_BOX(root),make_page_header("display-brightness-symbolic","Brightness"));
	gtk_box_append(GTK_BOX(root),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget *scr=gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr,TRUE);
	GtkWidget *content=gtk_box_new(GTK_ORIENTATION_VERTICAL,16);
	gtk_widget_set_margin_start(content,24); gtk_widget_set_margin_end(content,24);
	gtk_widget_set_margin_top(content,20);   gtk_widget_set_margin_bottom(content,24);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr),content);
	gtk_box_append(GTK_BOX(root),scr);
	/* per-monitor xrandr brightness only — no hardcoded intel */
	GtkWidget *mon_frame=make_section_box("Monitor Brightness");
	GtkWidget *mon_box=g_object_get_data(G_OBJECT(mon_frame),"inner-box");
	char *xr=run_cmd_str("xrandr --query 2>/dev/null | grep ' connected' | awk '{print $1}'");
	char **outputs=g_strsplit(xr,"\n",-1); g_free(xr);
	gboolean any=FALSE;
	for (int i=0;outputs[i]&&outputs[i][0];i++) {
		any=TRUE; const char *out=outputs[i];
		char cmd[256];
		snprintf(cmd,sizeof(cmd),
			 "xrandr --verbose 2>/dev/null | awk '/^%s /,/^[^ ]/' | grep -i 'Brightness:' | head -1 | awk '{print $2}'",out);
		char *bs=run_cmd_str(cmd); g_strstrip(bs);
		double cb=bs[0]?atof(bs):1.0; g_free(bs);
		GtkWidget *mr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
		gtk_widget_set_margin_start(mr,14); gtk_widget_set_margin_end(mr,14);
		gtk_widget_set_margin_top(mr,10);   gtk_widget_set_margin_bottom(mr,10);
		GtkWidget *mi=gtk_image_new_from_icon_name(strncmp(out,"eDP",3)==0?"computer-symbolic":"video-display-symbolic");
		gtk_image_set_pixel_size(GTK_IMAGE(mi),18); gtk_widget_set_valign(mi,GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(mr),mi);
		char friendly[64];
		if      (strncmp(out,"eDP",3)==0)  snprintf(friendly,sizeof(friendly),"Built-in (%s)",out);
		else if (strncmp(out,"HDMI",4)==0) snprintf(friendly,sizeof(friendly),"HDMI (%s)",out);
		else if (strncmp(out,"DP",2)==0)   snprintf(friendly,sizeof(friendly),"DisplayPort (%s)",out);
		else                               snprintf(friendly,sizeof(friendly),"%s",out);
		GtkWidget *ml=gtk_label_new(friendly);
		gtk_widget_add_css_class(ml,"dim-label"); gtk_widget_set_size_request(ml,160,-1);
		gtk_widget_set_valign(ml,GTK_ALIGN_CENTER); gtk_box_append(GTK_BOX(mr),ml);
		GtkWidget *ms=gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,10,100,1);
		gtk_range_set_value(GTK_RANGE(ms),CLAMP(cb*100.0,10.0,100.0));
		gtk_scale_set_draw_value(GTK_SCALE(ms),TRUE);
		gtk_scale_set_format_value_func(GTK_SCALE(ms),bright_format_pct,NULL,NULL);
		gtk_widget_set_hexpand(ms,TRUE);
		BrightXrandrData *bd=g_new0(BrightXrandrData,1);
		strncpy(bd->output,out,sizeof(bd->output)-1);
		g_signal_connect_data(ms,"value-changed",G_CALLBACK(bright_xrandr_changed),bd,(GClosureNotify)g_free,0);
		gtk_box_append(GTK_BOX(mr),ms); gtk_box_append(GTK_BOX(mon_box),mr);
		if (outputs[i+1]&&outputs[i+1][0])
			gtk_box_append(GTK_BOX(mon_box),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	}
	g_strfreev(outputs);
	if (!any) {
		GtkWidget *na=gtk_label_new("No connected monitors found");
		gtk_widget_add_css_class(na,"dim-label"); gtk_widget_set_halign(na,GTK_ALIGN_CENTER);
		gtk_widget_set_margin_top(na,12); gtk_widget_set_margin_bottom(na,12);
		gtk_box_append(GTK_BOX(mon_box),na);
	}
	gtk_box_append(GTK_BOX(content),mon_frame);
	return root;
}

/* ================================================================== */
/* Keyboard                                                             */
/* ================================================================== */
typedef struct {
	GtkWidget *layout_dd, *variant_dd, *status_lbl;
	GtkStringList *layout_list;
	char current_layout[64], current_variant[64];
} KbdData;

static void kbd_apply(GtkWidget *btn, gpointer ud) {
	KbdData *kd=ud;
	guint li=gtk_drop_down_get_selected(GTK_DROP_DOWN(kd->layout_dd));
	GObject *lo=g_list_model_get_item(gtk_drop_down_get_model(GTK_DROP_DOWN(kd->layout_dd)),li);
	const char *layout=lo?gtk_string_object_get_string(GTK_STRING_OBJECT(lo)):"us";
	if(lo)g_object_unref(lo);
	guint vi=gtk_drop_down_get_selected(GTK_DROP_DOWN(kd->variant_dd));
	GObject *vo=g_list_model_get_item(gtk_drop_down_get_model(GTK_DROP_DOWN(kd->variant_dd)),vi);
	const char *variant=vo?gtk_string_object_get_string(GTK_STRING_OBJECT(vo)):"";
	if(vo)g_object_unref(vo);
	char cmd[512];
	if (variant&&variant[0]&&strcmp(variant,"(none)")!=0)
		snprintf(cmd,sizeof(cmd),"setxkbmap -layout '%s' -variant '%s' 2>/dev/null",layout,variant);
	else
		snprintf(cmd,sizeof(cmd),"setxkbmap -layout '%s' 2>/dev/null",layout);
	system(cmd);
	char msg[128];
	snprintf(msg,sizeof(msg),"Applied: %s%s%s",layout,
		 (variant&&variant[0]&&strcmp(variant,"(none)")!=0)?" / ":"",
		 (variant&&variant[0]&&strcmp(variant,"(none)")!=0)?variant:"");
	gtk_label_set_text(GTK_LABEL(kd->status_lbl),msg);
}

static void kbd_layout_changed(GtkDropDown *dd, GParamSpec *ps, gpointer ud) {
	KbdData *kd=ud;
	guint li=gtk_drop_down_get_selected(dd);
	GObject *lo=g_list_model_get_item(gtk_drop_down_get_model(GTK_DROP_DOWN(kd->layout_dd)),li);
	if(!lo)return;
	const char *layout=gtk_string_object_get_string(GTK_STRING_OBJECT(lo));
	g_object_unref(lo);
	char cmd[256];
	snprintf(cmd,sizeof(cmd),
		 "grep -A999 '^! variant' /usr/share/X11/xkb/rules/evdev.lst 2>/dev/null"
		 " | grep '%s$' | awk '{print $1}'",layout);
	char *raw=run_cmd_str(cmd); char **variants=g_strsplit(raw,"\n",-1); g_free(raw);
	GtkStringList *vl=gtk_string_list_new(NULL);
	gtk_string_list_append(vl,"(none)");
	for(int i=0;variants[i]&&variants[i][0];i++) gtk_string_list_append(vl,variants[i]);
	g_strfreev(variants);
	gtk_drop_down_set_model(GTK_DROP_DOWN(kd->variant_dd),G_LIST_MODEL(vl));
	gtk_drop_down_set_selected(GTK_DROP_DOWN(kd->variant_dd),0);
}

GtkWidget *keyboard_settings(void) {
	KbdData *kd=g_new0(KbdData,1);
	char *cr=run_cmd_str("setxkbmap -query 2>/dev/null | grep '^layout' | awk '{print $2}' | tr -d '\\n'");
	g_strstrip(cr); strncpy(kd->current_layout,cr[0]?cr:"us",sizeof(kd->current_layout)-1); g_free(cr);
	char *vr=run_cmd_str("setxkbmap -query 2>/dev/null | grep '^variant' | awk '{print $2}' | tr -d '\\n'");
	g_strstrip(vr); strncpy(kd->current_variant,vr,sizeof(kd->current_variant)-1); g_free(vr);
	GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	gtk_widget_set_hexpand(root,TRUE); gtk_widget_set_vexpand(root,TRUE);
	gtk_box_append(GTK_BOX(root),make_page_header("input-keyboard-symbolic","Keyboard"));
	gtk_box_append(GTK_BOX(root),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget *scr=gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr,TRUE);
	GtkWidget *content=gtk_box_new(GTK_ORIENTATION_VERTICAL,16);
	gtk_widget_set_margin_start(content,24); gtk_widget_set_margin_end(content,24);
	gtk_widget_set_margin_top(content,20);   gtk_widget_set_margin_bottom(content,24);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr),content);
	gtk_box_append(GTK_BOX(root),scr);
	GtkWidget *kbd_hdr = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_halign(kbd_hdr, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_bottom(kbd_hdr, 8);
	GtkWidget *kbd_ico = gtk_image_new_from_file("/usr/share/icons/mrrobotos/128x128/devices/keyboard.png");
	gtk_image_set_pixel_size(GTK_IMAGE(kbd_ico), 128);
	gtk_widget_set_halign(kbd_ico, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(kbd_hdr), kbd_ico);
	char cm[128]; snprintf(cm,sizeof(cm),"Current: %s%s%s",kd->current_layout,
			       kd->current_variant[0]?" / ":"",kd->current_variant);
	GtkWidget *cl = gtk_label_new(cm);
	gtk_widget_add_css_class(cl, "dim-label");
	gtk_widget_set_halign(cl, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(kbd_hdr), cl);
	gtk_box_append(GTK_BOX(content), kbd_hdr);
	//char cm[128]; snprintf(cm,sizeof(cm),"Current: %s%s%s",kd->current_layout,
	//    kd->current_variant[0]?" / ":"",kd->current_variant);
	//GtkWidget *cl=gtk_label_new(cm);
	//gtk_widget_add_css_class(cl,"dim-label"); gtk_widget_set_halign(cl,GTK_ALIGN_START);
	//gtk_box_append(GTK_BOX(content),cl);
	GtkWidget *lf=make_section_box("Keyboard Layout");
	GtkWidget *lb=g_object_get_data(G_OBJECT(lf),"inner-box");
	char *lr=run_cmd_str("awk '/^! layout/{found=1;next} found&&/^! /{found=0} found&&NF>=2{print $1}' /usr/share/X11/xkb/rules/evdev.lst 2>/dev/null");
	char **la=g_strsplit(lr,"\n",-1); g_free(lr);
	GtkStringList *ll=gtk_string_list_new(NULL); int ci=0,idx=0;
	for(int i=0;la[i]&&la[i][0];i++){
		gtk_string_list_append(ll,la[i]);
		if(strcmp(la[i],kd->current_layout)==0)ci=idx;
		idx++;
	}
	g_strfreev(la); kd->layout_list=ll;
	GtkWidget *lr2=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
	gtk_widget_set_margin_start(lr2,14); gtk_widget_set_margin_end(lr2,14);
	gtk_widget_set_margin_top(lr2,12);   gtk_widget_set_margin_bottom(lr2,8);
	GtkWidget *ll2=gtk_label_new("Layout");
	gtk_widget_add_css_class(ll2,"dim-label"); gtk_widget_set_size_request(ll2,80,-1);
	gtk_widget_set_halign(ll2,GTK_ALIGN_START); gtk_widget_set_valign(ll2,GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(lr2),ll2);
	GtkWidget *ld=gtk_drop_down_new(G_LIST_MODEL(ll),NULL);
	gtk_drop_down_set_selected(GTK_DROP_DOWN(ld),ci);
	gtk_drop_down_set_enable_search(GTK_DROP_DOWN(ld),TRUE);
	gtk_widget_set_hexpand(ld,TRUE); kd->layout_dd=ld;
	gtk_box_append(GTK_BOX(lr2),ld); gtk_box_append(GTK_BOX(lb),lr2);
	gtk_box_append(GTK_BOX(lb),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget *vr2=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
	gtk_widget_set_margin_start(vr2,14); gtk_widget_set_margin_end(vr2,14);
	gtk_widget_set_margin_top(vr2,8);    gtk_widget_set_margin_bottom(vr2,12);
	GtkWidget *vl2=gtk_label_new("Variant");
	gtk_widget_add_css_class(vl2,"dim-label"); gtk_widget_set_size_request(vl2,80,-1);
	gtk_widget_set_halign(vl2,GTK_ALIGN_START); gtk_widget_set_valign(vl2,GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(vr2),vl2);
	GtkStringList *vl_init=gtk_string_list_new(NULL); gtk_string_list_append(vl_init,"(none)");
	GtkWidget *vd=gtk_drop_down_new(G_LIST_MODEL(vl_init),NULL);
	gtk_drop_down_set_enable_search(GTK_DROP_DOWN(vd),TRUE);
	gtk_widget_set_hexpand(vd,TRUE); kd->variant_dd=vd;
	gtk_box_append(GTK_BOX(vr2),vd); gtk_box_append(GTK_BOX(lb),vr2);
	g_signal_connect(ld,"notify::selected",G_CALLBACK(kbd_layout_changed),kd);
	kbd_layout_changed(GTK_DROP_DOWN(ld),NULL,kd);
	if (kd->current_variant[0]) {
		GListModel *vm=gtk_drop_down_get_model(GTK_DROP_DOWN(vd));
		guint vn=g_list_model_get_n_items(vm);
		for(guint j=0;j<vn;j++){
			GObject *o=g_list_model_get_item(vm,j);
			if(strcmp(gtk_string_object_get_string(GTK_STRING_OBJECT(o)),kd->current_variant)==0){
				gtk_drop_down_set_selected(GTK_DROP_DOWN(vd),j); g_object_unref(o); break;
			}
			g_object_unref(o);
		}
	}
	gtk_box_append(GTK_BOX(content),lf);
	GtkWidget *ab=gtk_button_new_with_label("Apply Layout");
	gtk_widget_add_css_class(ab,"suggested-action"); gtk_widget_add_css_class(ab,"pill");
	gtk_widget_set_halign(ab,GTK_ALIGN_CENTER);
	g_signal_connect(ab,"clicked",G_CALLBACK(kbd_apply),kd);
	gtk_box_append(GTK_BOX(content),ab);
	GtkWidget *sl=gtk_label_new("");
	gtk_widget_add_css_class(sl,"dim-label"); gtk_widget_set_halign(sl,GTK_ALIGN_CENTER);
	kd->status_lbl=sl; gtk_box_append(GTK_BOX(content),sl);
	g_signal_connect_swapped(root,"destroy",G_CALLBACK(g_free),kd);
	return root;
}

/* ================================================================== */
/* Users & Groups                                                       */
/* ================================================================== */
GtkWidget *users_settings(void) {
	GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	gtk_widget_set_hexpand(root,TRUE); gtk_widget_set_vexpand(root,TRUE);
	gtk_box_append(GTK_BOX(root),make_page_header("system-users-symbolic","Users & Groups"));
	gtk_box_append(GTK_BOX(root),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget *scr=gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr,TRUE);
	GtkWidget *vb=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	gtk_widget_set_margin_start(vb,16); gtk_widget_set_margin_end(vb,16);
	gtk_widget_set_margin_top(vb,12);   gtk_widget_set_margin_bottom(vb,12);
	/* Users */
	GtkWidget *ul=gtk_label_new("Users");
	gtk_widget_add_css_class(ul,"title-4"); gtk_widget_set_halign(ul,GTK_ALIGN_START);
	gtk_widget_set_margin_bottom(ul,8); gtk_box_append(GTK_BOX(vb),ul);
	GtkWidget *user_list=gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(user_list),GTK_SELECTION_NONE);
	gtk_widget_add_css_class(user_list,"boxed-list");
	gtk_widget_set_margin_bottom(user_list,20);
	setpwent();
	struct passwd *pw;
	while ((pw=getpwent())) {
		if (pw->pw_uid<1000||pw->pw_uid>65000) continue;
		if (!pw->pw_dir||strncmp(pw->pw_dir,"/home",5)!=0) continue;
		GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
		gtk_widget_set_margin_start(row,14); gtk_widget_set_margin_end(row,14);
		gtk_widget_set_margin_top(row,10);   gtk_widget_set_margin_bottom(row,10);
		char avatarp[512]; snprintf(avatarp,sizeof(avatarp),"%s/.face",pw->pw_dir);
		GtkWidget *av;
		if (g_file_test(avatarp,G_FILE_TEST_EXISTS)) av=gtk_image_new_from_file(avatarp);
		else av=gtk_image_new_from_icon_name("avatar-default-symbolic");
		gtk_image_set_pixel_size(GTK_IMAGE(av),40); gtk_box_append(GTK_BOX(row),av);
		GtkWidget *inf=gtk_box_new(GTK_ORIENTATION_VERTICAL,2); gtk_widget_set_hexpand(inf,TRUE);
		GtkWidget *nl=gtk_label_new(pw->pw_name);
		gtk_widget_add_css_class(nl,"body"); gtk_widget_set_halign(nl,GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(inf),nl);
		char uid_str[64]; snprintf(uid_str,sizeof(uid_str),"UID %d · %s",pw->pw_uid,pw->pw_shell);
		GtkWidget *sl=gtk_label_new(uid_str);
		gtk_widget_add_css_class(sl,"dim-label"); gtk_widget_add_css_class(sl,"caption");
		gtk_widget_set_halign(sl,GTK_ALIGN_START); gtk_box_append(GTK_BOX(inf),sl);
		gtk_box_append(GTK_BOX(row),inf);
		char chk[256];
		snprintf(chk,sizeof(chk),"groups %s 2>/dev/null | grep -qw wheel && echo yes",pw->pw_name);
		FILE *gf=popen(chk,"r"); char gbuf[8]="";
		if(gf){fgets(gbuf,sizeof(gbuf),gf);pclose(gf);}
		if (strncmp(gbuf,"yes",3)==0) {
			GtkWidget *badge=gtk_label_new("sudo");
			gtk_widget_add_css_class(badge,"tag"); gtk_widget_set_valign(badge,GTK_ALIGN_CENTER);
			gtk_box_append(GTK_BOX(row),badge);
		}
		gtk_list_box_append(GTK_LIST_BOX(user_list),row);
	}
	endpwent();
	gtk_box_append(GTK_BOX(vb),user_list);
	/* Groups */
	GtkWidget *gl=gtk_label_new("Groups");
	gtk_widget_add_css_class(gl,"title-4"); gtk_widget_set_halign(gl,GTK_ALIGN_START);
	gtk_widget_set_margin_bottom(gl,8); gtk_box_append(GTK_BOX(vb),gl);
	GtkWidget *grp_list=gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(grp_list),GTK_SELECTION_NONE);
	gtk_widget_add_css_class(grp_list,"boxed-list");
	setgrent();
	struct group *gr;
	while ((gr=getgrent())) {
		int hm=gr->gr_mem&&gr->gr_mem[0];
		if (!hm&&gr->gr_gid<1000) continue;
		GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
		gtk_widget_set_margin_start(row,14); gtk_widget_set_margin_end(row,14);
		gtk_widget_set_margin_top(row,8);    gtk_widget_set_margin_bottom(row,8);
		GtkWidget *ni=gtk_image_new_from_icon_name("system-users-symbolic");
		gtk_image_set_pixel_size(GTK_IMAGE(ni),24); gtk_box_append(GTK_BOX(row),ni);
		GtkWidget *inf=gtk_box_new(GTK_ORIENTATION_VERTICAL,2); gtk_widget_set_hexpand(inf,TRUE);
		GtkWidget *gn=gtk_label_new(gr->gr_name); gtk_widget_set_halign(gn,GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(inf),gn);
		if (hm) {
			GString *ms=g_string_new(NULL);
			for(int m=0;gr->gr_mem[m];m++){if(m)g_string_append(ms,", ");g_string_append(ms,gr->gr_mem[m]);}
			GtkWidget *ml=gtk_label_new(ms->str);
			gtk_widget_add_css_class(ml,"dim-label"); gtk_widget_add_css_class(ml,"caption");
			gtk_widget_set_halign(ml,GTK_ALIGN_START);
			gtk_label_set_ellipsize(GTK_LABEL(ml),PANGO_ELLIPSIZE_END);
			gtk_box_append(GTK_BOX(inf),ml); g_string_free(ms,TRUE);
		}
		gtk_box_append(GTK_BOX(row),inf);
		char gs[32]; snprintf(gs,sizeof(gs),"GID %d",gr->gr_gid);
		GtkWidget *gidl=gtk_label_new(gs);
		gtk_widget_add_css_class(gidl,"dim-label"); gtk_widget_add_css_class(gidl,"caption");
		gtk_widget_set_valign(gidl,GTK_ALIGN_CENTER); gtk_box_append(GTK_BOX(row),gidl);
		gtk_list_box_append(GTK_LIST_BOX(grp_list),row);
	}
	endgrent();
	gtk_box_append(GTK_BOX(vb),grp_list);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr),vb);
	gtk_box_append(GTK_BOX(root),scr);
	return root;
}

/* ================================================================== */
/* Date & Time                                                          */
/* ================================================================== */
static GtkWidget *dt_time_label=NULL, *dt_date_label=NULL, *dt_tz_label=NULL;
static guint      dt_tick_id    = 0;

static gboolean dt_tick(gpointer ud) {
	if (!dt_time_label) {
		dt_tick_id = 0;
		return G_SOURCE_REMOVE;
	}
	time_t now=time(NULL); struct tm *tm=localtime(&now);
	char tb[32],db[64];
	strftime(tb,sizeof(tb),"%H:%M:%S",tm);
	strftime(db,sizeof(db),"%A, %B %d %Y",tm);
	gtk_label_set_text(GTK_LABEL(dt_time_label),tb);
	gtk_label_set_text(GTK_LABEL(dt_date_label),db);
	return G_SOURCE_CONTINUE;
}
static void dt_apply_tz(GtkWidget *btn, gpointer ud) {
	GtkDropDown *dd=GTK_DROP_DOWN(ud);
	GtkStringList *sl=GTK_STRING_LIST(gtk_drop_down_get_model(dd));
	const char *tz=gtk_string_list_get_string(sl,gtk_drop_down_get_selected(dd));
	char cmd[256]; snprintf(cmd,sizeof(cmd),"timedatectl set-timezone '%s' 2>/dev/null",tz);
	system(cmd);
	char *cur=run_cmd_str("timedatectl show --property=Timezone --value 2>/dev/null | tr -d '\\n'");
	gtk_label_set_text(GTK_LABEL(dt_tz_label),cur); g_free(cur);
}
static void dt_toggle_ntp(GtkSwitch *sw, GParamSpec *ps, gpointer ud) {
	if (gtk_switch_get_active(sw)) system("timedatectl set-ntp true 2>/dev/null");
	else                           system("timedatectl set-ntp false 2>/dev/null");
}

static void dt_page_destroyed(GtkWidget *w, gpointer ud) {
	dt_time_label = NULL;
	dt_date_label = NULL;
	dt_tz_label   = NULL;
	if (dt_tick_id) { g_source_remove(dt_tick_id); dt_tick_id = 0; }
}

GtkWidget *datetime_settings(void) {
	GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	gtk_widget_set_hexpand(root,TRUE); gtk_widget_set_vexpand(root,TRUE);
	gtk_box_append(GTK_BOX(root),make_page_header("preferences-system-time-symbolic","Date & Time"));
	gtk_box_append(GTK_BOX(root),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget *scr=gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr,TRUE);
	GtkWidget *vb=gtk_box_new(GTK_ORIENTATION_VERTICAL,16);
	gtk_widget_set_margin_start(vb,16); gtk_widget_set_margin_end(vb,16);
	gtk_widget_set_margin_top(vb,20);   gtk_widget_set_margin_bottom(vb,20);
	/* live clock */
	GtkWidget *cb=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
	gtk_widget_set_halign(cb,GTK_ALIGN_CENTER); gtk_widget_set_margin_bottom(cb,8);
	dt_time_label=gtk_label_new("00:00:00");
	gtk_widget_add_css_class(dt_time_label,"title-1");
	gtk_widget_set_halign(dt_time_label,GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(cb),dt_time_label);
	dt_date_label=gtk_label_new("");
	gtk_widget_add_css_class(dt_date_label,"title-4");
	gtk_widget_add_css_class(dt_date_label,"dim-label");
	gtk_widget_set_halign(dt_date_label,GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(cb),dt_date_label);
	gtk_box_append(GTK_BOX(vb),cb);
	//dt_tick(NULL); g_timeout_add_seconds(1,dt_tick,NULL);
	dt_tick(NULL);
	if (dt_tick_id) g_source_remove(dt_tick_id);
	dt_tick_id = g_timeout_add_seconds(1, dt_tick, NULL);
	/* NTP */
	GtkWidget *ntp_list=gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(ntp_list),GTK_SELECTION_NONE);
	gtk_widget_add_css_class(ntp_list,"boxed-list");
	GtkWidget *nr=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
	gtk_widget_set_margin_start(nr,14); gtk_widget_set_margin_end(nr,14);
	gtk_widget_set_margin_top(nr,10);   gtk_widget_set_margin_bottom(nr,10);
	GtkWidget *nl=gtk_label_new("Automatic (NTP)");
	gtk_widget_set_hexpand(nl,TRUE); gtk_widget_set_halign(nl,GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(nr),nl);
	GtkWidget *nsw=gtk_switch_new();
	char *ns=run_cmd_str("timedatectl show --property=NTP --value 2>/dev/null | tr -d '\\n'");
	gtk_switch_set_active(GTK_SWITCH(nsw),strncmp(ns,"yes",3)==0); g_free(ns);
	gtk_widget_set_valign(nsw,GTK_ALIGN_CENTER);
	g_signal_connect(nsw,"notify::active",G_CALLBACK(dt_toggle_ntp),NULL);
	gtk_box_append(GTK_BOX(nr),nsw);
	gtk_list_box_append(GTK_LIST_BOX(ntp_list),nr);
	gtk_box_append(GTK_BOX(vb),ntp_list);
	/* timezone — full searchable list, pre-selected to current */
	{
		GtkWidget *tz_frame = make_section_box("Timezone");
		GtkWidget *tz_inner = g_object_get_data(G_OBJECT(tz_frame), "inner-box");

		/* current timezone display */
		char *ctz = run_cmd_str(
					"timedatectl show --property=Timezone --value 2>/dev/null | tr -d '\n'");
		g_strstrip(ctz);
		GtkWidget *tz_cur_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
		gtk_widget_set_margin_start(tz_cur_row, 14); gtk_widget_set_margin_end(tz_cur_row, 14);
		gtk_widget_set_margin_top(tz_cur_row, 12);   gtk_widget_set_margin_bottom(tz_cur_row, 12);
		GtkWidget *tz_ico = gtk_image_new_from_icon_name("preferences-system-time-symbolic");
		gtk_image_set_pixel_size(GTK_IMAGE(tz_ico), 22);
		gtk_widget_set_valign(tz_ico, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(tz_cur_row), tz_ico);
		GtkWidget *tz_cur_inf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		gtk_widget_set_hexpand(tz_cur_inf, TRUE);
		GtkWidget *tz_cur_lbl = gtk_label_new("Current Timezone");
		gtk_widget_set_halign(tz_cur_lbl, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(tz_cur_inf), tz_cur_lbl);
		dt_tz_label = gtk_label_new(ctz[0] ? ctz : "Unknown");
		gtk_widget_add_css_class(dt_tz_label, "dim-label");
		gtk_widget_set_halign(dt_tz_label, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(tz_cur_inf), dt_tz_label);
		gtk_box_append(GTK_BOX(tz_cur_row), tz_cur_inf);
		gtk_box_append(GTK_BOX(tz_inner), tz_cur_row);
		gtk_box_append(GTK_BOX(tz_inner), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

		/* searchable timezone dropdown */
		char *tz_raw = run_cmd_str("timedatectl list-timezones 2>/dev/null");
		char **tz_lines = g_strsplit(tz_raw, "\n", -1);
		g_free(tz_raw);
		GtkStringList *tz_model = gtk_string_list_new(NULL);
		guint tz_cur_idx = 0, tz_i = 0;
		for (int i = 0; tz_lines[i] && tz_lines[i][0]; i++) {
			gtk_string_list_append(tz_model, tz_lines[i]);
			if (strcmp(tz_lines[i], ctz) == 0) tz_cur_idx = tz_i;
			tz_i++;
		}
		g_strfreev(tz_lines);
		g_free(ctz);

		GtkWidget *tz_sel_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		gtk_widget_set_margin_start(tz_sel_row, 14); gtk_widget_set_margin_end(tz_sel_row, 14);
		gtk_widget_set_margin_top(tz_sel_row, 10);   gtk_widget_set_margin_bottom(tz_sel_row, 10);
		GtkWidget *tz_sel_lbl = gtk_label_new("Select Timezone");
		gtk_widget_add_css_class(tz_sel_lbl, "dim-label");
		gtk_widget_set_hexpand(tz_sel_lbl, TRUE);
		gtk_widget_set_halign(tz_sel_lbl, GTK_ALIGN_START);
		gtk_widget_set_valign(tz_sel_lbl, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(tz_sel_row), tz_sel_lbl);
		GtkWidget *tdd = gtk_drop_down_new(G_LIST_MODEL(tz_model), NULL);
		gtk_drop_down_set_enable_search(GTK_DROP_DOWN(tdd), TRUE);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(tdd), tz_cur_idx);
		gtk_widget_set_size_request(tdd, 260, -1);
		gtk_box_append(GTK_BOX(tz_sel_row), tdd);
		GtkWidget *ta = gtk_button_new_with_label("Apply");
		gtk_widget_add_css_class(ta, "suggested-action");
		g_signal_connect(ta, "clicked", G_CALLBACK(dt_apply_tz), tdd);
		gtk_box_append(GTK_BOX(tz_sel_row), ta);
		gtk_box_append(GTK_BOX(tz_inner), tz_sel_row);
		gtk_box_append(GTK_BOX(vb), tz_frame);
	}
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr),vb);
	gtk_box_append(GTK_BOX(root),scr);
	g_signal_connect(root, "destroy", G_CALLBACK(dt_page_destroyed), NULL);
	return root;
}

/* ================================================================== */
/* Region & Language                                                    */
/* ================================================================== */

static void region_apply_locale(GtkWidget *btn, gpointer ud) {
	GtkDropDown *dd = GTK_DROP_DOWN(ud);
	GObject *item = g_list_model_get_item(gtk_drop_down_get_model(dd),
					      gtk_drop_down_get_selected(dd));
	if (!item) return;
	const char *locale = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
	g_object_unref(item);
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "localectl set-locale LANG='%s' 2>/dev/null", locale);
	system(cmd);
}

static void region_apply_format(GtkWidget *btn, gpointer ud) {
	GtkDropDown *dd = GTK_DROP_DOWN(ud);
	GObject *item = g_list_model_get_item(gtk_drop_down_get_model(dd),
					      gtk_drop_down_get_selected(dd));
	if (!item) return;
	const char *locale = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
	g_object_unref(item);
	char cmd[512];
	snprintf(cmd, sizeof(cmd),
		 "localectl set-locale LC_TIME='%s' LC_NUMERIC='%s' LC_MONETARY='%s' LC_PAPER='%s' 2>/dev/null",
		 locale, locale, locale, locale);
	system(cmd);
}


static void region_goto_keyboard(GtkWidget *btn, gpointer ud) {
	if (!sidebar_listbox) return;
	int total = 0;
	while (gtk_list_box_get_row_at_index(GTK_LIST_BOX(sidebar_listbox), total)) total++;
	for (int i = 0; i < total; i++) {
		GtkListBoxRow *r = gtk_list_box_get_row_at_index(GTK_LIST_BOX(sidebar_listbox), i);
		if (!r) continue;
		const char *name = g_object_get_data(G_OBJECT(r), "page-name");
		if (name && strcmp(name, "Keyboard") == 0) {
			gtk_list_box_select_row(GTK_LIST_BOX(sidebar_listbox), r);
			return;
		}
	}
}

GtkWidget *region_settings(void) {
	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand(root, TRUE); gtk_widget_set_vexpand(root, TRUE);
	gtk_box_append(GTK_BOX(root), make_page_header("preferences-desktop-locale-symbolic", "Region & Language"));
	gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	GtkWidget *scr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr, TRUE);
	GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
	gtk_widget_set_margin_start(vb, 24); gtk_widget_set_margin_end(vb, 24);
	gtk_widget_set_margin_top(vb, 20);   gtk_widget_set_margin_bottom(vb, 24);

	/* ---- gather current values ---- */
	char *cur_lang = run_cmd_str(
				     "localectl status 2>/dev/null | grep 'LANG=' | cut -d= -f2 | tr -d ' \n'");
	if (!cur_lang || !cur_lang[0]) {
		g_free(cur_lang);
		cur_lang = g_strdup(getenv("LANG") ? getenv("LANG") : "en_US.UTF-8");
	}
	g_strstrip(cur_lang);

	char *cur_fmt = run_cmd_str(
				    "localectl status 2>/dev/null | grep 'LC_TIME=' | cut -d= -f2 | tr -d ' \n'");
	if (!cur_fmt || !cur_fmt[0]) { g_free(cur_fmt); cur_fmt = g_strdup(cur_lang); }
	g_strstrip(cur_fmt);

	/* build locale list */
	char *loc_raw = run_cmd_str("localectl list-locales 2>/dev/null");
	char **loc_lines = g_strsplit(loc_raw, "\n", -1);
	g_free(loc_raw);
	GtkStringList *loc_model = gtk_string_list_new(NULL);
	guint n_loc = 0, lang_idx = 0, fmt_idx = 0;
	for (int i = 0; loc_lines[i] && loc_lines[i][0]; i++) {
		gtk_string_list_append(loc_model, loc_lines[i]);
		if (strcmp(loc_lines[i], cur_lang) == 0) lang_idx = n_loc;
		if (strcmp(loc_lines[i], cur_fmt)  == 0) fmt_idx  = n_loc;
		n_loc++;
	}
	g_strfreev(loc_lines);

	/* ======================================================
	 * LANGUAGE section
	 * ====================================================== */
	GtkWidget *lang_frame = make_bat_box("Language");
	GtkWidget *lang_inner = g_object_get_data(G_OBJECT(lang_frame), "inner-box");

	/* current value row */
	{
		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
		gtk_widget_set_margin_start(row, 16); gtk_widget_set_margin_end(row, 16);
		gtk_widget_set_margin_top(row, 14);   gtk_widget_set_margin_bottom(row, 14);
		GtkWidget *ico = gtk_image_new_from_icon_name("preferences-desktop-locale-symbolic");
		gtk_image_set_pixel_size(GTK_IMAGE(ico), 32);
		gtk_widget_set_valign(ico, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(row), ico);
		GtkWidget *inf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
		gtk_widget_set_hexpand(inf, TRUE);
		GtkWidget *t = gtk_label_new("Language");
		gtk_widget_add_css_class(t, "title-4");
		gtk_widget_set_halign(t, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(inf), t);
		GtkWidget *v = gtk_label_new(cur_lang);
		gtk_widget_add_css_class(v, "dim-label");
		gtk_widget_set_halign(v, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(inf), v);
		gtk_box_append(GTK_BOX(row), inf);
		gtk_box_append(GTK_BOX(lang_inner), row);
		gtk_box_append(GTK_BOX(lang_inner), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	}
	/* change row */
	{
		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		gtk_widget_set_margin_start(row, 16); gtk_widget_set_margin_end(row, 16);
		gtk_widget_set_margin_top(row, 10);   gtk_widget_set_margin_bottom(row, 10);
		GtkWidget *lbl = gtk_label_new("Select Language");
		gtk_widget_add_css_class(lbl, "dim-label");
		gtk_widget_set_hexpand(lbl, TRUE);
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(row), lbl);
		GtkStringList *lm = gtk_string_list_new(NULL);
		for (guint i = 0; i < n_loc; i++) {
			GObject *o = g_list_model_get_item(G_LIST_MODEL(loc_model), i);
			gtk_string_list_append(lm, gtk_string_object_get_string(GTK_STRING_OBJECT(o)));
			g_object_unref(o);
		}
		GtkWidget *dd = gtk_drop_down_new(G_LIST_MODEL(lm), NULL);
		gtk_drop_down_set_enable_search(GTK_DROP_DOWN(dd), TRUE);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), lang_idx);
		gtk_widget_set_size_request(dd, 260, -1);
		gtk_box_append(GTK_BOX(row), dd);
		GtkWidget *apply = gtk_button_new_with_label("Apply");
		gtk_widget_add_css_class(apply, "suggested-action");
		g_signal_connect(apply, "clicked", G_CALLBACK(region_apply_locale), dd);
		gtk_box_append(GTK_BOX(row), apply);
		gtk_box_append(GTK_BOX(lang_inner), row);
	}
	gtk_box_append(GTK_BOX(vb), lang_frame);

	/* ======================================================
	 * FORMATS section
	 * ====================================================== */
	GtkWidget *fmt_frame = make_bat_box("Formats");
	GtkWidget *fmt_inner = g_object_get_data(G_OBJECT(fmt_frame), "inner-box");

	/* show all LC_ vars */
	struct { const char *key; const char *label; const char *icon; } lc_vars[] = {
		{ "LC_TIME",     "Date & Time",   "preferences-system-time-symbolic"   },
		{ "LC_NUMERIC",  "Numbers",       "accessories-calculator-symbolic"     },
		{ "LC_MONETARY", "Currency",      "emblem-money-symbolic"               },
		{ "LC_PAPER",    "Paper Size",    "printer-symbolic"                    },
		{ NULL, NULL, NULL }
	};
	for (int i = 0; lc_vars[i].key; i++) {
		char cmd2[256];
		snprintf(cmd2, sizeof(cmd2),
			 "localectl status 2>/dev/null | grep '%s=' | cut -d= -f2 | tr -d ' \n'",
			 lc_vars[i].key);
		char *val = run_cmd_str(cmd2);
		if (!val || !val[0]) {
			g_free(val);
			val = g_strdup(getenv(lc_vars[i].key) ? getenv(lc_vars[i].key) : cur_fmt);
		}
		g_strstrip(val);

		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
		gtk_widget_set_margin_start(row, 16); gtk_widget_set_margin_end(row, 16);
		gtk_widget_set_margin_top(row, 12);   gtk_widget_set_margin_bottom(row, 12);
		GtkWidget *ico = gtk_image_new_from_icon_name(lc_vars[i].icon);
		gtk_image_set_pixel_size(GTK_IMAGE(ico), 22);
		gtk_widget_set_valign(ico, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(row), ico);
		GtkWidget *inf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		gtk_widget_set_hexpand(inf, TRUE);
		GtkWidget *t = gtk_label_new(lc_vars[i].label);
		gtk_widget_set_halign(t, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(inf), t);
		GtkWidget *v = gtk_label_new(val[0] ? val : "—");
		gtk_widget_add_css_class(v, "dim-label");
		gtk_widget_add_css_class(v, "caption");
		gtk_widget_add_css_class(v, "monospace");
		gtk_widget_set_halign(v, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(inf), v);
		gtk_box_append(GTK_BOX(row), inf);
		gtk_box_append(GTK_BOX(fmt_inner), row);
		if (lc_vars[i+1].key)
			gtk_box_append(GTK_BOX(fmt_inner), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
		g_free(val);
	}
	gtk_box_append(GTK_BOX(fmt_inner), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	/* change formats row */
	{
		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		gtk_widget_set_margin_start(row, 16); gtk_widget_set_margin_end(row, 16);
		gtk_widget_set_margin_top(row, 10);   gtk_widget_set_margin_bottom(row, 10);
		GtkWidget *lbl = gtk_label_new("Format Locale");
		gtk_widget_add_css_class(lbl, "dim-label");
		gtk_widget_set_hexpand(lbl, TRUE);
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(row), lbl);
		GtkStringList *fm = gtk_string_list_new(NULL);
		for (guint i = 0; i < n_loc; i++) {
			GObject *o = g_list_model_get_item(G_LIST_MODEL(loc_model), i);
			gtk_string_list_append(fm, gtk_string_object_get_string(GTK_STRING_OBJECT(o)));
			g_object_unref(o);
		}
		GtkWidget *dd = gtk_drop_down_new(G_LIST_MODEL(fm), NULL);
		gtk_drop_down_set_enable_search(GTK_DROP_DOWN(dd), TRUE);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), fmt_idx);
		gtk_widget_set_size_request(dd, 260, -1);
		gtk_box_append(GTK_BOX(row), dd);
		GtkWidget *apply = gtk_button_new_with_label("Apply");
		gtk_widget_add_css_class(apply, "suggested-action");
		g_signal_connect(apply, "clicked", G_CALLBACK(region_apply_format), dd);
		gtk_box_append(GTK_BOX(row), apply);
		gtk_box_append(GTK_BOX(fmt_inner), row);
	}
	gtk_box_append(GTK_BOX(vb), fmt_frame);

	/* ======================================================
	 * TIMEZONE section (searchable dropdown, shows current)
	 * ====================================================== */
	GtkWidget *tz_frame = make_bat_box("Timezone");
	GtkWidget *tz_inner = g_object_get_data(G_OBJECT(tz_frame), "inner-box");

	char *cur_tz = run_cmd_str(
				   "timedatectl show --property=Timezone --value 2>/dev/null | tr -d '\n'");
	g_strstrip(cur_tz);

	/* current timezone display row */
	{
		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
		gtk_widget_set_margin_start(row, 16); gtk_widget_set_margin_end(row, 16);
		gtk_widget_set_margin_top(row, 14);   gtk_widget_set_margin_bottom(row, 14);
		GtkWidget *ico = gtk_image_new_from_icon_name("preferences-system-time-symbolic");
		gtk_image_set_pixel_size(GTK_IMAGE(ico), 32);
		gtk_widget_set_valign(ico, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(row), ico);
		GtkWidget *inf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
		gtk_widget_set_hexpand(inf, TRUE);
		GtkWidget *t = gtk_label_new("Current Timezone");
		gtk_widget_add_css_class(t, "title-4");
		gtk_widget_set_halign(t, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(inf), t);
		GtkWidget *v = gtk_label_new(cur_tz[0] ? cur_tz : "Unknown");
		gtk_widget_add_css_class(v, "dim-label");
		gtk_widget_set_halign(v, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(inf), v);
		gtk_box_append(GTK_BOX(row), inf);
		gtk_box_append(GTK_BOX(tz_inner), row);
		gtk_box_append(GTK_BOX(tz_inner), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	}

	/* searchable timezone dropdown */
	{
		char *tz_raw = run_cmd_str("timedatectl list-timezones 2>/dev/null");
		char **tz_lines = g_strsplit(tz_raw, "\n", -1);
		g_free(tz_raw);
		GtkStringList *tz_model = gtk_string_list_new(NULL);
		guint tz_cur_idx = 0, tz_i = 0;
		for (int i = 0; tz_lines[i] && tz_lines[i][0]; i++) {
			gtk_string_list_append(tz_model, tz_lines[i]);
			if (strcmp(tz_lines[i], cur_tz) == 0) tz_cur_idx = tz_i;
			tz_i++;
		}
		g_strfreev(tz_lines);

		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		gtk_widget_set_margin_start(row, 16); gtk_widget_set_margin_end(row, 16);
		gtk_widget_set_margin_top(row, 10);   gtk_widget_set_margin_bottom(row, 10);
		GtkWidget *lbl = gtk_label_new("Select Timezone");
		gtk_widget_add_css_class(lbl, "dim-label");
		gtk_widget_set_hexpand(lbl, TRUE);
		gtk_widget_set_halign(lbl, GTK_ALIGN_START);
		gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(row), lbl);

		GtkWidget *tz_dd = gtk_drop_down_new(G_LIST_MODEL(tz_model), NULL);
		gtk_drop_down_set_enable_search(GTK_DROP_DOWN(tz_dd), TRUE);
		gtk_drop_down_set_selected(GTK_DROP_DOWN(tz_dd), tz_cur_idx);
		gtk_widget_set_size_request(tz_dd, 260, -1);
		gtk_box_append(GTK_BOX(row), tz_dd);

		GtkWidget *tz_apply = gtk_button_new_with_label("Apply");
		gtk_widget_add_css_class(tz_apply, "suggested-action");
		g_signal_connect(tz_apply, "clicked", G_CALLBACK(dt_apply_tz), tz_dd);
		gtk_box_append(GTK_BOX(row), tz_apply);
		gtk_box_append(GTK_BOX(tz_inner), row);
	}
	g_free(cur_tz);
	gtk_box_append(GTK_BOX(vb), tz_frame);

	/* ======================================================
	 * INPUT SOURCES section
	 * ====================================================== */
	GtkWidget *inp_frame = make_bat_box("Input Sources");
	GtkWidget *inp_inner = g_object_get_data(G_OBJECT(inp_frame), "inner-box");

	char *kb_layout  = run_cmd_str(
				       "setxkbmap -query 2>/dev/null | grep '^layout' | awk '{print $2}' | tr -d '\n'");
	char *kb_variant = run_cmd_str(
				       "setxkbmap -query 2>/dev/null | grep '^variant' | awk '{print $2}' | tr -d '\n'");
	g_strstrip(kb_layout); g_strstrip(kb_variant);

	char kb_display[128];
	if (kb_variant && kb_variant[0])
		snprintf(kb_display, sizeof(kb_display), "%s (%s)",
			 kb_layout[0] ? kb_layout : "us", kb_variant);
	else
		snprintf(kb_display, sizeof(kb_display), "%s",
			 kb_layout[0] ? kb_layout : "us");

	GtkWidget *kb_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
	gtk_widget_set_margin_start(kb_row, 16); gtk_widget_set_margin_end(kb_row, 16);
	gtk_widget_set_margin_top(kb_row, 14);   gtk_widget_set_margin_bottom(kb_row, 14);
	GtkWidget *kb_ico = gtk_image_new_from_icon_name("input-keyboard-symbolic");
	gtk_image_set_pixel_size(GTK_IMAGE(kb_ico), 32);
	gtk_widget_set_valign(kb_ico, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(kb_row), kb_ico);
	GtkWidget *kb_inf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_widget_set_hexpand(kb_inf, TRUE);
	GtkWidget *kb_t = gtk_label_new("Keyboard Layout");
	gtk_widget_add_css_class(kb_t, "title-4");
	gtk_widget_set_halign(kb_t, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(kb_inf), kb_t);
	GtkWidget *kb_v = gtk_label_new(kb_display);
	gtk_widget_add_css_class(kb_v, "dim-label");
	gtk_widget_set_halign(kb_v, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(kb_inf), kb_v);
	gtk_box_append(GTK_BOX(kb_row), kb_inf);
	GtkWidget *kb_btn = gtk_button_new_with_label("Keyboard Settings \xe2\x86\x92");
	gtk_widget_add_css_class(kb_btn, "flat");
	gtk_widget_set_valign(kb_btn, GTK_ALIGN_CENTER);
	g_signal_connect(kb_btn, "clicked", G_CALLBACK(region_goto_keyboard), NULL);
	gtk_box_append(GTK_BOX(kb_row), kb_btn);
	gtk_box_append(GTK_BOX(inp_inner), kb_row);
	g_free(kb_layout); g_free(kb_variant);
	gtk_box_append(GTK_BOX(vb), inp_frame);

	g_free(cur_lang); g_free(cur_fmt);

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr), vb);
	gtk_box_append(GTK_BOX(root), scr);
	return root;
}


/* ================================================================== */
/* Applications                                                         */
/* ================================================================== */
static void setup_listitem_cb(GtkListItemFactory *f, GtkListItem *item, gpointer ud) {
	GtkWidget *box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
	gtk_widget_set_margin_start(box,12); gtk_widget_set_margin_end(box,12);
	gtk_widget_set_margin_top(box,8);    gtk_widget_set_margin_bottom(box,8);
	GtkWidget *icon=gtk_image_new(); gtk_image_set_pixel_size(GTK_IMAGE(icon),32);
	g_object_set_data(G_OBJECT(box),"icon",icon); gtk_box_append(GTK_BOX(box),icon);
	GtkWidget *vb=gtk_box_new(GTK_ORIENTATION_VERTICAL,2); gtk_widget_set_hexpand(vb,TRUE);
	GtkWidget *nl=gtk_label_new(NULL);
	gtk_widget_set_halign(nl,GTK_ALIGN_START); gtk_widget_add_css_class(nl,"body");
	g_object_set_data(G_OBJECT(box),"name",nl); gtk_box_append(GTK_BOX(vb),nl);
	GtkWidget *dl=gtk_label_new(NULL);
	gtk_widget_set_halign(dl,GTK_ALIGN_START);
	gtk_widget_add_css_class(dl,"dim-label"); gtk_widget_add_css_class(dl,"caption");
	gtk_label_set_ellipsize(GTK_LABEL(dl),PANGO_ELLIPSIZE_END);
	g_object_set_data(G_OBJECT(box),"desc",dl); gtk_box_append(GTK_BOX(vb),dl);
	gtk_box_append(GTK_BOX(box),vb);
	gtk_list_item_set_child(item,box);
}
static void bind_listitem_cb(GtkListItemFactory *f, GtkListItem *item, gpointer ud) {
	GtkWidget *box=gtk_list_item_get_child(item);
	GAppInfo  *info=gtk_list_item_get_item(item);
	if(!info)return;
	GtkWidget *iw=g_object_get_data(G_OBJECT(box),"icon");
	GtkWidget *nw=g_object_get_data(G_OBJECT(box),"name");
	GtkWidget *dw=g_object_get_data(G_OBJECT(box),"desc");
	GIcon *gi=g_app_info_get_icon(info);
	if(gi) gtk_image_set_from_gicon(GTK_IMAGE(iw),gi);
	else   gtk_image_set_from_icon_name(GTK_IMAGE(iw),"application-x-executable");
	gtk_label_set_text(GTK_LABEL(nw),g_app_info_get_name(info));
	const char *desc=g_app_info_get_description(info);
	gtk_label_set_text(GTK_LABEL(dw),desc?desc:"");
}
static void app_activate_cb(GtkListView *lv, guint pos, gpointer ud) {
	GListModel *model=G_LIST_MODEL(gtk_single_selection_get_model(
								      GTK_SINGLE_SELECTION(gtk_list_view_get_model(lv))));
	GAppInfo *info=g_list_model_get_item(model,pos);
	if(info){g_app_info_launch(info,NULL,NULL,NULL);g_object_unref(info);}
}
static GListModel *create_application_list(void) {
	GListStore *store=g_list_store_new(G_TYPE_APP_INFO);
	GList *apps=g_app_info_get_all();
	for(GList *l=apps;l;l=l->next)
		if(g_app_info_should_show(l->data)) g_list_store_append(store,l->data);
	g_list_free_full(apps,g_object_unref);
	return G_LIST_MODEL(store);
}

GtkWidget *applications_settings(void) {
	GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	gtk_widget_set_hexpand(root,TRUE); gtk_widget_set_vexpand(root,TRUE);
	gtk_box_append(GTK_BOX(root),make_page_header("preferences-system-symbolic","Applications"));
	gtk_box_append(GTK_BOX(root),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkListItemFactory *factory=gtk_signal_list_item_factory_new();
	g_signal_connect(factory,"setup",G_CALLBACK(setup_listitem_cb),NULL);
	g_signal_connect(factory,"bind", G_CALLBACK(bind_listitem_cb), NULL);
	GListModel *model=create_application_list();
	GtkWidget *list=gtk_list_view_new(
					  GTK_SELECTION_MODEL(gtk_single_selection_new(model)),factory);
	gtk_widget_add_css_class(list,"navigation-sidebar");
	g_signal_connect(list,"activate",G_CALLBACK(app_activate_cb),NULL);
	GtkWidget *sw=gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(sw,TRUE);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw),list);
	gtk_box_append(GTK_BOX(root),sw);
	return root;
}

/* ================================================================== */
/* About                                                                */
/* ================================================================== */
GtkWidget *about_settings(void) {
	GtkWidget *root=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
	gtk_widget_set_hexpand(root,TRUE); gtk_widget_set_vexpand(root,TRUE);
	GtkWidget *scr=gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr,TRUE);
	GtkWidget *vb=gtk_box_new(GTK_ORIENTATION_VERTICAL,20);
	gtk_widget_set_halign(vb,GTK_ALIGN_CENTER);
	gtk_widget_set_margin_start(vb,40); gtk_widget_set_margin_end(vb,40);
	gtk_widget_set_margin_top(vb,32);   gtk_widget_set_margin_bottom(vb,32);
	/* fsociety logo */
	const char *logo_path="/usr/share/mrrobotos/mrrobotos.png";
	GtkWidget *logo;
	if (g_file_test(logo_path,G_FILE_TEST_EXISTS)) {
		GFile *_gf = g_file_new_for_path(logo_path);
		logo = gtk_picture_new_for_file(_gf);
		g_object_unref(_gf);
		gtk_picture_set_content_fit(GTK_PICTURE(logo), GTK_CONTENT_FIT_CONTAIN);
		gtk_widget_set_size_request(logo, 400, 400);
		gtk_widget_set_halign(logo, GTK_ALIGN_CENTER);
	} else {
		logo=gtk_image_new_from_icon_name("computer-symbolic");
		gtk_image_set_pixel_size(GTK_IMAGE(logo),140);
	}
	gtk_widget_set_halign(logo,GTK_ALIGN_CENTER); gtk_box_append(GTK_BOX(vb),logo);
	/* OS name */
	GtkWidget *os=gtk_label_new("MrRobotOS");
	gtk_widget_add_css_class(os,"title-1"); gtk_widget_set_halign(os,GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(vb),os);
	/* kernel */
	struct utsname un; uname(&un);
	char kver[256]; snprintf(kver,sizeof(kver),"Kernel %s",un.release);
	GtkWidget *kl=gtk_label_new(kver);
	gtk_widget_add_css_class(kl,"dim-label"); gtk_widget_set_halign(kl,GTK_ALIGN_CENTER);
	gtk_widget_set_margin_bottom(kl,8); gtk_box_append(GTK_BOX(vb),kl);
	gtk_box_append(GTK_BOX(vb),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	/* hw info */
	GtkWidget *hw=gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(hw),GTK_SELECTION_NONE);
	gtk_widget_add_css_class(hw,"boxed-list");
	gtk_widget_set_hexpand(hw,TRUE); gtk_widget_set_size_request(hw,500,-1);
	char hostname[256]=""; gethostname(hostname,sizeof(hostname));
	struct { const char *label; char *value; } rows[]={
		{"Hostname", g_strdup(hostname[0]?hostname:un.nodename)},
		{"CPU",      run_cmd_str("grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2 | sed 's/^ *//' | tr -d '\\n'")},
		{"Memory",   run_cmd_str("awk '/MemTotal/{printf \"%.1f GB\",$2/1024/1024}' /proc/meminfo")},
		{"GPU",      run_cmd_str("lspci 2>/dev/null | grep -i 'vga\\|3d\\|display' | head -1 | sed 's/.*: //' | tr -d '\\n'")},
		{"Disk",     run_cmd_str("df -h / 2>/dev/null | awk 'NR==2{print $2\" total, \"$3\" used\"}' | tr -d '\\n'")},
		{"Architecture", g_strdup(un.machine)},
		{NULL,NULL}
	};
	for (int i=0;rows[i].label;i++) {
		g_strstrip(rows[i].value);
		GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
		gtk_widget_set_margin_start(row,16); gtk_widget_set_margin_end(row,16);
		gtk_widget_set_margin_top(row,10);   gtk_widget_set_margin_bottom(row,10);
		GtkWidget *k=gtk_label_new(rows[i].label);
		gtk_widget_set_hexpand(k,TRUE); gtk_widget_set_halign(k,GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(row),k);
		GtkWidget *v=gtk_label_new(rows[i].value&&rows[i].value[0]?rows[i].value:"—");
		gtk_widget_add_css_class(v,"dim-label");
		gtk_label_set_ellipsize(GTK_LABEL(v),PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(v),40);
		gtk_widget_set_halign(v,GTK_ALIGN_END);
		gtk_box_append(GTK_BOX(row),v);
		gtk_list_box_append(GTK_LIST_BOX(hw),row);
		g_free(rows[i].value);
	}
	gtk_box_append(GTK_BOX(vb),hw);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr),vb);
	gtk_box_append(GTK_BOX(root),scr);
	return root;
}

/* ================================================================== */
/* Sidebar helpers                                                      */
/* ================================================================== */

/* ================================================================== */
/* VPN Settings                                                         */
/* ================================================================== */
typedef struct {
	GtkWidget *conn_list;
	guint      refresh_id;
	gboolean   destroyed;
} VpnData;

typedef struct {
	char    name[256];
	VpnData *vd;
} VpnActionData;

static gboolean vpn_refresh_once(gpointer ud);

static gpointer vpn_action_thread(gpointer ud) {
	VpnActionData *ad = ud;
	/* ud->name holds the full shell command */
	system(ad->name);
	if (!ad->vd->destroyed)
		g_idle_add(vpn_refresh_once, ad->vd);
	g_free(ad);
	return NULL;
}
static void vpn_run_async(const char *cmd, VpnData *vd) {
	VpnActionData *ad = g_new0(VpnActionData, 1);
	strncpy(ad->name, cmd, sizeof(ad->name) - 1);
	ad->vd = vd;
	g_thread_unref(g_thread_new("vpn", vpn_action_thread, ad));
}

static void vpn_toggle(GtkWidget *btn, gpointer ud) {
	const char *cmd = g_object_get_data(G_OBJECT(btn), "vpn-cmd");
	VpnData    *vd  = g_object_get_data(G_OBJECT(btn), "vpn-data");
	if (!vd || vd->destroyed || !cmd) return;
	gtk_widget_set_sensitive(btn, FALSE);
	vpn_run_async(cmd, vd);
}

static void vpn_refresh_internal(VpnData *vd) {
	if (!vd || vd->destroyed) return;
	GtkWidget *c;
	while ((c = gtk_widget_get_first_child(vd->conn_list)))
		gtk_list_box_remove(GTK_LIST_BOX(vd->conn_list), c);

	/* list all VPN-type connections */
	char *raw = run_cmd_str(
				"nmcli -t -f NAME,TYPE con show 2>/dev/null | "
				"grep -iE ':vpn|:wireguard|:openvpn|:l2tp|:pptp'");
	char *active_raw = run_cmd_str(
				       "nmcli -t -f NAME,TYPE con show --active 2>/dev/null | "
				       "grep -iE ':vpn|:wireguard|:openvpn|:l2tp|:pptp' | cut -d: -f1");
	char **lines  = g_strsplit(raw,        "\n", -1);
	char **active = g_strsplit(active_raw, "\n", -1);
	g_free(raw); g_free(active_raw);

	int found = 0;
	for (int i = 0; lines[i] && lines[i][0]; i++) {
		char *line = g_strdup(lines[i]);
		/* NAME:TYPE */
		char *colon = strrchr(line, ':');
		if (colon) *colon = '\0';
		const char *name = line;
		const char *type = colon ? colon + 1 : "vpn";

		gboolean is_active = FALSE;
		for (int j = 0; active[j] && active[j][0]; j++)
			if (strcmp(active[j], name) == 0) { is_active = TRUE; break; }

		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
		gtk_widget_set_margin_start(row, 14); gtk_widget_set_margin_end(row, 14);
		gtk_widget_set_margin_top(row, 10);   gtk_widget_set_margin_bottom(row, 10);

		GtkWidget *ico = gtk_image_new_from_icon_name("network-vpn-symbolic");
		gtk_image_set_pixel_size(GTK_IMAGE(ico), 22);
		gtk_widget_set_valign(ico, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(row), ico);

		GtkWidget *inf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		gtk_widget_set_hexpand(inf, TRUE);
		GtkWidget *nl = gtk_label_new(name);
		gtk_widget_add_css_class(nl, "body");
		if (is_active) gtk_widget_add_css_class(nl, "wifi-active-name");
		gtk_widget_set_halign(nl, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(inf), nl);
		char sub[128];
		snprintf(sub, sizeof(sub), "%s  ·  %s", type, is_active ? "Connected" : "Disconnected");
		GtkWidget *sl = gtk_label_new(sub);
		gtk_widget_add_css_class(sl, "dim-label");
		gtk_widget_add_css_class(sl, "caption");
		gtk_widget_set_halign(sl, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(inf), sl);
		gtk_box_append(GTK_BOX(row), inf);

		char cmd[512];
		GtkWidget *btn;
		if (is_active) {
			snprintf(cmd, sizeof(cmd), "nmcli con down id '%s' 2>/dev/null", name);
			btn = gtk_button_new_with_label("Disconnect");
			gtk_widget_add_css_class(btn, "destructive-action");
		} else {
			snprintf(cmd, sizeof(cmd), "nmcli con up id '%s' 2>/dev/null", name);
			btn = gtk_button_new_with_label("Connect");
			gtk_widget_add_css_class(btn, "suggested-action");
		}
		gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
		gtk_widget_set_size_request(btn, 110, -1);
		g_object_set_data_full(G_OBJECT(btn), "vpn-cmd", g_strdup(cmd), g_free);
		g_object_set_data(G_OBJECT(btn), "vpn-data", vd);
		g_signal_connect(btn, "clicked", G_CALLBACK(vpn_toggle), NULL);
		gtk_box_append(GTK_BOX(row), btn);
		gtk_list_box_append(GTK_LIST_BOX(vd->conn_list), row);
		g_free(line);
		found++;
	}
	g_strfreev(lines); g_strfreev(active);

	if (found == 0) {
		GtkWidget *empty = gtk_label_new(
						 "No VPN connections configured.\n"
						 "Add one with nmcli or nm-connection-editor.");
		gtk_widget_add_css_class(empty, "dim-label");
		gtk_label_set_justify(GTK_LABEL(empty), GTK_JUSTIFY_CENTER);
		gtk_widget_set_margin_top(empty, 24); gtk_widget_set_margin_bottom(empty, 24);
		gtk_widget_set_halign(empty, GTK_ALIGN_CENTER);
		gtk_list_box_append(GTK_LIST_BOX(vd->conn_list), empty);
	}
}
static gboolean vpn_refresh_once(gpointer ud) {
	VpnData *vd = ud;
	if (!vd || vd->destroyed) return G_SOURCE_REMOVE;
	vpn_refresh_internal(vd); return G_SOURCE_REMOVE;
}
static gboolean vpn_auto_refresh(gpointer ud) {
	VpnData *vd = ud;
	if (!vd || vd->destroyed) return G_SOURCE_REMOVE;
	vpn_refresh_internal(vd); return G_SOURCE_CONTINUE;
}
static void vpn_page_destroyed(GtkWidget *w, gpointer ud) {
	VpnData *vd = ud;
	vd->destroyed = TRUE;
	if (vd->refresh_id) { g_source_remove(vd->refresh_id); vd->refresh_id = 0; }
	g_free(vd);
}

static GtkWidget *vpn_settings(void) {
	VpnData *vd = g_new0(VpnData, 1);
	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand(root, TRUE); gtk_widget_set_vexpand(root, TRUE);

	GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_start(hdr, 20); gtk_widget_set_margin_end(hdr, 20);
	gtk_widget_set_margin_top(hdr, 16);   gtk_widget_set_margin_bottom(hdr, 12);
	GtkWidget *ic = gtk_image_new_from_icon_name("network-vpn-symbolic");
	gtk_image_set_pixel_size(GTK_IMAGE(ic), 24);
	gtk_box_append(GTK_BOX(hdr), ic);
	GtkWidget *tl = gtk_label_new("VPN");
	gtk_widget_add_css_class(tl, "title-2");
	gtk_widget_set_hexpand(tl, TRUE); gtk_widget_set_halign(tl, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(hdr), tl);
	GtkWidget *ref = gtk_button_new_from_icon_name("view-refresh-symbolic");
	gtk_widget_add_css_class(ref, "flat"); gtk_widget_set_valign(ref, GTK_ALIGN_CENTER);
	g_signal_connect_swapped(ref, "clicked", G_CALLBACK(vpn_refresh_once), vd);
	gtk_box_append(GTK_BOX(hdr), ref);
	gtk_box_append(GTK_BOX(root), hdr);
	gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	GtkWidget *scr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr, TRUE);
	vd->conn_list = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(vd->conn_list), GTK_SELECTION_NONE);
	gtk_widget_add_css_class(vd->conn_list, "boxed-list");
	gtk_widget_set_margin_start(vd->conn_list, 16); gtk_widget_set_margin_end(vd->conn_list, 16);
	gtk_widget_set_margin_top(vd->conn_list, 12);   gtk_widget_set_margin_bottom(vd->conn_list, 12);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr), vd->conn_list);
	gtk_box_append(GTK_BOX(root), scr);

	vpn_refresh_internal(vd);
	vd->refresh_id = g_timeout_add_seconds(15, vpn_auto_refresh, vd);
	g_signal_connect(root, "destroy", G_CALLBACK(vpn_page_destroyed), vd);
	return root;
}

/* ================================================================== */
/* Notifications Settings (dunst)                                       */
/* ================================================================== */
static void notif_toggle_dnd(GtkSwitch *sw, GParamSpec *ps, gpointer ud) {
	if (gtk_switch_get_active(sw))
		system("dunstctl set-paused true 2>/dev/null");
	else
		system("dunstctl set-paused false 2>/dev/null");
}

static void notif_open_config(GtkWidget *btn, gpointer ud) {
	const char *path = ud;
	char cmd[1024];
	snprintf(cmd, sizeof(cmd),
		 "xterm -e \"${EDITOR:-nano} '%s'\" 2>/dev/null &", path);
	system(cmd);
}

static GtkWidget *notifications_settings(void) {
	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand(root, TRUE); gtk_widget_set_vexpand(root, TRUE);

	GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_start(hdr, 20); gtk_widget_set_margin_end(hdr, 20);
	gtk_widget_set_margin_top(hdr, 16);   gtk_widget_set_margin_bottom(hdr, 12);
	GtkWidget *ic = gtk_image_new_from_icon_name("preferences-system-notifications-symbolic");
	gtk_image_set_pixel_size(GTK_IMAGE(ic), 24); gtk_box_append(GTK_BOX(hdr), ic);
	GtkWidget *tl = gtk_label_new("Notifications");
	gtk_widget_add_css_class(tl, "title-2");
	gtk_widget_set_hexpand(tl, TRUE); gtk_widget_set_halign(tl, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(hdr), tl);
	gtk_box_append(GTK_BOX(root), hdr);
	gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	GtkWidget *scr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr, TRUE);
	GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
	gtk_widget_set_margin_start(vb, 16); gtk_widget_set_margin_end(vb, 16);
	gtk_widget_set_margin_top(vb, 16);   gtk_widget_set_margin_bottom(vb, 16);

	/* daemon status */
	char *is_running = run_cmd_str("pgrep -x dunst >/dev/null 2>&1 && echo yes || echo no");
	g_strstrip(is_running);
	gboolean dunst_up = strcmp(is_running, "yes") == 0;
	g_free(is_running);

	GtkWidget *sf = make_bat_box("Notification Daemon");
	GtkWidget *sb = g_object_get_data(G_OBJECT(sf), "inner-box");
	GtkWidget *sr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_start(sr, 14); gtk_widget_set_margin_end(sr, 14);
	gtk_widget_set_margin_top(sr, 12);   gtk_widget_set_margin_bottom(sr, 12);
	GtkWidget *sico = gtk_image_new_from_icon_name(
						       dunst_up ? "emblem-ok-symbolic" : "dialog-warning-symbolic");
	gtk_image_set_pixel_size(GTK_IMAGE(sico), 20);
	gtk_box_append(GTK_BOX(sr), sico);
	GtkWidget *sinf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2); gtk_widget_set_hexpand(sinf, TRUE);
	GtkWidget *dunst_name_lbl = gtk_label_new("dunst");
	gtk_widget_set_halign(dunst_name_lbl, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(sinf), dunst_name_lbl);
	GtkWidget *sst = gtk_label_new(dunst_up ? "Running" : "Not running — start from xinitrc");
	gtk_widget_add_css_class(sst, "dim-label"); gtk_widget_add_css_class(sst, "caption");
	gtk_widget_set_halign(sst, GTK_ALIGN_START); gtk_box_append(GTK_BOX(sinf), sst);
	gtk_box_append(GTK_BOX(sr), sinf); gtk_box_append(GTK_BOX(sb), sr);
	gtk_box_append(GTK_BOX(vb), sf);

	/* DND toggle */
	GtkWidget *df = make_bat_box("Do Not Disturb");
	GtkWidget *db = g_object_get_data(G_OBJECT(df), "inner-box");
	GtkWidget *dr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_start(dr, 14); gtk_widget_set_margin_end(dr, 14);
	gtk_widget_set_margin_top(dr, 10);   gtk_widget_set_margin_bottom(dr, 10);
	GtkWidget *dl = gtk_label_new("Pause all notifications");
	gtk_widget_set_hexpand(dl, TRUE); gtk_widget_set_halign(dl, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(dr), dl);
	char *paused_raw = run_cmd_str("dunstctl is-paused 2>/dev/null | tr -d '\\n'");
	g_strstrip(paused_raw);
	GtkWidget *dsw = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(dsw), strcmp(paused_raw, "true") == 0);
	gtk_widget_set_valign(dsw, GTK_ALIGN_CENTER);
	gtk_widget_set_sensitive(dsw, dunst_up);
	g_free(paused_raw);
	g_signal_connect(dsw, "notify::active", G_CALLBACK(notif_toggle_dnd), NULL);
	gtk_box_append(GTK_BOX(dr), dsw); gtk_box_append(GTK_BOX(db), dr);
	gtk_box_append(GTK_BOX(vb), df);

	/* Config file */
	char *home = get_home_env();
	char *cfg_path = g_strdup_printf("%s/.config/dunst/dunstrc", home ? home : "/root");
	free(home);
	gboolean cfg_exists = g_file_test(cfg_path, G_FILE_TEST_EXISTS);

	GtkWidget *cf = make_bat_box("Configuration File");
	GtkWidget *cb = g_object_get_data(G_OBJECT(cf), "inner-box");
	GtkWidget *cr2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_start(cr2, 14); gtk_widget_set_margin_end(cr2, 14);
	gtk_widget_set_margin_top(cr2, 10);   gtk_widget_set_margin_bottom(cr2, 10);
	GtkWidget *ci = gtk_image_new_from_icon_name("text-x-generic-symbolic");
	gtk_image_set_pixel_size(GTK_IMAGE(ci), 20); gtk_widget_set_valign(ci, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(cr2), ci);
	GtkWidget *cinf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2); gtk_widget_set_hexpand(cinf, TRUE);
	GtkWidget *cn = gtk_label_new("dunstrc");
	gtk_widget_set_halign(cn, GTK_ALIGN_START); gtk_box_append(GTK_BOX(cinf), cn);
	GtkWidget *cp = gtk_label_new(cfg_path);
	gtk_widget_add_css_class(cp, "dim-label"); gtk_widget_add_css_class(cp, "caption");
	gtk_widget_add_css_class(cp, "monospace");
	gtk_label_set_ellipsize(GTK_LABEL(cp), PANGO_ELLIPSIZE_START);
	gtk_widget_set_halign(cp, GTK_ALIGN_START); gtk_box_append(GTK_BOX(cinf), cp);
	gtk_box_append(GTK_BOX(cr2), cinf);
	GtkWidget *eb = gtk_button_new_with_label(cfg_exists ? "Edit" : "Create");
	gtk_widget_add_css_class(eb, cfg_exists ? "flat" : "suggested-action");
	gtk_widget_set_valign(eb, GTK_ALIGN_CENTER);
	/* store path as widget data so it's freed with the widget */
	/* cfg_path is owned by loc_model or g_strdup - pass directly to signal */
	/* g_object_set_data_full takes ownership for cleanup */
	//char *cfg_path_copy = g_strdup(cfg_path);
	//g_object_set_data_full(G_OBJECT(eb), "cfg-path", cfg_path, g_free);
	//g_signal_connect(eb, "clicked", G_CALLBACK(notif_open_config), cfg_path_copy);
	/* cfg_path_copy will leak but it's static for app lifetime - acceptable */
	g_object_set_data_full(G_OBJECT(eb), "cfg-path", cfg_path, g_free);
	g_signal_connect(eb, "clicked", G_CALLBACK(notif_open_config), g_object_get_data(G_OBJECT(eb), "cfg-path"));
	gtk_box_append(GTK_BOX(cr2), eb);
	gtk_box_append(GTK_BOX(cb), cr2);

	/* key values from existing config */
	if (cfg_exists) {
		gtk_box_append(GTK_BOX(cb), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
		const char *keys[][2] = {
			{ "timeout",    "Timeout"    },
			{ "font",       "Font"       },
			{ "background", "Background" },
			{ "foreground", "Foreground" },
			{ "geometry",   "Geometry"   },
			{ NULL, NULL }
		};
		for (int k = 0; keys[k][0]; k++) {
			char cmd2[512];
			snprintf(cmd2, sizeof(cmd2),
				 "grep -m1 '^    %s ' '%s' 2>/dev/null | "
				 "cut -d= -f2 | sed 's/^ *//' | tr -d '\\n'",
				 keys[k][0], cfg_path);
			char *val = run_cmd_str(cmd2);
			g_strstrip(val);
			if (!val || !val[0]) { g_free(val); continue; }
			GtkWidget *kr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
			gtk_widget_set_margin_start(kr, 14); gtk_widget_set_margin_end(kr, 14);
			gtk_widget_set_margin_top(kr, 8);    gtk_widget_set_margin_bottom(kr, 8);
			GtkWidget *kl = gtk_label_new(keys[k][1]);
			gtk_widget_add_css_class(kl, "dim-label");
			gtk_widget_set_hexpand(kl, TRUE); gtk_widget_set_halign(kl, GTK_ALIGN_START);
			gtk_box_append(GTK_BOX(kr), kl);
			GtkWidget *vw = gtk_label_new(val);
			gtk_widget_add_css_class(vw, "monospace");
			gtk_label_set_ellipsize(GTK_LABEL(vw), PANGO_ELLIPSIZE_END);
			gtk_widget_set_halign(vw, GTK_ALIGN_END);
			gtk_box_append(GTK_BOX(kr), vw);
			gtk_box_append(GTK_BOX(cb), kr);
			gtk_box_append(GTK_BOX(cb), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
			g_free(val);
		}
	}
	gtk_box_append(GTK_BOX(vb), cf);

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr), vb);
	gtk_box_append(GTK_BOX(root), scr);
	return root;
}

/* ================================================================== */
/* Software & Updates (pacman)                                          */
/* ================================================================== */
typedef struct {
	GtkWidget *update_list;
	GtkWidget *status_label;
	GtkWidget *count_label;
	GtkWidget *refresh_btn;
	GtkWidget *update_btn;
	guint      check_id;
	gboolean   destroyed;
	gboolean   checking;
} UpdateData;

typedef struct {
	UpdateData  *ud;
	char       **packages;
	int          n;
} UpdateResult;

static gboolean updates_apply_result(gpointer p) {
	UpdateResult *res = p;
	UpdateData   *ud  = res->ud;
	if (ud->destroyed) goto done;

	ud->checking = FALSE;
	gtk_widget_set_sensitive(ud->refresh_btn, TRUE);

	GtkWidget *c;
	while ((c = gtk_widget_get_first_child(ud->update_list)))
		gtk_list_box_remove(GTK_LIST_BOX(ud->update_list), c);

	if (res->n == 0) {
		gtk_label_set_text(GTK_LABEL(ud->status_label), "System is up to date.");
		gtk_label_set_text(GTK_LABEL(ud->count_label),  "0 updates available");
		gtk_widget_set_sensitive(ud->update_btn, FALSE);
		GtkWidget *empty = gtk_label_new("✓  No updates available");
		gtk_widget_add_css_class(empty, "dim-label");
		gtk_widget_set_margin_top(empty, 28);
		gtk_widget_set_halign(empty, GTK_ALIGN_CENTER);
		gtk_list_box_append(GTK_LIST_BOX(ud->update_list), empty);
	} else {
		char s[64];
		snprintf(s, sizeof(s), "%d update%s available", res->n, res->n == 1 ? "" : "s");
		gtk_label_set_text(GTK_LABEL(ud->status_label), s);
		gtk_label_set_text(GTK_LABEL(ud->count_label),  s);
		gtk_widget_set_sensitive(ud->update_btn, TRUE);

		for (int i = 0; i < res->n; i++) {
			GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
			gtk_widget_set_margin_start(row, 14); gtk_widget_set_margin_end(row, 14);
			gtk_widget_set_margin_top(row, 7);    gtk_widget_set_margin_bottom(row, 7);
			GtkWidget *ico = gtk_image_new_from_icon_name("software-update-available-symbolic");
			gtk_image_set_pixel_size(GTK_IMAGE(ico), 16);
			gtk_widget_set_valign(ico, GTK_ALIGN_CENTER);
			gtk_box_append(GTK_BOX(row), ico);
			/* "pkgname old -> new" */
			char *line = g_strdup(res->packages[i]);
			char *sp = strchr(line, ' ');
			const char *pkgname = line;
			const char *version = "";
			if (sp) { *sp = '\0'; version = sp + 1; }
			GtkWidget *inf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
			gtk_widget_set_hexpand(inf, TRUE);
			GtkWidget *nl = gtk_label_new(pkgname);
			gtk_widget_add_css_class(nl, "body");
			gtk_widget_set_halign(nl, GTK_ALIGN_START);
			gtk_box_append(GTK_BOX(inf), nl);
			if (version[0]) {
				GtkWidget *vl = gtk_label_new(version);
				gtk_widget_add_css_class(vl, "dim-label");
				gtk_widget_add_css_class(vl, "caption");
				gtk_widget_set_halign(vl, GTK_ALIGN_START);
				gtk_box_append(GTK_BOX(inf), vl);
			}
			gtk_box_append(GTK_BOX(row), inf);

			/* per-package update button */
			GtkWidget *upd_btn = gtk_button_new_with_label("Update");
			gtk_widget_add_css_class(upd_btn, "flat");
			gtk_widget_set_valign(upd_btn, GTK_ALIGN_CENTER);
			gtk_widget_set_size_request(upd_btn, 72, -1);
			/* store pkg name - line was split so pkgname points into line */
			g_object_set_data_full(G_OBJECT(upd_btn), "pkg-name",
					       g_strdup(pkgname), g_free);
			g_signal_connect(upd_btn, "clicked",
					 G_CALLBACK(updates_apply_single), NULL);
			gtk_box_append(GTK_BOX(row), upd_btn);

			gtk_list_box_append(GTK_LIST_BOX(ud->update_list), row);
			g_free(line);
		}
	}
done:
	if (res->packages) g_strfreev(res->packages);
	g_free(res);
	return G_SOURCE_REMOVE;
}

static gpointer updates_check_thread(gpointer p) {
	UpdateData *ud = p;
	/* checkupdates from pacman-contrib is safest (no db lock) */
	char *raw = run_cmd_str("checkupdates 2>/dev/null");
	if (!raw || !raw[0]) {
		g_free(raw);
		raw = run_cmd_str("pacman -Qu 2>/dev/null");
	}
	UpdateResult *res = g_new0(UpdateResult, 1);
	res->ud = ud;
	if (raw && raw[0]) {
		char **lines = g_strsplit(raw, "\n", -1);
		int n = 0;
		for (int i = 0; lines[i] && lines[i][0]; i++) n++;
		res->packages = lines;
		res->n = n;
	}
	g_free(raw);
	g_idle_add(updates_apply_result, res);
	return NULL;
}

static void updates_start_check(GtkWidget *btn, gpointer ud) {
	UpdateData *udd = ud;
	if (udd->checking) return;
	udd->checking = TRUE;
	gtk_widget_set_sensitive(udd->refresh_btn, FALSE);
	gtk_label_set_text(GTK_LABEL(udd->status_label), "Checking for updates…");
	GtkWidget *c;
	while ((c = gtk_widget_get_first_child(udd->update_list)))
		gtk_list_box_remove(GTK_LIST_BOX(udd->update_list), c);
	GtkWidget *wl = gtk_label_new("Checking…");
	gtk_widget_add_css_class(wl, "dim-label");
	gtk_widget_set_margin_top(wl, 28);
	gtk_widget_set_halign(wl, GTK_ALIGN_CENTER);
	gtk_list_box_append(GTK_LIST_BOX(udd->update_list), wl);
	g_thread_unref(g_thread_new("pacman-check", updates_check_thread, udd));
}

static void updates_apply_single(GtkWidget *btn, gpointer ud) {
	const char *pkg = g_object_get_data(G_OBJECT(btn), "pkg-name");
	if (!pkg || !pkg[0]) return;
	char cmd[512];
	snprintf(cmd, sizeof(cmd),
		 "xterm -e 'sudo pacman -S --noconfirm %s; echo; echo Done. Press Enter.; read' &",
		 pkg);
	system(cmd);
}

static void updates_apply_all(GtkWidget *btn, gpointer ud) {
	system("xterm -e 'sudo pacman -Syu; echo; echo Done. Press Enter.; read' &");
}

static gboolean updates_auto_check(gpointer ud) {
	UpdateData *udd = ud;
	if (!udd || udd->destroyed) return G_SOURCE_REMOVE;
	if (!udd->checking) updates_start_check(NULL, udd);
	return G_SOURCE_CONTINUE;
}

static void updates_page_destroyed(GtkWidget *w, gpointer ud) {
	UpdateData *udd = ud;
	udd->destroyed = TRUE;
	if (udd->check_id) { g_source_remove(udd->check_id); udd->check_id = 0; }
	g_free(udd);
}

static GtkWidget *updates_settings(void) {
	UpdateData *ud = g_new0(UpdateData, 1);
	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand(root, TRUE); gtk_widget_set_vexpand(root, TRUE);

	GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_start(hdr, 20); gtk_widget_set_margin_end(hdr, 20);
	gtk_widget_set_margin_top(hdr, 16);   gtk_widget_set_margin_bottom(hdr, 12);
	GtkWidget *ic = gtk_image_new_from_icon_name("software-update-available-symbolic");
	gtk_image_set_pixel_size(GTK_IMAGE(ic), 24); gtk_box_append(GTK_BOX(hdr), ic);
	GtkWidget *tl = gtk_label_new("Software & Updates");
	gtk_widget_add_css_class(tl, "title-2");
	gtk_widget_set_hexpand(tl, TRUE); gtk_widget_set_halign(tl, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(hdr), tl);
	GtkWidget *ref = gtk_button_new_from_icon_name("view-refresh-symbolic");
	gtk_widget_add_css_class(ref, "flat"); gtk_widget_set_valign(ref, GTK_ALIGN_CENTER);
	gtk_widget_set_tooltip_text(ref, "Check for updates");
	g_signal_connect(ref, "clicked", G_CALLBACK(updates_start_check), ud);
	gtk_box_append(GTK_BOX(hdr), ref);
	ud->refresh_btn = ref;
	gtk_box_append(GTK_BOX(root), hdr);

	ud->status_label = gtk_label_new("Not checked yet");
	gtk_widget_add_css_class(ud->status_label, "dim-label");
	gtk_widget_set_halign(ud->status_label, GTK_ALIGN_START);
	gtk_widget_set_margin_start(ud->status_label, 20);
	gtk_widget_set_margin_bottom(ud->status_label, 8);
	gtk_box_append(GTK_BOX(root), ud->status_label);
	gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	/* toolbar */
	GtkWidget *tb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_start(tb, 16); gtk_widget_set_margin_end(tb, 16);
	gtk_widget_set_margin_top(tb, 10);   gtk_widget_set_margin_bottom(tb, 6);
	ud->count_label = gtk_label_new("");
	gtk_widget_add_css_class(ud->count_label, "dim-label");
	gtk_widget_set_hexpand(ud->count_label, TRUE);
	gtk_widget_set_halign(ud->count_label, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(tb), ud->count_label);
	ud->update_btn = gtk_button_new_with_label("Update All");
	gtk_widget_add_css_class(ud->update_btn, "suggested-action");
	gtk_widget_set_sensitive(ud->update_btn, FALSE);
	g_signal_connect(ud->update_btn, "clicked", G_CALLBACK(updates_apply_all), ud);
	gtk_box_append(GTK_BOX(tb), ud->update_btn);
	gtk_box_append(GTK_BOX(root), tb);

	GtkWidget *scr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr, TRUE);
	ud->update_list = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(ud->update_list), GTK_SELECTION_NONE);
	gtk_widget_add_css_class(ud->update_list, "boxed-list");
	gtk_widget_set_margin_start(ud->update_list, 16); gtk_widget_set_margin_end(ud->update_list, 16);
	gtk_widget_set_margin_top(ud->update_list, 4);    gtk_widget_set_margin_bottom(ud->update_list, 12);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr), ud->update_list);
	gtk_box_append(GTK_BOX(root), scr);

	updates_start_check(NULL, ud);
	//g_timeout_add(100, (GSourceFunc)updates_start_check, ud);
	ud->check_id = g_timeout_add_seconds(1800, updates_auto_check, ud);
	g_signal_connect(root, "destroy", G_CALLBACK(updates_page_destroyed), ud);
	return root;
}

static GtkWidget *sharing_settings(void) {
	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand(root, TRUE); gtk_widget_set_vexpand(root, TRUE);
	gtk_box_append(GTK_BOX(root), make_page_header("preferences-system-sharing-symbolic", "Sharing"));
	gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	GtkWidget *scr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr, TRUE);
	GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
	gtk_widget_set_margin_start(content, 24); gtk_widget_set_margin_end(content, 24);
	gtk_widget_set_margin_top(content, 24);   gtk_widget_set_margin_bottom(content, 24);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr), content);
	gtk_box_append(GTK_BOX(root), scr);

	/* Coming soon banner */
	GtkWidget *banner = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
	gtk_widget_add_css_class(banner, "bat-box");
	gtk_widget_set_margin_bottom(banner, 8);
	GtkWidget *banner_inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_start(banner_inner, 20); gtk_widget_set_margin_end(banner_inner, 20);
	gtk_widget_set_margin_top(banner_inner, 20);   gtk_widget_set_margin_bottom(banner_inner, 20);
	gtk_widget_set_hexpand(banner_inner, TRUE);

	GtkWidget *ico = gtk_image_new_from_icon_name("network-server-symbolic");
	gtk_image_set_pixel_size(GTK_IMAGE(ico), 48);
	gtk_widget_set_halign(ico, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(banner_inner), ico);

	GtkWidget *title = gtk_label_new("Coming Soon — Cloud & Storage Integration");
	gtk_widget_add_css_class(title, "title-2");
	gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
	gtk_label_set_wrap(GTK_LABEL(title), TRUE);
	gtk_label_set_justify(GTK_LABEL(title), GTK_JUSTIFY_CENTER);
	gtk_box_append(GTK_BOX(banner_inner), title);

	gtk_box_append(GTK_BOX(banner), banner_inner);
	gtk_box_append(GTK_BOX(content), banner);

	/* Feature cards */
	struct { const char *icon; const char *title; const char *desc; } features[] = {
		{
			"computer-symbolic",
			"Cloud Integration",
			"Manage VMs, containers, remote services and databases directly from the MrRobotOS desktop."
		},
		{
			"drive-multidisk-symbolic",
			"Storage Replication",
			"Built-in replication across local and remote targets, designed for Proxmox and VMware backends."
		},
		{
			"network-workgroup-symbolic",
			"Distributed Storage",
			"Redundancy and high availability across nodes."
		},
		{
			"document-save-symbolic",
			"Backup & Snapshot Management",
			"Scheduled snapshots and offsite backup from the desktop environment."
		},
		{ NULL, NULL, NULL }
	};

	for (int i = 0; features[i].title; i++) {
		GtkWidget *frame = make_section_box(NULL);
		GtkWidget *inner = g_object_get_data(G_OBJECT(frame), "inner-box");

		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
		gtk_widget_set_margin_start(row, 16); gtk_widget_set_margin_end(row, 16);
		gtk_widget_set_margin_top(row, 16);   gtk_widget_set_margin_bottom(row, 16);

		GtkWidget *fic = gtk_image_new_from_icon_name(features[i].icon);
		gtk_image_set_pixel_size(GTK_IMAGE(fic), 32);
		gtk_widget_set_valign(fic, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(row), fic);

		GtkWidget *inf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
		gtk_widget_set_hexpand(inf, TRUE);

		GtkWidget *tl = gtk_label_new(features[i].title);
		gtk_widget_add_css_class(tl, "title-4");
		gtk_widget_set_halign(tl, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(inf), tl);

		GtkWidget *dl = gtk_label_new(features[i].desc);
		gtk_widget_add_css_class(dl, "dim-label");
		gtk_widget_set_halign(dl, GTK_ALIGN_START);
		gtk_label_set_wrap(GTK_LABEL(dl), TRUE);
		gtk_box_append(GTK_BOX(inf), dl);

		GtkWidget *badge = gtk_label_new("Coming Soon");
		gtk_widget_add_css_class(badge, "caption");
		gtk_widget_set_halign(badge, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(inf), badge);

		gtk_box_append(GTK_BOX(row), inf);
		gtk_box_append(GTK_BOX(inner), row);
		gtk_box_append(GTK_BOX(content), frame);
	}

	return root;
}

/* ================================================================== */
/* Navigation — shared helpers                                          */
/* ================================================================== */
typedef struct { const char *keys; const char *action; const char *detail; } NavRow;

static void nav_render_table(GtkWidget *content, const NavRow *tbl) {
	GtkWidget *cur_box = NULL;
	for (int i = 0; !(tbl[i].keys == NULL && tbl[i].action == NULL); i++) {
		if (tbl[i].keys == NULL) {
			GtkWidget *f = make_section_box(tbl[i].action);
			cur_box = g_object_get_data(G_OBJECT(f), "inner-box");
			gtk_box_append(GTK_BOX(content), f);
			continue;
		}

		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
		gtk_widget_set_margin_start(row, 16); gtk_widget_set_margin_end(row, 16);
		gtk_widget_set_margin_top(row, 10);   gtk_widget_set_margin_bottom(row, 10);

		/* key badges */
		GtkWidget *keys_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
		gtk_widget_set_size_request(keys_box, 320, -1);
		gtk_widget_set_halign(keys_box, GTK_ALIGN_START);
		gtk_widget_set_valign(keys_box, GTK_ALIGN_CENTER);

		char *keys_copy = g_strdup(tbl[i].keys);
		char *saveptr   = NULL;
		char *token     = strtok_r(keys_copy, "+", &saveptr);
		gboolean first  = TRUE;
		while (token) {
			while (*token == ' ') token++;
			char *end = token + strlen(token) - 1;
			while (end > token && *end == ' ') { *end = '\0'; end--; }
			if (!first) {
				GtkWidget *plus = gtk_label_new("+");
				gtk_widget_add_css_class(plus, "dim-label");
				gtk_widget_set_valign(plus, GTK_ALIGN_CENTER);
				gtk_box_append(GTK_BOX(keys_box), plus);
			}
			first = FALSE;
			GtkWidget *badge = gtk_label_new(token);
			gtk_widget_add_css_class(badge, "kbd-key");
			gtk_widget_set_valign(badge, GTK_ALIGN_CENTER);
			gtk_box_append(GTK_BOX(keys_box), badge);
			token = strtok_r(NULL, "+", &saveptr);
		}
		g_free(keys_copy);
		gtk_box_append(GTK_BOX(row), keys_box);

		/* action + detail */
		GtkWidget *inf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
		gtk_widget_set_hexpand(inf, TRUE);
		gtk_widget_set_valign(inf, GTK_ALIGN_CENTER);

		GtkWidget *a = gtk_label_new(tbl[i].action);
		gtk_widget_add_css_class(a, "nav-action");
		gtk_widget_set_halign(a, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(inf), a);

		if (tbl[i].detail && tbl[i].detail[0]) {
			GtkWidget *d = gtk_label_new(tbl[i].detail);
			gtk_widget_add_css_class(d, "nav-detail");
			gtk_widget_set_halign(d, GTK_ALIGN_START);
			gtk_label_set_wrap(GTK_LABEL(d), TRUE);
			gtk_box_append(GTK_BOX(inf), d);
		}

		gtk_box_append(GTK_BOX(row), inf);
		gtk_box_append(GTK_BOX(cur_box), row);
		if (tbl[i+1].keys != NULL || tbl[i+1].action == NULL)
			gtk_box_append(GTK_BOX(cur_box),
				       gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	}
}

static GtkWidget *nav_make_page(const char *icon, const char *title) {
	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand(root, TRUE);
	gtk_widget_set_vexpand(root, TRUE);
	gtk_box_append(GTK_BOX(root), make_page_header(icon, title));
	gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
	GtkWidget *scr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scr, TRUE);
	GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
	gtk_widget_set_margin_start(content, 24); gtk_widget_set_margin_end(content, 24);
	gtk_widget_set_margin_top(content, 20);   gtk_widget_set_margin_bottom(content, 24);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr), content);
	gtk_box_append(GTK_BOX(root), scr);
	g_object_set_data(G_OBJECT(root), "content", content);
	return root;
}


/* ================================================================== */
/* MRDWM Keys                                                         */
/* ================================================================== */
static GtkWidget *keybindings_settings(void) {
	GtkWidget *root    = nav_make_page("preferences-desktop-keyboard-shortcuts-symbolic", "Keybindings");
	GtkWidget *content = g_object_get_data(G_OBJECT(root), "content");
	static const NavRow tbl[] = {
		{ NULL, "Launcher & Terminal", NULL },
		{ "Alt + p",                  "Application Launcher",
			"Opens dmenu_run — type to search and launch any installed application" },
		{ "Alt + Shift + Return",     "Open Terminal",
			"Spawns a new st terminal window" },
		{ NULL, "Windows", NULL },
		{ "Alt + Shift + c",          "Close Window",
			"Kills the currently focused client window" },
		{ "Alt + Return",             "Zoom",
			"Swaps the focused window with the master area" },
		{ "Alt + s",                  "Show Hidden Window",
			"Reveals the next window that was hidden with Alt+Shift+h" },
		{ "Alt + Shift + s",          "Show All Hidden",
			"Reveals all windows that were previously hidden" },
		{ "Alt + Shift + h",          "Hide Window",
			"Hides the focused window from the current tag view" },
		{ "Alt + w",                  "Preview All Windows",
			"Shows a thumbnail grid of all open windows for quick switching" },
		{ NULL, "Focus", NULL },
		{ "Alt + j / k",              "Focus Visible Window",
			"Moves focus to the next or previous visible window in the stack" },
		{ "Alt + Shift + j / k",      "Focus Hidden Window",
			"Moves focus to the next or previous hidden window" },
		{ "Alt + Tab",                "Toggle Previous Tag",
			"Switches back to the previously viewed tag" },
		{ "Alt + , / .",              "Focus Monitor",
			"Moves focus to the previous or next connected monitor" },
		{ "Alt + Shift + , / .",      "Move to Monitor",
			"Sends the focused window to the previous or next monitor" },
		{ NULL, "Layouts", NULL },
		{ "Alt + t",                  "Tile  []=",
			"Default tiling layout — master on left, stack on right" },
		{ "Alt + f",                  "Floating  ><>",
			"All windows float freely — drag to move, resize with Alt+right drag" },
		{ "Alt + m",                  "Monocle  [M]",
			"Full screen layout — one window visible at a time, maximised" },
		{ "Alt + Shift + t",          "Three-Column  |||",
			"Splits the screen into three vertical columns" },
		{ "Alt + Shift + d",          "Deck  [D]",
			"Master on left, stack windows stacked on top of each other on right" },
		{ "Alt + r",                  "Spiral  [@]",
			"Fibonacci spiral layout — windows spiral inward" },
		{ "Alt + Shift + r",          "Dwindle  [\\]",
			"Dwindle layout — each new window takes half the remaining space" },
		{ "Alt + u",                  "Centered Master  |M|",
			"Master window centered, stack split on both sides" },
		{ "Alt + o",                  "Centered Floating  >M>",
			"Master window centered and floating above the tiled stack" },
		{ "Alt + g",                  "Grid  HHH",
			"Arranges all windows in an even grid" },
		{ "Alt + space",              "Toggle Previous Layout",
			"Switches back to the layout used before the current one" },
		{ "Alt + Shift + space",      "Toggle Floating",
			"Toggles the focused window between tiled and floating state" },
		{ NULL, "Master Area", NULL },
		{ "Alt + h / l",              "Resize Master Area",
			"Shrinks or expands the master area width by 5%" },
		{ "Alt + i / d",              "Master Count",
			"Increases or decreases the number of windows in the master area" },
		{ NULL, "Gaps", NULL },
		{ "Alt + minus / equal",      "Adjust Gaps",
			"Decreases or increases the gap size between windows by 1px" },
		{ "Alt + Shift + equal",      "Reset Gaps",
			"Resets all gaps back to the default size" },
		{ NULL, "Tags", NULL },
		{ "Alt + 1-9",                "Switch Tag",
			"Jumps to the selected tag — tags are like virtual desktops" },
		{ "Alt + Shift + 1-9",        "Move Window to Tag",
			"Sends the focused window to the selected tag" },
		{ "Alt + Ctrl + 1-9",         "Toggle Tag View",
			"Adds or removes a tag from the current view without leaving it" },
		{ "Alt + Ctrl + Shift + 1-9", "Preview Tag",
			"Shows a preview of the selected tag without switching to it" },
		{ "Alt + 0",                  "View All Tags",
			"Shows all windows from all tags at once" },
		{ "Alt + Shift + 0",          "Assign to All Tags",
			"Makes the focused window visible on every tag" },
		{ NULL, "Bar", NULL },
		{ "Alt + b",                  "Toggle Status Bar",
			"Shows or hides the MRDWM status bar at the top of the screen" },
		{ NULL, "Session", NULL },
		{ "Alt + Shift + q",          "Quit MRDWM",
			"Exits MRDWM and returns to the display manager or TTY" },
		{ "Alt + Ctrl + Shift + q",   "Restart MRDWM",
			"Recompiles and restarts MRDWM in place — all windows are preserved" },
		{ NULL, NULL, NULL }
	};
	nav_render_table(content, tbl);
	return root;}

	/* ================================================================== */
	/* MRDWM Mouse                                                        */
	/* ================================================================== */
	static GtkWidget *clicks_settings(void) {
		GtkWidget *root    = nav_make_page("input-mouse-symbolic", "Mouse Clicks & Buttons");
		GtkWidget *content = g_object_get_data(G_OBJECT(root), "content");
		static const NavRow tbl[] = {
			{ NULL, "Status Bar", NULL },
			{ "Left click launcher button",  "Open dmenu",
				"Clicks the launcher button in the bar to open dmenu" },
			{ "Left click layout symbol",    "Toggle Layout",
				"Cycles to the next layout by clicking the layout symbol" },
			{ "Right click layout symbol",   "Layout Menu",
				"Opens the layout selection menu" },
			{ "Left click window title",     "Toggle Show / Hide",
				"Clicking a window title in the bar shows or hides that window" },
			{ "Middle click window title",   "Zoom Window",
				"Swaps the clicked window into the master area" },
			{ "Right click window title",    "Close Window",
				"Kills the window whose title was right-clicked" },
			{ "Left click status text",      "Signal dwmblocks (1)",
				"Sends signal 1 to dwmblocks — used to refresh a status block" },
			{ "Middle click status text",    "Signal dwmblocks (2)",
				"Sends signal 2 to dwmblocks" },
			{ "Right click status text",     "Signal dwmblocks (3)",
				"Sends signal 3 to dwmblocks" },
			{ NULL, "Tags", NULL },
			{ "Left click tag",              "Switch to Tag",
				"Jumps to the clicked tag" },
			{ "Right click tag",             "Toggle Tag in View",
				"Adds or removes the clicked tag from the current view" },
			{ "Alt + Left click tag",        "Move Window to Tag",
				"Sends the focused window to the clicked tag" },
			{ "Alt + Right click tag",       "Toggle Tag on Window",
				"Adds or removes the clicked tag from the focused window" },
			{ NULL, "Windows", NULL },
			{ "Alt + Left drag",             "Move Window",
				"Hold Alt and drag with left button to move a floating window" },
			{ "Alt + Middle click",          "Toggle Floating",
				"Hold Alt and middle click to toggle floating on a window" },
			{ "Alt + Right drag",            "Resize Window",
				"Hold Alt and drag with right button to resize a floating window" },
			{ NULL, NULL, NULL }
		};
		nav_render_table(content, tbl);
		return root;}

		/* ================================================================== */
		/* ST Terminal                                                          */
		/* ================================================================== */
		static GtkWidget *terminal_settings(void) {
			GtkWidget *root    = nav_make_page("utilities-terminal-symbolic", "Terminal Shortcuts");
			GtkWidget *content = g_object_get_data(G_OBJECT(root), "content");
			static const NavRow tbl[] = {
				{ NULL, "Zoom", NULL },
				{ "Ctrl + Shift + Prior",    "Zoom In",
					"Increases the terminal font size by 1pt" },
				{ "Ctrl + Shift + Next",     "Zoom Out",
					"Decreases the terminal font size by 1pt" },
				{ "Ctrl + Shift + Home",     "Reset Zoom",
					"Resets the font size back to the default configured size" },
				{ NULL, "Clipboard", NULL },
				{ "Ctrl + Shift + C",        "Copy to Clipboard",
					"Copies the current selection to the system clipboard" },
				{ "Ctrl + Shift + V",        "Paste from Clipboard",
					"Pastes text from the system clipboard into the terminal" },
				{ "Ctrl + Shift + Y",        "Paste from Selection",
					"Pastes the current X11 primary selection" },
				{ "Shift + Insert",          "Paste Primary Selection",
					"Pastes the primary selection — equivalent to middle click paste" },
				{ NULL, "Scroll", NULL },
				{ "Shift + Page Up",         "Scroll Up",
					"Scrolls the terminal output up by one page" },
				{ "Shift + Page Down",       "Scroll Down",
					"Scrolls the terminal output down by one page" },
				{ "Scroll wheel up",         "Scroll Up One Line",
					"Scrolls terminal output up one line at a time" },
				{ "Scroll wheel down",       "Scroll Down One Line",
					"Scrolls terminal output down one line at a time" },
				{ NULL, "Color Schemes", NULL },
				{ "Alt + F1",                "st (dark)",
					"Default st dark color scheme — black background, grey text" },
				{ "Alt + F2",                "Alacritty Dark",
					"Alacritty-inspired dark color scheme" },
				{ "Alt + F3",                "One Half Dark",
					"Atom One Half dark theme" },
				{ "Alt + F4",                "One Half Light",
					"Atom One Half light theme — white background" },
				{ "Alt + F5",                "Solarized Dark",
					"Ethan Schoonover's Solarized dark palette" },
				{ "Alt + F6",                "Solarized Light",
					"Ethan Schoonover's Solarized light palette" },
				{ "Alt + F7",                "Gruvbox Dark",
					"Gruvbox retro dark color scheme" },
				{ "Alt + F8",                "Gruvbox Light",
					"Gruvbox retro light color scheme" },
				{ "Alt + 0",                 "Next Color Scheme",
					"Cycles forward through all available color schemes" },
				{ "Alt + Ctrl + 0",          "Previous Color Scheme",
					"Cycles backward through all available color schemes" },
				{ NULL, "Mouse", NULL },
				{ "Middle click",            "Paste Primary Selection",
					"Standard X11 middle click paste from primary selection" },
				{ "Ctrl + Print",            "Toggle Printer",
					"Toggles printing terminal output to the printer" },
				{ "Shift + Print",           "Print Screen",
					"Prints the current screen content" },
				{ "Any + Print",             "Print Selection",
					"Prints the current text selection" },
				{ NULL, NULL, NULL }
			};
			nav_render_table(content, tbl);
			return root;
		}

/* ================================================================== */
/* Appearance Settings                                                  */
/* - Toggle xfce4-panel on/off                                         */
/* - Toggle MRDWM bar on/off (writes config.h, triggers recompile)      */
/* - Edit MRDWM color schemes via colors.h                               */
/* - Recompile MRDWM + restart via SIGUSR2 → run_restart flag           */
/*                                                                      */
/* MRDWM side requires in dwm.c:                                         */
/*   static int run_restart = 0;                                        */
/*   static void sigusr2(int unused) { run_restart = 1; }              */
/*   signal(SIGUSR2, sigusr2);   ← in setup()                         */
/*   In run() loop:                                                     */
/*     if (run_restart) { run_restart = 0; Arg a={.i=1}; quit(&a); }  */
/* ================================================================== */

/* ------------------------------------------------------------------ */
/* Structs                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
	char fg[32];
	char bg[32];
	char border[32];
} AppScheme;

static const char *app_scheme_names[] = {
	"Normal",
	"Selected",
	"Hover",
	"Hidden",
	"Status Bar",
	"Tags Selected",
	"Tags Normal",
	"Info Selected",
	"Info Normal",
	NULL
};

static const char *app_scheme_vars[] = {
	"sch_norm",
	"sch_sel",
	"sch_hov",
	"sch_hid",
	"sch_status",
	"sch_tags_sel",
	"sch_tags_norm",
	"sch_info_sel",
	"sch_info_norm",
	NULL
};

#define APP_N_SCHEMES 9

typedef struct {
	AppScheme       schemes[APP_N_SCHEMES];
	char            colors_h_path[512];
	char            dwm_src_path[512];
	GtkWidget      *status_label;
	GtkColorDialog *cdlg;
} AppData;

typedef struct {
	GtkWidget *lbl;
	char      *txt;
} AppLblMsg;

typedef struct {
	AppData *ad;
	int      scheme_idx;
	int      col_idx;
} AppColorData;

/* ------------------------------------------------------------------ */
/* Color helpers                                                        */
/* ------------------------------------------------------------------ */

	static void
rgba_to_hex (const GdkRGBA *c, char *out, size_t sz)
{
	snprintf (out, sz, "#%02x%02x%02x",
		  (int)(c->red   * 255 + 0.5),
		  (int)(c->green * 255 + 0.5),
		  (int)(c->blue  * 255 + 0.5));
}

/* Read  static const char varname[] = "#RRGGBB";  from file */
	static char *
app_read_color (const char *path, const char *varname)
{
	FILE *f = fopen (path, "r");
	if (!f) return NULL;
	char line[256];
	while (fgets (line, sizeof (line), f)) {
		if (!strstr (line, varname)) continue;
		char *q = strchr (line, '"');
		if (!q) continue;
		char *e = strchr (q + 1, '"');
		if (!e) continue;
		*e = '\0';
		fclose (f);
		return strdup (q + 1);
	}
	fclose (f);
	return NULL;
}

	static void
app_load_colors (AppData *ad)
{
	for (int i = 0; app_scheme_vars[i]; i++) {
		char var[64];

		snprintf (var, sizeof (var), "%s_fg", app_scheme_vars[i]);
		char *v = app_read_color (ad->colors_h_path, var);
		strncpy (ad->schemes[i].fg, v ? v : "#222222", 31);
		free (v);

		snprintf (var, sizeof (var), "%s_bg", app_scheme_vars[i]);
		v = app_read_color (ad->colors_h_path, var);
		strncpy (ad->schemes[i].bg, v ? v : "#005577", 31);
		free (v);

		snprintf (var, sizeof (var), "%s_border", app_scheme_vars[i]);
		v = app_read_color (ad->colors_h_path, var);
		strncpy (ad->schemes[i].border, v ? v : "#444444", 31);
		free (v);
	}
}

	static gboolean
app_write_colors (AppData *ad)
{
	FILE *f = fopen (ad->colors_h_path, "w");
	if (!f) return FALSE;
	fprintf (f, "#ifndef COLORS_H\n#define COLORS_H\n");
	for (int i = 0; app_scheme_vars[i]; i++) {
		fprintf (f, "static const char %s_fg[]     = \"%s\";\n",
			 app_scheme_vars[i], ad->schemes[i].fg);
		fprintf (f, "static const char %s_bg[]     = \"%s\";\n",
			 app_scheme_vars[i], ad->schemes[i].bg);
		fprintf (f, "static const char %s_border[] = \"%s\";\n",
			 app_scheme_vars[i], ad->schemes[i].border);
	}
	fprintf (f, "#endif /* COLORS_H */\n");
	fclose (f);
	return TRUE;
}

/* ------------------------------------------------------------------ */
/* Color button changed callback                                        */
/* ------------------------------------------------------------------ */

	static void
app_color_set_cb (GtkColorDialogButton *btn, GParamSpec *ps, gpointer ud)
{
	AppColorData  *acd = ud;
	const GdkRGBA *c   = gtk_color_dialog_button_get_rgba (btn);
	char hex[32];
	rgba_to_hex (c, hex, sizeof (hex));
	AppScheme *s = &acd->ad->schemes[acd->scheme_idx];
	if      (acd->col_idx == 0) strncpy (s->fg,     hex, 31);
	else if (acd->col_idx == 1) strncpy (s->bg,     hex, 31);
	else                        strncpy (s->border, hex, 31);
}

/* ------------------------------------------------------------------ */
/* Compile + restart thread                                             */
/* ------------------------------------------------------------------ */

	static gboolean
app_set_label_idle (gpointer p)
{
	AppLblMsg *m = p;
	if (m->lbl) gtk_label_set_text (GTK_LABEL (m->lbl), m->txt);
	g_free (m->txt);
	g_free (m);
	return G_SOURCE_REMOVE;
}

	static gpointer
app_compile_thread (gpointer p)
{
	AppData   *ad = p;
	AppLblMsg *m  = g_new0 (AppLblMsg, 1);
	m->lbl = ad->status_label;

	char cmd[1024];
	snprintf (cmd, sizeof (cmd),
		  "cd '%s' && sudo make clean install 2>/dev/null",
		  ad->dwm_src_path);
	int ret = system (cmd);

	if (ret != 0) {
		m->txt = g_strdup ("✗  Build failed — check your source.");
		g_idle_add (app_set_label_idle, m);
		return NULL;
	}

	int sig_ret = system ("pkill -SIGUSR2 mrdwm 2>/dev/null");
	m->txt = sig_ret == 0
		? g_strdup ("✓  MRDWM rebuilt and restarted.")
		: g_strdup ("✓  MRDWM rebuilt. Run: pkill -SIGUSR2 mrdwm");

	g_idle_add (app_set_label_idle, m);
	return NULL;
}

/* ------------------------------------------------------------------ */
/* Apply button callback                                                */
/* ------------------------------------------------------------------ */

	static void
app_apply_cb (GtkWidget *btn, gpointer ud)
{
	AppData *ad = ud;
	gtk_label_set_text (GTK_LABEL (ad->status_label), "Writing colors.h…");

	if (!app_write_colors (ad)) {
		gtk_label_set_text (GTK_LABEL (ad->status_label),
				    "✗  Cannot write colors.h — check path.");
		return;
	}

	gtk_label_set_text (GTK_LABEL (ad->status_label), "Compiling MRDWM…");
	g_thread_unref (g_thread_new ("dwm-build", app_compile_thread, ad));
}

/* ------------------------------------------------------------------ */
/* xfce4-panel color callback                                           */
/* ------------------------------------------------------------------ */

	static void
app_panel_color_set_cb (GtkColorDialogButton *btn, GParamSpec *ps, gpointer ud)
{
	const GdkRGBA *c = gtk_color_dialog_button_get_rgba (btn);

	char cmd[512];
	snprintf (cmd, sizeof (cmd),
		  "xfconf-query -c xfce4-panel -p /panels/panel-1/background-style -s 1 2>/dev/null && "
		  "xfconf-query -c xfce4-panel -p /panels/panel-1/background-rgba "
		  "--create -t double -t double -t double -t double "
		  "-s %.6f -s %.6f -s %.6f -s %.6f 2>/dev/null",
		  c->red, c->green, c->blue, c->alpha);
	//"xfce4-panel --restart 2>/dev/null &",
	system (cmd);
}



	static void
app_panel_toggled (GtkSwitch *sw, GParamSpec *ps, gpointer ud)
{
	if (gtk_switch_get_active (sw))
		system ("xfce4-panel & disown");
	else
		system ("pkill -x xfce4-panel 2>/dev/null");
}

	static void
app_bar_toggled (GtkSwitch *sw, GParamSpec *ps, gpointer ud)
{
	AppData *ad   = ud;
	int      show = gtk_switch_get_active (sw) ? 1 : 0;
	char     cmd[1024];
	snprintf (cmd, sizeof (cmd),
		  "sed -i 's/^static const int showbar[^=]*=.*;/"
		  "static const int showbar                  = %d;        "
		  "\\/\\* 0 means no bar *\\//' '%s/config.h' 2>/dev/null",
		  show, ad->dwm_src_path);
	system (cmd);
}

/* ------------------------------------------------------------------ */
/* Page destroy cleanup                                                 */
/* ------------------------------------------------------------------ */

	static void
app_data_free (GtkWidget *w, gpointer ud)
{
	AppData *ad = ud;
	if (ad->cdlg) { g_object_unref (ad->cdlg); ad->cdlg = NULL; }
	g_free (ad);
}

/* ------------------------------------------------------------------ */
/* Build the page                                                       */
/* ------------------------------------------------------------------ */

	static GtkWidget *
appearance_settings (void)
{
	AppData *ad = g_new0 (AppData, 1);

	//char *home = get_home_env ();
	//snprintf (ad->dwm_src_path,  sizeof (ad->dwm_src_path),
	//	  "%s/.local/src/suckless/dwm", home ? home : "/root");

	snprintf (ad->dwm_src_path,  sizeof (ad->dwm_src_path),
          "/usr/local/src/mrrobotos/mrdwm");
	//snprintf (ad->colors_h_path, sizeof (ad->colors_h_path),
	//	  "%s/.local/src/suckless/dwm/colors.h", home ? home : "/root");
	snprintf(ad->colors_h_path, sizeof(ad->colors_h_path),
         "/usr/local/src/mrrobotos/mrdwm/colors.h");

	//free (home);

	app_load_colors (ad);

	/* ── Root ── */
	GtkWidget *root = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand (root, TRUE);
	gtk_widget_set_vexpand (root, TRUE);

	gtk_box_append (GTK_BOX (root),
			make_page_header ("preferences-desktop-appearance-symbolic", "Appearance"));
	gtk_box_append (GTK_BOX (root),
			gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

	GtkWidget *scr = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scr),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand (scr, TRUE);

	GtkWidget *content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 20);
	gtk_widget_set_margin_start  (content, 24);
	gtk_widget_set_margin_end    (content, 24);
	gtk_widget_set_margin_top    (content, 20);
	gtk_widget_set_margin_bottom (content, 28);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scr), content);
	gtk_box_append (GTK_BOX (root), scr);

	/* ════════════════════════════════════════
	 * SECTION 1 — Panels & Bars
	 * ════════════════════════════════════════ */
	GtkWidget *bars_frame = make_section_box ("Panels & Bars");
	GtkWidget *bars_box   = g_object_get_data (G_OBJECT (bars_frame), "inner-box");

#define TOGGLE_ROW(icon, title, subtitle, switch_widget) \
	{ \
		GtkWidget *_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12); \
		gtk_widget_set_margin_start  (_row, 16); gtk_widget_set_margin_end   (_row, 16); \
		gtk_widget_set_margin_top    (_row, 12); gtk_widget_set_margin_bottom (_row, 12); \
		GtkWidget *_ico = gtk_image_new_from_icon_name (icon); \
		gtk_image_set_pixel_size (GTK_IMAGE (_ico), 22); \
		gtk_widget_set_valign (_ico, GTK_ALIGN_CENTER); \
		gtk_box_append (GTK_BOX (_row), _ico); \
		GtkWidget *_inf = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2); \
		gtk_widget_set_hexpand (_inf, TRUE); \
		GtkWidget *_t = gtk_label_new (title); \
		gtk_widget_add_css_class (_t, "title-4"); \
		gtk_widget_set_halign (_t, GTK_ALIGN_START); \
		gtk_box_append (GTK_BOX (_inf), _t); \
		GtkWidget *_s = gtk_label_new (subtitle); \
		gtk_widget_add_css_class (_s, "dim-label"); \
		gtk_widget_add_css_class (_s, "caption"); \
		gtk_widget_set_halign (_s, GTK_ALIGN_START); \
		gtk_box_append (GTK_BOX (_inf), _s); \
		gtk_box_append (GTK_BOX (_row), _inf); \
		gtk_widget_set_valign ((switch_widget), GTK_ALIGN_CENTER); \
		gtk_box_append (GTK_BOX (_row), (switch_widget)); \
		gtk_box_append (GTK_BOX (bars_box), _row); \
	}

	/* xfce4-panel */
	{
		char *chk = run_cmd_str ("pgrep -x xfce4-panel >/dev/null 2>&1 && echo yes || echo no");
		g_strstrip (chk);
		GtkWidget *sw = gtk_switch_new ();
		gtk_switch_set_active (GTK_SWITCH (sw), strcmp (chk, "yes") == 0);
		g_free (chk);
		g_signal_connect (sw, "notify::active", G_CALLBACK (app_panel_toggled), ad);
		TOGGLE_ROW ("preferences-system-symbolic",
			    "xfce4-panel",
			    "Show or hide the XFCE panel",
			    sw);
	}

	gtk_box_append (GTK_BOX (bars_box),
			gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

	/* DWM bar */
	{
		char val_cmd[512];
		snprintf (val_cmd, sizeof (val_cmd),
			  "grep 'showbar' '%s/config.h' 2>/dev/null | grep -o '= [01]' | tr -d '= '",
			  ad->dwm_src_path);
		char *val = run_cmd_str (val_cmd);
		g_strstrip (val);
		GtkWidget *sw = gtk_switch_new ();
		gtk_switch_set_active (GTK_SWITCH (sw), strcmp (val, "0") != 0);
		g_free (val);
		g_signal_connect (sw, "notify::active", G_CALLBACK (app_bar_toggled), ad);
		TOGGLE_ROW ("video-display-symbolic",
			    "MRDWM Status Bar",
			    "Toggle bar visibility (needs recompile)",
			    sw);
	}

#undef TOGGLE_ROW

	gtk_box_append (GTK_BOX (content), bars_frame);

	/* ════════════════════════════════════════
	 * SECTION 2 — xfce4-panel Color
	 * ════════════════════════════════════════ */
	GtkWidget *panel_frame = make_section_box ("Panel Color");
	GtkWidget *panel_box   = g_object_get_data (G_OBJECT (panel_frame), "inner-box");

	{
		GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
		gtk_widget_set_margin_start  (row, 16); gtk_widget_set_margin_end   (row, 16);
		gtk_widget_set_margin_top    (row, 12); gtk_widget_set_margin_bottom (row, 12);

		GtkWidget *ico = gtk_image_new_from_icon_name ("preferences-system-symbolic");
		gtk_image_set_pixel_size (GTK_IMAGE (ico), 22);
		gtk_widget_set_valign (ico, GTK_ALIGN_CENTER);
		gtk_box_append (GTK_BOX (row), ico);

		GtkWidget *inf = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
		gtk_widget_set_hexpand (inf, TRUE);
		GtkWidget *t = gtk_label_new ("xfce4-panel Background");
		gtk_widget_add_css_class (t, "title-4");
		gtk_widget_set_halign (t, GTK_ALIGN_START);
		gtk_box_append (GTK_BOX (inf), t);
		GtkWidget *s = gtk_label_new ("Applied immediately via xfconf-query");
		gtk_widget_add_css_class (s, "dim-label");
		gtk_widget_add_css_class (s, "caption");
		gtk_widget_set_halign (s, GTK_ALIGN_START);
		gtk_box_append (GTK_BOX (inf), s);
		gtk_box_append (GTK_BOX (row), inf);

		/* Read current panel background-rgba */
		char *cur = run_cmd_str (
					 "xfconf-query -c xfce4-panel -p /panels/panel-1/background-rgba"
					 " 2>/dev/null | tail -n4");
		GdkRGBA rgba = {0.133, 0.133, 0.133, 1.0};
		if (cur && cur[0]) {
			double r=0, g=0, b=0, a=1;
			if (sscanf (cur, "%lf\n%lf\n%lf\n%lf", &r, &g, &b, &a) == 4 ||
			    sscanf (cur, "%lf %lf %lf %lf",    &r, &g, &b, &a) == 4) {
				rgba.red = r; rgba.green = g; rgba.blue = b; rgba.alpha = a;
			}
		}
		g_free (cur);

		/* ✅ Each button owns its dialog via g_object_set_data_full — no manual unref */
		GtkColorDialog *panel_cdlg = gtk_color_dialog_new ();
		gtk_color_dialog_set_with_alpha (panel_cdlg, TRUE);
		GtkWidget *cb = gtk_color_dialog_button_new (panel_cdlg);
		g_object_set_data_full (G_OBJECT (cb), "cdlg",
					panel_cdlg, g_object_unref);

		gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (cb), &rgba);
		gtk_widget_set_size_request (cb, 100, 36);
		gtk_widget_set_hexpand      (cb, FALSE);
		gtk_widget_set_valign       (cb, GTK_ALIGN_CENTER);
		g_signal_connect (cb, "notify::rgba",
				  G_CALLBACK (app_panel_color_set_cb), NULL);

		gtk_box_append (GTK_BOX (row), cb);
		gtk_box_append (GTK_BOX (panel_box), row);
	}

	gtk_box_append (GTK_BOX (content), panel_frame);

	/* ════════════════════════════════════════
	 * SECTION 2b — DWM Color Schemes
	 * ════════════════════════════════════════ */
	GtkWidget *col_frame = make_section_box ("MRDWM Color Schemes");
	GtkWidget *col_box   = g_object_get_data (G_OBJECT (col_frame), "inner-box");

	/* Column headers */
	{
		GtkWidget *hdr = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_set_margin_start (hdr, 16); gtk_widget_set_margin_end   (hdr, 16);
		gtk_widget_set_margin_top   (hdr,  8); gtk_widget_set_margin_bottom (hdr,  4);

		GtkWidget *sl = gtk_label_new ("Scheme");
		gtk_widget_add_css_class (sl, "caption");
		gtk_widget_add_css_class (sl, "dim-label");
		gtk_widget_set_hexpand (sl, TRUE);
		gtk_widget_set_halign  (sl, GTK_ALIGN_START);
		gtk_box_append (GTK_BOX (hdr), sl);

		const char *heads[] = { "Foreground", "Background", "Border" };
		for (int h = 0; h < 3; h++) {
			GtkWidget *hl = gtk_label_new (heads[h]);
			gtk_widget_add_css_class (hl, "caption");
			gtk_widget_add_css_class (hl, "dim-label");
			gtk_widget_set_size_request (hl, 110, -1);
			gtk_widget_set_halign (hl, GTK_ALIGN_CENTER);
			gtk_box_append (GTK_BOX (hdr), hl);
		}
		gtk_box_append (GTK_BOX (col_box), hdr);
		gtk_box_append (GTK_BOX (col_box),
				gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
	}

	/* ✅ REMOVED: ad->cdlg shared dialog — each button gets its own below */

	for (int i = 0; app_scheme_names[i]; i++) {
		GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_set_margin_start  (row, 16); gtk_widget_set_margin_end   (row, 16);
		gtk_widget_set_margin_top    (row,  6); gtk_widget_set_margin_bottom (row,  6);

		GtkWidget *lbl = gtk_label_new (app_scheme_names[i]);
		gtk_widget_set_hexpand (lbl, TRUE);
		gtk_widget_set_halign  (lbl, GTK_ALIGN_START);
		gtk_widget_set_valign  (lbl, GTK_ALIGN_CENTER);
		gtk_box_append (GTK_BOX (row), lbl);

		const char *hex[3] = {
			ad->schemes[i].fg,
			ad->schemes[i].bg,
			ad->schemes[i].border
		};

		for (int c = 0; c < 3; c++) {
			GdkRGBA rgba;
			if (!gdk_rgba_parse (&rgba, hex[c]))
				gdk_rgba_parse (&rgba, "#222222");

			/* ✅ Fresh dialog per button, tied to button lifetime */
			GtkColorDialog *dlg = gtk_color_dialog_new ();
			gtk_color_dialog_set_with_alpha (dlg, FALSE);
			GtkWidget *cb = gtk_color_dialog_button_new (dlg);
			g_object_set_data_full (G_OBJECT (cb), "cdlg",
						dlg, g_object_unref);

			gtk_color_dialog_button_set_rgba (GTK_COLOR_DIALOG_BUTTON (cb), &rgba);
			gtk_widget_set_size_request (cb, 100, 32);
			gtk_widget_set_valign       (cb, GTK_ALIGN_CENTER);
			gtk_widget_set_margin_start (cb, 5);
			gtk_widget_set_margin_end   (cb, 5);

			AppColorData *acd = g_new0 (AppColorData, 1);
			acd->ad         = ad;
			acd->scheme_idx = i;
			acd->col_idx    = c;
			g_signal_connect_data (cb, "notify::rgba",
					       G_CALLBACK (app_color_set_cb), acd,
					       (GClosureNotify) g_free, 0);

			gtk_box_append (GTK_BOX (row), cb);
		}

		gtk_box_append (GTK_BOX (col_box), row);
		if (app_scheme_names[i + 1])
			gtk_box_append (GTK_BOX (col_box),
					gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
	}

	gtk_box_append (GTK_BOX (content), col_frame);

	/* ════════════════════════════════════════
	 * SECTION 3 — Apply
	 * ════════════════════════════════════════ */
	GtkWidget *apply_frame = make_section_box ("Apply Changes");
	GtkWidget *apply_box   = g_object_get_data (G_OBJECT (apply_frame), "inner-box");

	GtkWidget *note = gtk_label_new (
					 "Recompiles MRDWM from source and installs the new binary. "
					 "All running applications are preserved during the restart.");

	gtk_widget_add_css_class (note, "dim-label");
	gtk_widget_add_css_class (note, "caption");
	gtk_label_set_wrap (GTK_LABEL (note), TRUE);
	gtk_widget_set_margin_start (note, 16); gtk_widget_set_margin_end  (note, 16);
	gtk_widget_set_margin_top   (note, 12); gtk_widget_set_halign      (note, GTK_ALIGN_START);
	gtk_box_append (GTK_BOX (apply_box), note);

	ad->status_label = gtk_label_new ("");
	gtk_widget_add_css_class (ad->status_label, "dim-label");
	gtk_widget_set_halign     (ad->status_label, GTK_ALIGN_START);
	gtk_widget_set_margin_start  (ad->status_label, 16);
	gtk_widget_set_margin_bottom (ad->status_label, 4);
	gtk_box_append (GTK_BOX (apply_box), ad->status_label);

	GtkWidget *btn_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_start  (btn_row, 16); gtk_widget_set_margin_end   (btn_row, 16);
	gtk_widget_set_margin_top    (btn_row, 10); gtk_widget_set_margin_bottom (btn_row, 14);

	GtkWidget *apply_btn = gtk_button_new_with_label ("Apply & Restart MRDWM");
	gtk_widget_add_css_class (apply_btn, "suggested-action");
	gtk_widget_add_css_class (apply_btn, "pill");
	gtk_widget_set_halign    (apply_btn, GTK_ALIGN_START);
	g_signal_connect (apply_btn, "clicked", G_CALLBACK (app_apply_cb), ad);
	gtk_box_append (GTK_BOX (btn_row), apply_btn);
	gtk_box_append (GTK_BOX (apply_box), btn_row);

	gtk_box_append (GTK_BOX (content), apply_frame);

	g_signal_connect (root, "destroy", G_CALLBACK (app_data_free), ad);
	return root;
}





/* ================================================================== */
/*  AVATAR CALLBACKS                                                    */
/* ================================================================== */

	static void
av_open_finish_cb(GObject *src, GAsyncResult *res, gpointer ud)
{
	AvData *a = (AvData *)ud;
	GFile  *f = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, NULL);
	if (!f) { g_free(a); return; }

	char *sp = g_file_get_path(f);
	g_object_unref(f);
	if (!sp) { g_free(a); return; }

	GFile  *gsrc = g_file_new_for_path(sp);
	GFile  *gdst = g_file_new_for_path(a->apath);
	GError *err  = NULL;
	g_file_copy(gsrc, gdst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &err);
	g_object_unref(gsrc);
	g_object_unref(gdst);
	g_free(sp);
	if (err) { g_error_free(err); g_free(a); return; }

	/* Update account page avatar */
	GFile *gf = g_file_new_for_path(a->apath);
	if (GTK_IS_PICTURE(a->av))
		gtk_picture_set_file(GTK_PICTURE(a->av), gf);
	else {
		GdkTexture *tex = gdk_texture_new_from_file(gf, NULL);
		if (tex) {
			gtk_image_set_from_paintable(GTK_IMAGE(a->av), GDK_PAINTABLE(tex));
			g_object_unref(tex);
		}
	}

	/* Update sidebar avatar */
	//if (sidebar_av_global && GTK_IS_PICTURE(sidebar_av_global))
	//    gtk_picture_set_file(GTK_PICTURE(sidebar_av_global), gf);

	if (sidebar_av_global && GTK_IS_DRAWING_AREA (sidebar_av_global))
		av_circle_set_path (sidebar_av_global, a->apath, AVATAR_SIDEBAR_SIZE);

	g_object_unref(gf);
	g_free(a);
}

//static void
//av_open_finish_cb (GObject *src, GAsyncResult *res, gpointer ud)
//{
//    AvData *a = (AvData *)ud;
//    GFile  *f = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (src), res, NULL);
//    if (!f) { g_free (a); return; }
//
//    char *sp = g_file_get_path (f);
//    g_object_unref (f);
//    if (!sp) { g_free (a); return; }
//
//    GFile  *gsrc = g_file_new_for_path (sp);
//    GFile  *gdst = g_file_new_for_path (a->apath);
//    GError *err  = NULL;
//    g_file_copy (gsrc, gdst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &err);
//    g_object_unref (gsrc);
//    g_object_unref (gdst);
//    g_free (sp);
//    if (err) { g_error_free (err); g_free (a); return; }
//
//    /* Update the avatar widget */
//    GFile *gf = g_file_new_for_path (a->apath);
//    if (GTK_IS_PICTURE (a->av)) {
//        gtk_picture_set_file (GTK_PICTURE (a->av), gf);
//    } else {
//        /* fallback: replace GtkImage with file-based paintable */
//        GdkTexture *tex = gdk_texture_new_from_file (gf, NULL);
//        if (tex) {
//            gtk_image_set_from_paintable (GTK_IMAGE (a->av), GDK_PAINTABLE (tex));
//            g_object_unref (tex);
//        }
//    }
//    g_object_unref (gf);
//    g_free (a);
//}

static void av_picker_load_dir(AvPickerData *ap, const char *path);

/* ── path-bar button ── */
static void av_path_btn_cb(GtkWidget *btn, gpointer unused)
{
	AvPickerData *ap = g_object_get_data(G_OBJECT(btn), "ap");
	const char   *p  = g_object_get_data(G_OBJECT(btn), "path");
	av_picker_load_dir(ap, p);
}

static void av_build_path_bar(AvPickerData *ap)
{
	GtkWidget *c;
	while ((c = gtk_widget_get_first_child(ap->path_bar)))
		gtk_widget_unparent(c);

	char buf[4096];
	strncpy(buf, ap->current_dir, sizeof(buf) - 1);

	char *names[64], *paths[64];
	int   n = 0;
	char  accum[4096] = "";

	names[n] = strdup("/"); paths[n] = strdup("/"); n++;

	char *p = buf;
	while (*p == '/') p++;
	while (*p && n < 63) {
		char *sl  = strchr(p, '/');
		size_t len = sl ? (size_t)(sl - p) : strlen(p);
		if (!len) { p++; continue; }
		char seg[256] = "";
		if (len >= sizeof(seg)) len = sizeof(seg) - 1;
		memcpy(seg, p, len); seg[len] = '\0';
		size_t al = strlen(accum);
		snprintf(accum + al, sizeof(accum) - al, "/%s", seg);
		names[n] = strdup(seg); paths[n] = strdup(accum); n++;
		p += len; if (*p == '/') p++;
	}

	for (int i = 0; i < n; i++) {
		if (i > 0)
			gtk_box_append(GTK_BOX(ap->path_bar), gtk_label_new(" › "));
		GtkWidget *btn = gtk_button_new_with_label(names[i]);
		gtk_widget_add_css_class(btn, "flat");
		gtk_widget_add_css_class(btn, "path-btn");
		g_object_set_data_full(G_OBJECT(btn), "path", g_strdup(paths[i]), g_free);
		g_object_set_data(G_OBJECT(btn), "ap", ap);
		g_signal_connect(btn, "clicked", G_CALLBACK(av_path_btn_cb), NULL);
		gtk_box_append(GTK_BOX(ap->path_bar), btn);
		free(names[i]); free(paths[i]);
	}
}

/* ── click on folder in flow ── */
static void av_folder_cb(GtkWidget *btn, gpointer unused)
{
	AvPickerData *ap   = g_object_get_data(G_OBJECT(btn), "ap");
	const char   *path = g_object_get_data(G_OBJECT(btn), "path");
	av_picker_load_dir(ap, path);
}

static void av_image_cb (GtkWidget *btn, gpointer unused)
{
	AvPickerData *ap   = g_object_get_data (G_OBJECT (btn), "ap");
	const char   *path = g_object_get_data (G_OBJECT (btn), "path");
	strncpy (ap->selected_path, path, sizeof (ap->selected_path) - 1);

	/* Update circular DrawingArea — stays exactly 220×220 */
	av_circle_set_path (ap->picker_preview, path, 220);
}

static void av_picker_apply (GtkWidget *btn, gpointer ud)
{
	AvPickerData *ap = ud;
	if (!ap->selected_path[0]) return;

	/* Refresh account page avatar DrawingArea */
	if (ap->dest_av && GTK_IS_DRAWING_AREA (ap->dest_av))
		av_circle_set_path (ap->dest_av, ap->selected_path, AVATAR_DRAW_AREA_SIZE);

	if (account_av_global && GTK_IS_DRAWING_AREA (account_av_global))
		av_circle_set_path (account_av_global, ap->selected_path, AVATAR_DRAW_AREA_SIZE);

	/* Refresh sidebar avatar (GtkPicture, small — works fine) */
	//if (sidebar_av_global && GTK_IS_PICTURE (sidebar_av_global)) {
	//    GFile *gf = g_file_new_for_path (ap->selected_path);
	//    gtk_picture_set_file (GTK_PICTURE (sidebar_av_global), gf);
	//    g_object_unref (gf);
	//}

	if (sidebar_av_global && GTK_IS_DRAWING_AREA (sidebar_av_global))
		av_circle_set_path (sidebar_av_global, ap->selected_path, AVATAR_SIDEBAR_SIZE);

	/* Stage for Save Info */
	PiData *pid = g_object_get_data (G_OBJECT (ap->picker_window), "pi-data");
	if (pid)
		g_strlcpy (pid->staged_avatar_src, ap->selected_path,
			   sizeof (pid->staged_avatar_src));

	gtk_window_destroy (GTK_WINDOW (ap->picker_window));
	ap->picker_window = NULL;
}

///* ── click on image in flow → preview ── */
//static void av_image_cb(GtkWidget *btn, gpointer unused)
//{
//    AvPickerData *ap   = g_object_get_data(G_OBJECT(btn), "ap");
//    const char   *path = g_object_get_data(G_OBJECT(btn), "path");
//    strncpy(ap->selected_path, path, sizeof(ap->selected_path) - 1);
//    GFile *gf = g_file_new_for_path(path);
//    gtk_picture_set_file(GTK_PICTURE(ap->picker_preview), gf);
//    g_object_unref(gf);
//}

static void av_picker_load_dir(AvPickerData *ap, const char *path)
{
	char real[4096];
	if (!realpath(path, real)) strncpy(real, path, sizeof(real) - 1);
	strncpy(ap->current_dir, real, sizeof(ap->current_dir) - 1);
	av_build_path_bar(ap);

	/* clear flowbox */
	{
		GSList *to_rm = NULL;
		GtkWidget *ch = gtk_widget_get_first_child(ap->file_flow);
		while (ch) { to_rm = g_slist_prepend(to_rm, ch); ch = gtk_widget_get_next_sibling(ch); }
		for (GSList *l = to_rm; l; l = l->next)
			gtk_flow_box_remove(GTK_FLOW_BOX(ap->file_flow), GTK_WIDGET(l->data));
		g_slist_free(to_rm);
	}

	DIR *d = opendir(real);
	if (!d) { gtk_flow_box_insert(GTK_FLOW_BOX(ap->file_flow), gtk_label_new("Cannot open directory"), -1); return; }

	char **dirs = NULL, **fils = NULL;
	int    nd = 0, dc = 0, nf = 0, fc = 0;
	struct dirent *de;

	while ((de = readdir(d))) {
		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
		char fp[4096]; mkpath(fp, sizeof(fp), real, de->d_name);
		struct stat st; if (lstat(fp, &st)) continue;
		if (S_ISDIR(st.st_mode)) {
			if (nd >= dc) { dc = dc ? dc*2 : 64; dirs = realloc(dirs, dc * sizeof *dirs); }
			dirs[nd++] = strdup(de->d_name);
		} else if (is_img(de->d_name)) {
			if (nf >= fc) { fc = fc ? fc*2 : 64; fils = realloc(fils, fc * sizeof *fils); }
			fils[nf++] = strdup(de->d_name);
		}
	}
	closedir(d);

	if (nd) qsort(dirs, nd, sizeof *dirs, cmp_strp);
	if (nf) qsort(fils, nf, sizeof *fils, cmp_strp);

	/* folders */
	for (int i = 0; i < nd; i++) {
		char fp[4096]; mkpath(fp, sizeof(fp), real, dirs[i]);
		GtkWidget *vb  = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
		gtk_widget_set_size_request(vb, 100, 85);
		GtkWidget *ico = gtk_image_new_from_icon_name("folder");
		gtk_image_set_pixel_size(GTK_IMAGE(ico), 48);
		gtk_widget_set_halign(ico, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(vb), ico);
		GtkWidget *lbl = gtk_label_new(dirs[i]);
		gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_MIDDLE);
		gtk_label_set_max_width_chars(GTK_LABEL(lbl), 11);
		gtk_widget_set_halign(lbl, GTK_ALIGN_CENTER);
		gtk_box_append(GTK_BOX(vb), lbl);
		GtkWidget *btn = gtk_button_new();
		gtk_widget_add_css_class(btn, "flat");
		gtk_widget_add_css_class(btn, "picker-item");
		gtk_button_set_child(GTK_BUTTON(btn), vb);
		g_object_set_data_full(G_OBJECT(btn), "path", g_strdup(fp), g_free);
		g_object_set_data(G_OBJECT(btn), "ap", ap);
		g_signal_connect(btn, "clicked", G_CALLBACK(av_folder_cb), NULL);
		gtk_flow_box_insert(GTK_FLOW_BOX(ap->file_flow), btn, -1);
		free(dirs[i]);
	}
	free(dirs);

	/* images */
	for (int i = 0; i < nf; i++) {
		char fp[4096]; mkpath(fp, sizeof(fp), real, fils[i]);
		GFile     *_gf = g_file_new_for_path(fp);
		GtkWidget *pic = gtk_picture_new_for_file(_gf);
		g_object_unref(_gf);
		gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_COVER);
		gtk_widget_set_size_request(pic, 110, 90);
		gtk_widget_set_overflow(pic, GTK_OVERFLOW_HIDDEN);
		gtk_widget_set_halign(pic, GTK_ALIGN_CENTER);

		GtkWidget *lbl = gtk_label_new(fils[i]);
		gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_MIDDLE);
		gtk_label_set_max_width_chars(GTK_LABEL(lbl), 12);
		gtk_widget_set_halign(lbl, GTK_ALIGN_CENTER);

		GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
		gtk_widget_set_size_request(vb, 120, 115);
		gtk_box_append(GTK_BOX(vb), pic);
		gtk_box_append(GTK_BOX(vb), lbl);

		GtkWidget *btn = gtk_button_new();
		gtk_widget_add_css_class(btn, "flat");
		gtk_widget_add_css_class(btn, "picker-item");
		gtk_button_set_child(GTK_BUTTON(btn), vb);
		g_object_set_data_full(G_OBJECT(btn), "path", g_strdup(fp), g_free);
		g_object_set_data(G_OBJECT(btn), "ap", ap);
		g_signal_connect(btn, "clicked", G_CALLBACK(av_image_cb), NULL);
		gtk_flow_box_insert(GTK_FLOW_BOX(ap->file_flow), btn, -1);
		free(fils[i]);
	}
	free(fils);
}


typedef struct {
	GdkPixbuf *pixbuf;
	int        size;
} AvCircleData;

	static void
av_circle_data_free (gpointer p)
{
	AvCircleData *cd = p;
	if (cd && cd->pixbuf) g_object_unref (cd->pixbuf);
	g_free (cd);
}

	static GdkPixbuf *
av_load_pixbuf (const char *path, int px)
{
	if (!path || !g_file_test (path, G_FILE_TEST_EXISTS)) return NULL;
	/* Load at 2× requested size then scale — better quality */
	GdkPixbuf *raw = gdk_pixbuf_new_from_file_at_scale (
							    path, px * 2, px * 2, FALSE, NULL);
	if (!raw) return NULL;
	GdkPixbuf *scaled = gdk_pixbuf_scale_simple (
						     raw, px, px, GDK_INTERP_BILINEAR);
	g_object_unref (raw);
	return scaled;
}

	static void
av_circle_draw (GtkDrawingArea *da, cairo_t *cr, int w, int h, gpointer ud)
{
	AvCircleData *cd = ud;
	int    sz = cd->size;
	double cx = sz / 2.0;
	double cy = sz / 2.0;
	double r  = sz / 2.0 - 2.0;   /* 2px inset so border isn't clipped */

	/* ── circular clip ── */
	cairo_new_path (cr);
	cairo_arc      (cr, cx, cy, r, 0.0, 2.0 * G_PI);
	cairo_clip     (cr);

	if (cd->pixbuf) {
		int pw = gdk_pixbuf_get_width  (cd->pixbuf);
		int ph = gdk_pixbuf_get_height (cd->pixbuf);
		/* scale-to-cover: whichever axis is shorter drives the scale */
		double scale = MAX ((double)sz / pw, (double)sz / ph);
		double ox    = (sz - pw * scale) / 2.0;
		double oy    = (sz - ph * scale) / 2.0;
		cairo_translate (cr, ox, oy);
		cairo_scale     (cr, scale, scale);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		gdk_cairo_set_source_pixbuf (cr, cd->pixbuf, 0, 0);
#pragma GCC diagnostic pop
		cairo_paint (cr);
	} else {
		/* Placeholder — dark gradient + person silhouette */
		cairo_pattern_t *pat = cairo_pattern_create_radial (
								    cx, cy * 0.6, sz * 0.04,
								    cx, cy,       sz * 0.55);
		cairo_pattern_add_color_stop_rgb (pat, 0.0, 0.22, 0.22, 0.26);
		cairo_pattern_add_color_stop_rgb (pat, 1.0, 0.10, 0.10, 0.13);
		cairo_set_source (cr, pat);
		cairo_paint      (cr);
		cairo_pattern_destroy (pat);

		cairo_set_source_rgba (cr, 1, 1, 1, 0.20);
		/* head */
		cairo_arc  (cr, cx, cy - sz * 0.08, sz * 0.17, 0, 2 * G_PI);
		cairo_fill (cr);
		/* body arc */
		cairo_arc  (cr, cx, cy + sz * 0.32, sz * 0.27, G_PI, 2 * G_PI);
		cairo_fill (cr);
	}

	cairo_reset_clip (cr);

	/* ── border ring (drawn outside clip so fully visible) ── */
	cairo_new_path (cr);
	cairo_arc      (cr, cx, cy, r, 0.0, 2.0 * G_PI);
	cairo_set_source_rgba (cr, 0.20, 0.52, 0.89, 0.70);
	cairo_set_line_width  (cr, 3.5);
	cairo_stroke   (cr);
}

/* Create a new circular GtkDrawingArea.
 * path may be NULL for a placeholder.  size is the diameter in px. */
	static GtkWidget *
av_circle_new (const char *path, int size)
{
	AvCircleData *cd = g_new0 (AvCircleData, 1);
	cd->size   = size;
	cd->pixbuf = av_load_pixbuf (path, size);

	GtkWidget *da = gtk_drawing_area_new ();
	gtk_drawing_area_set_content_width  (GTK_DRAWING_AREA (da), size);
	gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (da), size);
	/* size_request on DrawingArea is honoured — unlike GtkPicture */
	gtk_widget_set_size_request (da, size, size);
	gtk_widget_set_hexpand      (da, FALSE);
	gtk_widget_set_vexpand      (da, FALSE);
	gtk_widget_set_halign       (da, GTK_ALIGN_CENTER);
	gtk_widget_set_valign       (da, GTK_ALIGN_CENTER);

	g_object_set_data_full (G_OBJECT (da), "av-cd", cd, av_circle_data_free);
	gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da),
					av_circle_draw, cd, NULL);
	return da;
}

/* Update an existing av_circle DrawingArea with a new image file. */
	static void
av_circle_set_path (GtkWidget *da, const char *path, int size)
{
	AvCircleData *cd = g_object_get_data (G_OBJECT (da), "av-cd");
	if (!cd) return;
	if (cd->pixbuf) { g_object_unref (cd->pixbuf); cd->pixbuf = NULL; }
	cd->pixbuf = av_load_pixbuf (path, size);
	gtk_widget_queue_draw (da);
}

/* ── "Set Avatar" confirm ── */
typedef struct { AvPickerData *ap; } AvConfirmData;

static void av_confirm_cb(GObject *src, GAsyncResult *res, gpointer ud)
{
	AvConfirmData *cd  = ud;
	AvPickerData  *ap  = cd->ap;
	g_free(cd);

	int btn = gtk_alert_dialog_choose_finish(GTK_ALERT_DIALOG(src), res, NULL);
	if (btn != 1) return;

	GFile  *gsrc = g_file_new_for_path(ap->selected_path);
	GFile  *gdst = g_file_new_for_path(ap->dest_path);
	GError *err  = NULL;
	g_file_copy(gsrc, gdst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &err);
	g_object_unref(gsrc); g_object_unref(gdst);
	if (err) { g_error_free(err); return; }

	GFile *gf = g_file_new_for_path(ap->dest_path);

	/* update account-page avatar */
	if (ap->dest_av) {
		if (GTK_IS_PICTURE(ap->dest_av))
			gtk_picture_set_file(GTK_PICTURE(ap->dest_av), gf);
		else {
			GdkTexture *tex = gdk_texture_new_from_file(gf, NULL);
			if (tex) { gtk_image_set_from_paintable(GTK_IMAGE(ap->dest_av), GDK_PAINTABLE(tex)); g_object_unref(tex); }
		}
	}
	/* update sidebar avatar */
	if (sidebar_av_global && GTK_IS_PICTURE(sidebar_av_global))
		gtk_picture_set_file(GTK_PICTURE(sidebar_av_global), gf);

	g_object_unref(gf);

	if (ap->picker_window) {
		gtk_window_destroy(GTK_WINDOW(ap->picker_window));
		ap->picker_window = NULL;
	}
}

//static void av_picker_apply(GtkWidget *btn, gpointer ud)
//{
//    AvPickerData *ap = ud;
//    if (!ap->selected_path[0]) return;
//
//    /* Update widgets visually — no file copy yet */
//    GFile *gf = g_file_new_for_path(ap->selected_path);
//
//    if (ap->dest_av) {
//        if (GTK_IS_PICTURE(ap->dest_av))
//            gtk_picture_set_file(GTK_PICTURE(ap->dest_av), gf);
//        else {
//            GdkTexture *tex = gdk_texture_new_from_file(gf, NULL);
//            if (tex) {
//                gtk_image_set_from_paintable(GTK_IMAGE(ap->dest_av),
//                                             GDK_PAINTABLE(tex));
//                g_object_unref(tex);
//            }
//        }
//    }
//    if (sidebar_av_global && GTK_IS_PICTURE(sidebar_av_global))
//        gtk_picture_set_file(GTK_PICTURE(sidebar_av_global), gf);
//
//    g_object_unref(gf);
//
//    /* Stage the source path — actual copy happens on Save Info */
//    PiData *pid = g_object_get_data(G_OBJECT(ap->picker_window), "pi-data");
//    if (pid)
//        g_strlcpy(pid->staged_avatar_src, ap->selected_path,
//                  sizeof(pid->staged_avatar_src));
//
//    gtk_window_destroy(GTK_WINDOW(ap->picker_window));
//    ap->picker_window = NULL;
//}

//static void av_picker_apply(GtkWidget *btn, gpointer ud)
//{
//    AvPickerData *ap = ud;
//    if (!ap->selected_path[0]) return;
//
//    AvConfirmData *cd = g_new0(AvConfirmData, 1);
//    cd->ap = ap;
//
//    char msg[4096 + 32];
//    snprintf(msg, sizeof(msg), "Use this image as your profile picture?\n%s", ap->selected_path);
//
//    GtkAlertDialog *dlg = gtk_alert_dialog_new("%s", msg);
//    gtk_alert_dialog_set_buttons(dlg, (const char *[]){"Cancel", "Set Avatar", NULL});
//    gtk_alert_dialog_set_default_button(dlg, 1);
//    gtk_alert_dialog_set_cancel_button(dlg, 0);
//    gtk_alert_dialog_choose(dlg,
//        ap->picker_window ? GTK_WINDOW(ap->picker_window) : GTK_WINDOW(window),
//        NULL, av_confirm_cb, cd);
//    g_object_unref(dlg);
//}

static void av_picker_cancel(GtkWidget *btn, gpointer ud)
{
	AvPickerData *ap = ud;
	gtk_window_destroy(GTK_WINDOW(ap->picker_window));
	ap->picker_window = NULL;
}

	static void
av_edit_clicked_cb(GtkButton *btn, gpointer d)
{
	AvData *av = (AvData *)d;

	/* Allocate picker state (freed when picker_window is destroyed) */
	AvPickerData *ap = g_new0(AvPickerData, 1);
	ap->dest_av = av->av;
	g_strlcpy(ap->dest_path, av->apath, sizeof(ap->dest_path));
	//ap->pid = av->pid;
	PiData *pid = g_object_get_data(G_OBJECT(btn), "pi-data");

	/* Build picker window */
	ap->picker_window = gtk_window_new();
	gtk_window_set_title(GTK_WINDOW(ap->picker_window), "Choose Profile Picture");
	gtk_window_set_transient_for(GTK_WINDOW(ap->picker_window), GTK_WINDOW(window));
	gtk_window_set_modal(GTK_WINDOW(ap->picker_window), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(ap->picker_window), 960, 640);
	/* ADD: */
	g_object_set_data(G_OBJECT(ap->picker_window), "pi-data", pid);
	g_signal_connect_swapped(ap->picker_window, "destroy", G_CALLBACK(g_free), ap);

	GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	/* path bar */
	GtkWidget *bscr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bscr), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_widget_set_margin_start(bscr, 8); gtk_widget_set_margin_end(bscr, 8);
	gtk_widget_set_margin_top(bscr, 8);   gtk_widget_set_margin_bottom(bscr, 4);
	ap->path_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(bscr), ap->path_bar);
	gtk_box_append(GTK_BOX(root), bscr);
	gtk_box_append(GTK_BOX(root), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

	/* main row: file flow + preview pane */
	GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_vexpand(row, TRUE);

	GtkWidget *fscr = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(fscr), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_hexpand(fscr, TRUE); gtk_widget_set_vexpand(fscr, TRUE);

	ap->file_flow = gtk_flow_box_new();
	gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(ap->file_flow), GTK_SELECTION_NONE);
	gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(ap->file_flow), 20);
	gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(ap->file_flow), 2);
	gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(ap->file_flow), 6);
	gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(ap->file_flow), 6);
	gtk_widget_set_margin_start(ap->file_flow, 10); gtk_widget_set_margin_end(ap->file_flow, 10);
	gtk_widget_set_margin_top(ap->file_flow, 10);   gtk_widget_set_margin_bottom(ap->file_flow, 10);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(fscr), ap->file_flow);
	gtk_box_append(GTK_BOX(row), fscr);
	gtk_box_append(GTK_BOX(row), gtk_separator_new(GTK_ORIENTATION_VERTICAL));

	/* preview pane */
	GtkWidget *pane = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_size_request(pane, 300, -1);
	gtk_widget_set_margin_start(pane, 14); gtk_widget_set_margin_end(pane, 14);
	gtk_widget_set_margin_top(pane, 14);   gtk_widget_set_margin_bottom(pane, 14);

	GtkWidget *pt = gtk_label_new("Preview");
	gtk_widget_add_css_class(pt, "title-4");
	gtk_widget_set_halign(pt, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(pane), pt);

	/* circular-clipped preview picture */
	//ap->picker_preview = gtk_picture_new();
	//gtk_picture_set_content_fit(GTK_PICTURE(ap->picker_preview), GTK_CONTENT_FIT_COVER);
	//gtk_widget_set_size_request(ap->picker_preview, 220, 220);
	//gtk_widget_set_hexpand(ap->picker_preview, FALSE);
	//gtk_widget_set_vexpand(ap->picker_preview, FALSE);
	//gtk_widget_set_halign(ap->picker_preview, GTK_ALIGN_CENTER);
	//gtk_widget_set_overflow(ap->picker_preview, GTK_OVERFLOW_HIDDEN);
	//gtk_widget_add_css_class(ap->picker_preview, "av-picker-preview");
	//gtk_box_append(GTK_BOX(pane), ap->picker_preview);
	//    ap->picker_preview = gtk_picture_new();
	//gtk_picture_set_content_fit(GTK_PICTURE(ap->picker_preview), GTK_CONTENT_FIT_COVER);
	//gtk_widget_set_hexpand(ap->picker_preview, TRUE);
	//gtk_widget_set_vexpand(ap->picker_preview, TRUE);
	//gtk_widget_set_overflow(ap->picker_preview, GTK_OVERFLOW_HIDDEN);
	//
	//GtkWidget *preview_frame = gtk_frame_new(NULL);
	//gtk_widget_set_size_request(preview_frame, 220, 220);   /* square */
	//gtk_widget_set_halign(preview_frame, GTK_ALIGN_CENTER);
	//gtk_widget_set_valign(preview_frame, GTK_ALIGN_CENTER);
	//gtk_widget_set_overflow(preview_frame, GTK_OVERFLOW_HIDDEN);
	//gtk_widget_add_css_class(preview_frame, "av-picker-preview-frame");
	//gtk_frame_set_child(GTK_FRAME(preview_frame), ap->picker_preview);
	//gtk_box_append(GTK_BOX(pane), preview_frame);

	//ap->picker_preview = gtk_picture_new();
	//gtk_picture_set_content_fit(GTK_PICTURE(ap->picker_preview), GTK_CONTENT_FIT_COVER);
	//gtk_widget_set_size_request(ap->picker_preview, 220, 220);
	//gtk_widget_set_hexpand     (ap->picker_preview, FALSE);
	//gtk_widget_set_vexpand     (ap->picker_preview, FALSE);
	//gtk_widget_set_overflow    (ap->picker_preview, GTK_OVERFLOW_HIDDEN);

	//GtkWidget *preview_frame = gtk_frame_new(NULL);
	//gtk_widget_set_size_request(preview_frame, 220, 220);
	//gtk_widget_set_halign      (preview_frame, GTK_ALIGN_CENTER);
	//gtk_widget_set_valign      (preview_frame, GTK_ALIGN_START); /* never stretch */
	//gtk_widget_set_hexpand     (preview_frame, FALSE);
	//gtk_widget_set_vexpand     (preview_frame, FALSE);
	//gtk_widget_set_overflow    (preview_frame, GTK_OVERFLOW_HIDDEN);
	//gtk_widget_add_css_class   (preview_frame, "av-picker-preview-frame");
	//gtk_frame_set_child(GTK_FRAME(preview_frame), ap->picker_preview);
	//gtk_box_append(GTK_BOX(pane), preview_frame);

	//GtkWidget *hint = gtk_label_new("Click an image to preview");
	//gtk_widget_add_css_class(hint, "dim-label");
	//gtk_widget_set_halign(hint, GTK_ALIGN_CENTER);
	//gtk_box_append(GTK_BOX(pane), hint);

	GtkWidget *prev_da = av_circle_new (NULL, 220);
	ap->picker_preview = prev_da;

	gtk_box_append (GTK_BOX (pane), prev_da);

	GtkWidget *hint = gtk_label_new ("Click an image to preview");
	gtk_widget_add_css_class (hint, "dim-label");
	gtk_widget_set_halign    (hint, GTK_ALIGN_CENTER);
	gtk_box_append (GTK_BOX (pane), hint);

	GtkWidget *sp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_vexpand(sp, TRUE);
	gtk_box_append(GTK_BOX(pane), sp);

	GtkWidget *set_btn = gtk_button_new_with_label("Set as Avatar");
	gtk_widget_add_css_class(set_btn, "suggested-action");
	g_signal_connect(set_btn, "clicked", G_CALLBACK(av_picker_apply), ap);
	gtk_box_append(GTK_BOX(pane), set_btn);

	GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
	g_signal_connect(cancel_btn, "clicked", G_CALLBACK(av_picker_cancel), ap);
	gtk_box_append(GTK_BOX(pane), cancel_btn);

	gtk_box_append(GTK_BOX(row), pane);
	gtk_box_append(GTK_BOX(root), row);
	gtk_window_set_child(GTK_WINDOW(ap->picker_window), root);

	/* open in home directory */
	char *home = get_home_env();
	av_picker_load_dir(ap, home ? home : "/");
	free(home);

	gtk_window_present(GTK_WINDOW(ap->picker_window));
}

//static void
//av_edit_clicked_cb(GtkButton *btn, gpointer d)
//{
//    AvData *av = (AvData *)d;
//
//    GtkFileDialog *fd = gtk_file_dialog_new();
//    gtk_file_dialog_set_title(fd, "Choose Profile Picture");
//
//    GtkFileFilter *ff = gtk_file_filter_new();
//    gtk_file_filter_add_mime_type(ff, "image/jpeg");
//    gtk_file_filter_add_mime_type(ff, "image/png");
//    gtk_file_filter_add_mime_type(ff, "image/webp");
//    gtk_file_filter_set_name(ff, "Images");
//
//    GListStore *flist = g_list_store_new(GTK_TYPE_FILE_FILTER);
//    g_list_store_append(flist, ff);
//    gtk_file_dialog_set_filters(fd, G_LIST_MODEL(flist));
//    g_object_unref(ff);
//    g_object_unref(flist);
//
//    AvData *av2 = g_new0(AvData, 1);
//    *av2 = *av;
//
//    gtk_file_dialog_open(fd, GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(btn))), NULL, av_open_finish_cb, av2);
//    g_object_unref(fd);
//}

//static void
//av_edit_clicked_cb (GtkButton *btn, gpointer d)
//{
//    (void)btn;
//    AvData *av = (AvData *)d;
//
//    GtkFileDialog *fd = gtk_file_dialog_new ();
//    gtk_file_dialog_set_title (fd, "Choose Profile Picture");
//
//    GtkFileFilter *ff = gtk_file_filter_new ();
//    gtk_file_filter_add_mime_type (ff, "image/jpeg");
//    gtk_file_filter_add_mime_type (ff, "image/png");
//    gtk_file_filter_add_mime_type (ff, "image/webp");
//    gtk_file_filter_set_name (ff, "Images");
//
//    GListStore *flist = g_list_store_new (GTK_TYPE_FILE_FILTER);
//    g_list_store_append (flist, ff);
//    gtk_file_dialog_set_filters (fd, G_LIST_MODEL (flist));
//    g_object_unref (ff);
//    g_object_unref (flist);
//
//    AvData *av2 = g_new0 (AvData, 1);
//    *av2 = *av;
//
//    //gtk_file_dialog_open (fd, NULL, NULL, av_open_finish_cb, av2);
//    gtk_file_dialog_open (fd, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (btn))), NULL, av_open_finish_cb, av2);
//    g_object_unref (fd);
//}

	static void
av_data_destroy_cb (gpointer d, GClosure *c)
{
	(void)c;
	g_free (d);
}

/* ================================================================== */
/*  PERSONAL INFO CALLBACKS                                             */
/* ================================================================== */

	static void
save_pi_clicked_cb (GtkButton *btn, gpointer d)
{
	PiData   *p  = (PiData *)d;
	GKeyFile *kf = g_key_file_new ();

	g_key_file_set_string (kf, "Personal", "FullName",
			       gtk_editable_get_text (GTK_EDITABLE (p->fullname)));
	g_key_file_set_string (kf, "Personal", "Email",
			       gtk_editable_get_text (GTK_EDITABLE (p->email)));
	g_key_file_set_string (kf, "Personal", "Phone",
			       gtk_editable_get_text (GTK_EDITABLE (p->phone)));
	g_key_file_set_string (kf, "Personal", "Organisation",
			       gtk_editable_get_text (GTK_EDITABLE (p->org)));
	g_key_file_set_string (kf, "Personal", "Address",
			       gtk_editable_get_text (GTK_EDITABLE (p->addr)));
	g_key_file_set_string (kf, "Personal", "City",
			       gtk_editable_get_text (GTK_EDITABLE (p->city)));
	g_key_file_set_string (kf, "Personal", "Country",
			       gtk_editable_get_text (GTK_EDITABLE (p->country)));
	g_key_file_set_string (kf, "Personal", "Website",
			       gtk_editable_get_text (GTK_EDITABLE (p->website)));

	char info_path[600];
	snprintf (info_path, sizeof (info_path), "%s/info", p->dir);

	GError  *err = NULL;
	gboolean ok  = g_key_file_save_to_file (kf, info_path, &err);
	if (err) g_error_free (err);
	g_key_file_free (kf);

	/* Copy staged avatar to final destination now that user confirmed */
	if (ok && p->staged_avatar_src[0]) {
		char av_path[600];
		snprintf (av_path, sizeof (av_path), "%s/avatar", p->dir);
		GFile  *gsrc = g_file_new_for_path (p->staged_avatar_src);
		GFile  *gdst = g_file_new_for_path (av_path);
		GError *cerr = NULL;
		g_file_copy (gsrc, gdst, G_FILE_COPY_OVERWRITE,
			     NULL, NULL, NULL, &cerr);
		g_object_unref (gsrc);
		g_object_unref (gdst);
		if (cerr) g_error_free (cerr);
		else p->staged_avatar_src[0] = '\0'; /* clear so double-save is a no-op */
	}

	GtkAlertDialog *dlg = gtk_alert_dialog_new (
						    ok ? "\342\234\223  Personal info saved."
						    : "\342\234\227  Failed to save info.");
	gtk_alert_dialog_set_buttons (dlg, (const char *[]){"OK", NULL});
	gtk_alert_dialog_set_default_button (dlg, 0);
	GtkRoot *top = gtk_widget_get_root (GTK_WIDGET (btn));
	gtk_alert_dialog_choose (dlg,
				 (top && GTK_IS_WINDOW (top)) ? GTK_WINDOW (top) : NULL,
				 NULL, NULL, NULL);
	g_object_unref (dlg);
}

	static void
pi_data_destroy_cb (gpointer d, GClosure *c)
{
	(void)c;
	g_free (d);
}

/* ================================================================== */
/*  SMALL UI HELPERS                                                    */
/* ================================================================== */

	static GtkWidget *
section_title (const char *t)
{
	GtkWidget *l = gtk_label_new (t);
	gtk_widget_set_halign (l, GTK_ALIGN_START);
	gtk_widget_add_css_class (l, "users-section-title");
	return l;
}

	static void
make_info_row (GtkWidget  *card,
	       const char *label,
	       const char *value,
	       gboolean    separator)
{
	GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_set_margin_start  (row, 16);
	gtk_widget_set_margin_end    (row, 16);
	gtk_widget_set_margin_top    (row, 13);
	gtk_widget_set_margin_bottom (row, 13);

	GtkWidget *lbl = gtk_label_new (label);
	gtk_widget_add_css_class (lbl, "users-row-label");
	gtk_widget_set_hexpand   (lbl, TRUE);
	gtk_widget_set_halign    (lbl, GTK_ALIGN_START);

	GtkWidget *val = gtk_label_new (value);
	gtk_widget_add_css_class (val, "users-row-value");

	gtk_box_append (GTK_BOX (row), lbl);
	gtk_box_append (GTK_BOX (row), val);
	gtk_box_append (GTK_BOX (card), row);

	if (separator)
		gtk_box_append (GTK_BOX (card),
				gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
}


/* Returns a vertical box: label on top, entry below.
 * The entry is always the LAST child — gtk_widget_get_last_child() retrieves it. */
	static GtkWidget *
make_entry_row (const char *label_text,
		const char *placeholder,
		gboolean    secret)
{
	GtkWidget *col = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);

	GtkWidget *lbl = gtk_label_new (label_text);
	gtk_widget_set_halign (lbl, GTK_ALIGN_START);
	gtk_widget_add_css_class (lbl, "caption");

	GtkWidget *ent = gtk_entry_new ();
	gtk_entry_set_placeholder_text (GTK_ENTRY (ent), placeholder);
	if (secret) {
		gtk_entry_set_visibility    (GTK_ENTRY (ent), FALSE);
		gtk_entry_set_input_purpose (GTK_ENTRY (ent), GTK_INPUT_PURPOSE_PASSWORD);
	}
	gtk_widget_set_hexpand (ent, TRUE);

	gtk_box_append (GTK_BOX (col), lbl);
	gtk_box_append (GTK_BOX (col), ent);
	return col;
}

/* Creates a card box inside parent and returns its inner padding box. */
	static GtkWidget *
card_inner (GtkWidget *parent, int spacing)
{
	GtkWidget *card  = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget *inner = gtk_box_new (GTK_ORIENTATION_VERTICAL, spacing);

	gtk_widget_add_css_class     (card, "users-card");
	gtk_widget_set_margin_start  (inner, 16);
	gtk_widget_set_margin_end    (inner, 16);
	gtk_widget_set_margin_top    (inner, 16);
	gtk_widget_set_margin_bottom (inner, 16);

	gtk_box_append (GTK_BOX (card), inner);
	gtk_box_append (GTK_BOX (parent), card);
	return inner;
}


/* ================================================================== */
/*  account_settings()  — pure C, no lambdas, no auto                 */
/* ================================================================== */

	static GtkWidget *
account_settings (void)
{
	const char *uname = g_get_user_name ();
	const char *rname = g_get_real_name ();
	char *home        = get_home_env ();

	char avatar_dir [512];
	char avatar_path[600];
	snprintf (avatar_dir,  sizeof (avatar_dir),
		  "%s/.config/mrrobotos/mrsettings/account", home ? home : "/root");

	snprintf (avatar_path, sizeof (avatar_path),
		  "%s/avatar", avatar_dir);

	free (home);

	g_mkdir_with_parents (avatar_dir, 0755);

	/* CSS */
	GtkCssProvider *av_css = gtk_css_provider_new ();
	//    gtk_css_provider_load_from_string (av_css,
	//    ".avatar-frame {"
	//    "  border-radius: 50%;"
	//    "  border: 3px solid rgba(53,132,228,0.45);"
	//    "  background: rgba(0,0,0,0.08);"
	//    "}"
	//    "image.avatar-img, picture.avatar-img {"
	//    "  border-radius: 0;"
	//    "}"
	//    "button.avatar-edit {"
	//    "  border-radius: 50%;"
	//    "  padding: 6px;"
	//    "  min-width: 0; min-height: 0;"
	//    "}"
	//    ".users-section-title {"
	//    "  font-size: 0.72em; font-weight: 800;"
	//    "  color: rgba(0,0,0,0.35);"
	//    "  letter-spacing: 0.08em;"
	//    "}"
	//    ".users-card {"
	//    "  border-radius: 12px;"
	//    "  border: 1px solid rgba(0,0,0,0.10);"
	//    "  background: white;"
	//    "}"
	//    ".users-row-label { font-size: 0.95em; }"
	//    ".users-row-value { font-size: 0.9em; color: rgba(0,0,0,0.5); }"
	//);

	gtk_css_provider_load_from_string (av_css,
					   "button.avatar-edit {"
					   "  border-radius: 50%;"
					   "  padding: 8px;"
					   "  min-width: 0; min-height: 0;"
					   "}"
					   ".users-section-title {"
					   "  font-size: 0.72em; font-weight: 800;"
					   "  color: rgba(0,0,0,0.35);"
					   "  letter-spacing: 0.08em;"
					   "}"
					   ".users-card {"
					   "  border-radius: 12px;"
					   "  border: 1px solid rgba(0,0,0,0.10);"
					   "  background: white;"
					   "}"
					   ".users-row-label { font-size: 0.95em; }"
					   ".users-row-value { font-size: 0.9em; color: rgba(0,0,0,0.5); }"
					  );

	gtk_style_context_add_provider_for_display (
						    gdk_display_get_default (),
						    GTK_STYLE_PROVIDER (av_css),
						    GTK_STYLE_PROVIDER_PRIORITY_USER);
	g_object_unref (av_css);

	/* Outer scroll */
	GtkWidget *scroll = gtk_scrolled_window_new ();
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	GtkWidget *root = gtk_box_new (GTK_ORIENTATION_VERTICAL, 28);
	gtk_widget_set_margin_start  (root, 56);
	gtk_widget_set_margin_end    (root, 56);
	gtk_widget_set_margin_top    (root, 36);
	gtk_widget_set_margin_bottom (root, 36);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroll), root);

	/* ── AVATAR ── */
	GtkWidget *avatar_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_halign (avatar_box, GTK_ALIGN_CENTER);



	GtkWidget *avatar_da = av_circle_new (avatar_path, AVATAR_DRAW_AREA_SIZE);

	account_av_global = avatar_da;
	g_object_add_weak_pointer (G_OBJECT (avatar_da),
				   (gpointer*)&account_av_global);

	GtkWidget *av_overlay = gtk_overlay_new();
	gtk_widget_set_size_request(av_overlay, AVATAR_DRAW_AREA_SIZE, AVATAR_DRAW_AREA_SIZE);
	gtk_widget_set_halign      (av_overlay, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand     (av_overlay, FALSE);
	gtk_widget_set_vexpand     (av_overlay, FALSE);
	gtk_widget_set_halign       (av_overlay, GTK_ALIGN_CENTER);
	gtk_overlay_set_child       (GTK_OVERLAY (av_overlay), avatar_da);

	/* Outer fixed-size frame that clips to a circle */
	//GtkWidget *av_frame = gtk_frame_new (NULL);
	//gtk_widget_set_size_request (av_frame, 200, 200);
	//gtk_widget_set_hexpand     (av_frame, FALSE);
	//gtk_widget_set_vexpand     (av_frame, FALSE);
	//gtk_widget_set_overflow    (av_frame, GTK_OVERFLOW_HIDDEN);
	//gtk_widget_add_css_class   (av_frame, "avatar-frame");
	////gtk_widget_set_halign (av_frame, GTK_ALIGN_CENTER);
	////gtk_widget_add_css_class (av_frame, "avatar-frame");

	///* GtkPicture fills its allocation — no shrinking */
	//GtkWidget *avatar_pic;
	//if (g_file_test (avatar_path, G_FILE_TEST_EXISTS)) {
	//    GFile *gf = g_file_new_for_path (avatar_path);
	//    avatar_pic = gtk_picture_new_for_file (gf);
	//    g_object_unref (gf);
	//    gtk_picture_set_content_fit(GTK_PICTURE(avatar_pic), GTK_CONTENT_FIT_COVER);
	//} else {
	//    /* fallback: icon in a picture-sized box */
	//    avatar_pic = gtk_image_new_from_icon_name ("avatar-default-symbolic");
	//    gtk_image_set_pixel_size (GTK_IMAGE (avatar_pic), 120);
	//    gtk_widget_set_halign (avatar_pic, GTK_ALIGN_CENTER);
	//    gtk_widget_set_valign (avatar_pic, GTK_ALIGN_CENTER);
	//}

	//gtk_widget_set_size_request(avatar_pic, 200, 200);
	//gtk_widget_set_hexpand     (avatar_pic, FALSE);
	//gtk_widget_set_vexpand     (avatar_pic, FALSE);

	//gtk_frame_set_child  (GTK_FRAME(av_frame),   avatar_pic);
	//gtk_overlay_set_child(GTK_OVERLAY(av_overlay), av_frame);



	////gtk_widget_add_css_class    (avatar_pic, "avatar-img");
	////gtk_widget_set_size_request (avatar_pic, 180, 180);
	////gtk_widget_set_hexpand (avatar_pic, FALSE);
	////gtk_widget_set_vexpand (avatar_pic, FALSE);
	////gtk_widget_set_overflow (avatar_pic, GTK_OVERFLOW_HIDDEN);
	////if (GTK_IS_PICTURE (avatar_pic))
	////    gtk_picture_set_content_fit (GTK_PICTURE (avatar_pic), GTK_CONTENT_FIT_COVER);

	//account_av_global = avatar_pic;
	//g_object_add_weak_pointer(G_OBJECT(avatar_pic), (gpointer*)&account_av_global);

	///* Overlay so the edit button sits on top */
	////GtkWidget *av_overlay = gtk_overlay_new ();
	////gtk_widget_set_size_request (av_overlay, 180, 180);
	////gtk_widget_set_halign (av_overlay, GTK_ALIGN_CENTER);
	////gtk_overlay_set_child (GTK_OVERLAY (av_overlay), avatar_pic);

	GtkWidget *edit_btn = gtk_button_new_from_icon_name ("camera-photo-symbolic");
	gtk_widget_add_css_class    (edit_btn, "avatar-edit");
	gtk_widget_add_css_class    (edit_btn, "suggested-action");
	gtk_widget_set_halign       (edit_btn, GTK_ALIGN_END);
	gtk_widget_set_valign       (edit_btn, GTK_ALIGN_END);
	gtk_widget_set_tooltip_text (edit_btn, "Change profile picture");
	gtk_overlay_add_overlay     (GTK_OVERLAY (av_overlay), edit_btn);

	/* AvData still points to avatar_pic so the callback can update it */
	//AvData *avd = g_new0 (AvData, 1);
	//avd->av = avatar_pic;
	//g_strlcpy (avd->apath, avatar_path, sizeof (avd->apath));
	//g_signal_connect_data (edit_btn, "clicked",
	//                       G_CALLBACK (av_edit_clicked_cb),
	//                       avd, av_data_destroy_cb, 0);

	//gtk_box_append (GTK_BOX (avatar_box), av_overlay);

	AvData *avd = g_new0 (AvData, 1);
	avd->av = avatar_da;
	g_strlcpy (avd->apath, avatar_path, sizeof (avd->apath));
	g_signal_connect_data (edit_btn, "clicked",
			       G_CALLBACK (av_edit_clicked_cb),
			       avd, av_data_destroy_cb, 0);

	gtk_box_append (GTK_BOX (avatar_box), av_overlay);

	const char *display = (rname && strlen (rname) && strcmp (rname, "Unknown"))
		? rname : uname;
	GtkWidget *disp_name = gtk_label_new (display);
	gtk_widget_add_css_class (disp_name, "title-2");
	gtk_box_append (GTK_BOX (avatar_box), disp_name);

	GtkWidget *uname_sub = gtk_label_new (uname);
	gtk_widget_add_css_class (uname_sub, "dim-label");
	gtk_box_append (GTK_BOX (avatar_box), uname_sub);

	gtk_box_append (GTK_BOX (root), avatar_box);
	//GtkWidget *avatar_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
	//gtk_widget_set_halign (avatar_box, GTK_ALIGN_CENTER);

	//GtkWidget *av_overlay = gtk_overlay_new ();
	//gtk_widget_set_halign (av_overlay, GTK_ALIGN_CENTER);

	//GtkWidget *avatar = NULL;
	//if (g_file_test (avatar_path, G_FILE_TEST_EXISTS)) {
	//    GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale (
	//                        avatar_path, 180, 180, TRUE, NULL);
	//    if (pb) {
	//        avatar = gtk_image_new_from_pixbuf (pb);
	//        g_object_unref (pb);
	//    }
	//}
	//if (!avatar) {
	//    avatar = gtk_image_new_from_icon_name ("avatar-default-symbolic");
	//    gtk_image_set_pixel_size (GTK_IMAGE (avatar), 180);
	//}
	//gtk_widget_set_size_request (avatar, 180, 180);
	//gtk_widget_add_css_class    (avatar, "avatar-img");
	//gtk_overlay_set_child       (GTK_OVERLAY (av_overlay), avatar);

	//GtkWidget *edit_btn = gtk_button_new_from_icon_name ("camera-photo-symbolic");
	//gtk_widget_add_css_class    (edit_btn, "avatar-edit");
	//gtk_widget_add_css_class    (edit_btn, "suggested-action");
	//gtk_widget_set_halign       (edit_btn, GTK_ALIGN_END);
	//gtk_widget_set_valign       (edit_btn, GTK_ALIGN_END);
	//gtk_widget_set_tooltip_text (edit_btn, "Change profile picture");
	//gtk_overlay_add_overlay     (GTK_OVERLAY (av_overlay), edit_btn);

	//AvData *avd = g_new0 (AvData, 1);
	//avd->av = avatar;
	//g_strlcpy (avd->apath, avatar_path, sizeof (avd->apath));
	//g_signal_connect_data (edit_btn, "clicked",
	//                       G_CALLBACK (av_edit_clicked_cb),
	//                       avd, av_data_destroy_cb, 0);

	//gtk_box_append (GTK_BOX (avatar_box), av_overlay);

	//const char *display = (rname && strlen (rname) && strcmp (rname, "Unknown"))
	//                      ? rname : uname;
	//GtkWidget *disp_name = gtk_label_new (display);
	//gtk_widget_add_css_class (disp_name, "title-2");
	//gtk_box_append (GTK_BOX (avatar_box), disp_name);

	//GtkWidget *uname_sub = gtk_label_new (uname);
	//gtk_widget_add_css_class (uname_sub, "dim-label");
	//gtk_box_append (GTK_BOX (avatar_box), uname_sub);

	//gtk_box_append (GTK_BOX (root), avatar_box);

	/* ── ACCOUNT INFO (read-only) ── */
	gtk_box_append (GTK_BOX (root), section_title ("ACCOUNT"));

	GtkWidget *acc_card = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class (acc_card, "users-card");

	char hostname[256] = "unknown";
	gethostname (hostname, sizeof (hostname));

	const char *shell   = getenv ("SHELL");
	char       *homedir = get_home_env ();

	make_info_row (acc_card, "Username",       uname,                       TRUE);
	make_info_row (acc_card, "Hostname",       hostname,                    TRUE);
	make_info_row (acc_card, "Shell",          shell ? shell : "/bin/sh",   TRUE);
	make_info_row (acc_card, "Home Directory", homedir ? homedir : "/root", FALSE);
	free (homedir);

	gtk_box_append (GTK_BOX (root), acc_card);

	/* ── PERSONAL INFO (editable) ── */
	gtk_box_append (GTK_BOX (root), section_title ("PERSONAL INFO"));

	GtkWidget *pi   = card_inner (root, 14);
	GtkWidget *grid = gtk_grid_new ();
	gtk_grid_set_row_spacing    (GTK_GRID (grid), 14);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 20);
	gtk_widget_set_hexpand      (grid, TRUE);

	GtkWidget *fullname_row = make_entry_row ("Full Name",    "John Doe",         FALSE);
	GtkWidget *email_row    = make_entry_row ("Email",        "user@example.com", FALSE);
	GtkWidget *phone_row    = make_entry_row ("Phone",        "+1 555 000 0000",  FALSE);
	GtkWidget *org_row      = make_entry_row ("Organisation", "Company / School", FALSE);
	GtkWidget *addr_row     = make_entry_row ("Address",      "123 Main St",      FALSE);
	GtkWidget *city_row     = make_entry_row ("City",         "New York",         FALSE);
	GtkWidget *country_row  = make_entry_row ("Country",      "United States",    FALSE);
	GtkWidget *website_row  = make_entry_row ("Website",      "https://",         FALSE);

	gtk_grid_attach (GTK_GRID (grid), fullname_row, 0, 0, 1, 1);
	gtk_grid_attach (GTK_GRID (grid), email_row,    1, 0, 1, 1);
	gtk_grid_attach (GTK_GRID (grid), phone_row,    0, 1, 1, 1);
	gtk_grid_attach (GTK_GRID (grid), org_row,      1, 1, 1, 1);
	gtk_grid_attach (GTK_GRID (grid), addr_row,     0, 2, 1, 1);
	gtk_grid_attach (GTK_GRID (grid), city_row,     1, 2, 1, 1);
	gtk_grid_attach (GTK_GRID (grid), country_row,  0, 3, 1, 1);
	gtk_grid_attach (GTK_GRID (grid), website_row,  1, 3, 1, 1);
	gtk_box_append (GTK_BOX (pi), grid);

	GtkWidget *save_pi = gtk_button_new_with_label ("Save Info");
	gtk_widget_add_css_class (save_pi, "suggested-action");
	gtk_widget_set_halign    (save_pi, GTK_ALIGN_END);
	gtk_box_append (GTK_BOX (pi), save_pi);

	PiData *pid   = g_new0 (PiData, 1);
	pid->fullname = gtk_widget_get_last_child (fullname_row);
	pid->email    = gtk_widget_get_last_child (email_row);
	pid->phone    = gtk_widget_get_last_child (phone_row);
	pid->org      = gtk_widget_get_last_child (org_row);
	pid->addr     = gtk_widget_get_last_child (addr_row);
	pid->city     = gtk_widget_get_last_child (city_row);
	pid->country  = gtk_widget_get_last_child (country_row);
	pid->website  = gtk_widget_get_last_child (website_row);
	g_strlcpy (pid->dir, avatar_dir, sizeof (pid->dir));

	g_object_set_data(G_OBJECT(edit_btn), "pi-data", pid);

	/* Load saved personal info */
	{
		char info_path[600];
		snprintf (info_path, sizeof (info_path), "%s/info", avatar_dir);
		GKeyFile *kf = g_key_file_new ();
		if (g_key_file_load_from_file (kf, info_path, G_KEY_FILE_NONE, NULL)) {
			char *v;
#define LOAD(widget, key) \
			v = g_key_file_get_string (kf, "Personal", key, NULL); \
			if (v) { gtk_editable_set_text (GTK_EDITABLE (widget), v); g_free (v); }
			LOAD (pid->fullname, "FullName")
				LOAD (pid->email,    "Email")
				LOAD (pid->phone,    "Phone")
				LOAD (pid->org,      "Organisation")
				LOAD (pid->addr,     "Address")
				LOAD (pid->city,     "City")
				LOAD (pid->country,  "Country")
				LOAD (pid->website,  "Website")
#undef LOAD
		}
		g_key_file_free (kf);
	}

	g_signal_connect_data (save_pi, "clicked",
			       G_CALLBACK (save_pi_clicked_cb),
			       pid, pi_data_destroy_cb, 0);

	return scroll;
}

/* Helper — call this in activate() User row block instead */
	static GtkWidget *
make_sidebar_avatar ()
{
	char *home = get_home_env ();
	char  saved[600];
	snprintf (saved, sizeof (saved),
		  "%s/.config/mrrobotos/mrsettings/account/avatar", home ? home : "/root");
	free (home);

	//const char *use_path = NULL;
	//if      (g_file_test (saved,     G_FILE_TEST_EXISTS)) use_path = saved;
	//else if (g_file_test (face_path, G_FILE_TEST_EXISTS)) use_path = face_path;

	//GtkWidget *av;
	//if (g_file_test(saved, G_FILE_TEST_EXISTS)) {
	//    GFile *_gf = g_file_new_for_path(saved);
	//    av = gtk_picture_new_for_file(_gf);
	//    g_object_unref(_gf);
	//    gtk_picture_set_content_fit(GTK_PICTURE(av), GTK_CONTENT_FIT_COVER);
	//    gtk_widget_set_size_request(av, AVATAR_SIDEBAR_SIZE, AVATAR_SIDEBAR_SIZE);
	//    gtk_widget_set_overflow(av, GTK_OVERFLOW_HIDDEN);
	//} else {
	//    av = gtk_image_new_from_icon_name("avatar-default-symbolic");
	//    gtk_image_set_pixel_size(GTK_IMAGE(av), AVATAR_SIDEBAR_SIZE);
	//}
	//gtk_widget_set_size_request(av, AVATAR_SIDEBAR_SIZE, AVATAR_SIDEBAR_SIZE);
	//gtk_widget_set_overflow(av, GTK_OVERFLOW_HIDDEN);
	//gtk_widget_set_halign(av, GTK_ALIGN_CENTER);

	///* circular clip via CSS */
	//GtkCssProvider *cp = gtk_css_provider_new();
	//gtk_css_provider_load_from_string(cp,
	//    "picture.sb-avatar, image.sb-avatar {"
	//    "  border-radius: 50%;"
	//    "  min-width:  112px;"
	//    "  min-height: 112px;"
	//    "}"
	//);

	//gtk_style_context_add_provider_for_display(
	//    gdk_display_get_default(),
	//    GTK_STYLE_PROVIDER(cp),
	//    GTK_STYLE_PROVIDER_PRIORITY_USER);
	//g_object_unref(cp);
	//gtk_widget_add_css_class(av, "sb-avatar");

	GtkWidget *av = av_circle_new (
				       g_file_test (saved, G_FILE_TEST_EXISTS) ? saved : NULL, AVATAR_SIDEBAR_SIZE); /* diameter in pixels — change this number to resize */

	sidebar_av_global = av;
	g_object_add_weak_pointer(G_OBJECT(av), (gpointer*)&sidebar_av_global);
	return av;
	//gtk_widget_set_halign(av, GTK_ALIGN_CENTER);
	//return av;

	//GtkWidget *av;
	//if (use_path) {
	//    GFile *_gf = g_file_new_for_path (use_path);
	//    av = gtk_picture_new_for_file (_gf);
	//    g_object_unref (_gf);
	//    gtk_picture_set_content_fit (GTK_PICTURE (av), GTK_CONTENT_FIT_COVER);
	//    gtk_widget_set_size_request (av, 64, 64);
	//    gtk_widget_set_overflow (av, GTK_OVERFLOW_HIDDEN);
	//} else {
	//    av = gtk_image_new_from_icon_name ("avatar-default-symbolic");
	//    gtk_image_set_pixel_size (GTK_IMAGE (av), 64);
	//}
	//gtk_widget_set_halign (av, GTK_ALIGN_CENTER);
	//return av;
}


static gboolean sidebar_filter_func(GtkListBoxRow *row, gpointer ud) {
	GtkSearchEntry *e=GTK_SEARCH_ENTRY(ud);
	const char *s=gtk_editable_get_text(GTK_EDITABLE(e));
	if(!s||!*s)return TRUE;
	if(!gtk_list_box_row_get_selectable(row))return TRUE;
	GtkWidget *w=gtk_widget_get_first_child(gtk_list_box_row_get_child(row));
	while(w){
		if(GTK_IS_LABEL(w)){
			char *tl=g_utf8_strdown(gtk_label_get_text(GTK_LABEL(w)),-1);
			char *sl=g_utf8_strdown(s,-1);
			gboolean m=strstr(tl,sl)!=NULL;
			g_free(tl);g_free(sl);return m;
		}
		w=gtk_widget_get_next_sibling(w);
	}
	return TRUE;
}

static void sidebar_search_changed(GtkSearchEntry *e, gpointer ud) {
	gtk_list_box_invalidate_filter(GTK_LIST_BOX(ud));
}

static void sidebar_row_selected(GtkListBox *box, GtkListBoxRow *row, gpointer ud) {
	if (!row) return;
	const char *name = g_object_get_data(G_OBJECT(row), "page-name");
	if (!name) return;
	GtkStack *stack = GTK_STACK(ud);

	for (int i = 0; lazy_pages[i].name; i++) {
		if (strcmp(lazy_pages[i].name, name) != 0) continue;
		if (!lazy_pages[i].built) {
			lazy_pages[i].built = TRUE;
			GtkWidget *real_page;
			if (lazy_pages[i].builder) {
				real_page = lazy_pages[i].builder();
			} else {
				real_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
				gtk_widget_set_valign(real_page, GTK_ALIGN_CENTER);
				gtk_widget_set_halign(real_page, GTK_ALIGN_CENTER);
				GtkWidget *ic = gtk_image_new_from_icon_name("preferences-system-sharing-symbolic");
				gtk_image_set_pixel_size(GTK_IMAGE(ic), 64);
				gtk_box_append(GTK_BOX(real_page), ic);
				gtk_box_append(GTK_BOX(real_page), gtk_label_new(name));
			}
			GtkWidget *old = gtk_stack_get_child_by_name(stack, name);
			if (old) gtk_stack_remove(stack, old);
			gtk_stack_add_named(stack, real_page, name);
		}
		gtk_stack_set_visible_child_name(stack, name);
		return;
	}
	/* User page — always built */
	gtk_stack_set_visible_child_name(stack, name);
}

//static void sidebar_row_selected(GtkListBox *lb, GtkListBoxRow *row, gpointer ud) {
//    if(!row)return;
//    const char *name=g_object_get_data(G_OBJECT(row),"page-name");
//    if(name)gtk_stack_set_visible_child_name(GTK_STACK(ud),name);
//}

static gboolean sidebar_keynav(GtkWidget *w, guint kv, guint kc,
			       GdkModifierType st, gpointer ud) {
	if(kv!=GDK_KEY_Up&&kv!=GDK_KEY_Down)return FALSE;
	GtkListBox *lb=GTK_LIST_BOX(w);
	GtkListBoxRow *cur=gtk_list_box_get_selected_row(lb);
	int total=0; while(gtk_list_box_get_row_at_index(lb,total))total++;
	if(!cur){
		for(int i=0;i<total;i++){GtkListBoxRow *r=gtk_list_box_get_row_at_index(lb,i);
			if(r&&gtk_list_box_row_get_selectable(r)){gtk_list_box_select_row(lb,r);return TRUE;}}
		return TRUE;
	}
	int step=(kv==GDK_KEY_Down)?1:-1,next=gtk_list_box_row_get_index(cur)+step;
	while(next>=0&&next<total){GtkListBoxRow *r=gtk_list_box_get_row_at_index(lb,next);
		if(r&&gtk_list_box_row_get_selectable(r)){gtk_list_box_select_row(lb,r);return TRUE;}
		next+=step;}
	return TRUE;
}
static void append_group_label(GtkListBox *lb, const char *text) {
	GtkWidget *lbl=gtk_label_new(text);
	gtk_widget_set_halign(lbl,GTK_ALIGN_START);
	gtk_widget_set_margin_start(lbl,12); gtk_widget_set_margin_top(lbl,14);
	gtk_widget_set_margin_bottom(lbl,4); gtk_widget_set_margin_end(lbl,12);
	gtk_widget_add_css_class(lbl,"sb-group-label");
	GtkWidget *row=gtk_list_box_row_new();
	gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row),lbl);
	gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row),FALSE);
	gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row),FALSE);
	gtk_widget_add_css_class(GTK_WIDGET(row),"sb-skip");
	gtk_list_box_append(lb,row);
}
static void append_separator(GtkListBox *lb) {
	GtkWidget *sp=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_widget_set_size_request(sp,-1,8);
	GtkWidget *row=gtk_list_box_row_new();
	gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row),sp);
	gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row),FALSE);
	gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row),FALSE);
	gtk_widget_add_css_class(GTK_WIDGET(row),"sb-skip");
	gtk_list_box_append(lb,row);
}
static void append_page_row(GtkListBox *lb, const char *title,
			    const char *icon_name, int icon_size, int pad_v) {
	GtkWidget *rb=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);
	gtk_widget_set_margin_start(rb,12); gtk_widget_set_margin_end(rb,12);
	gtk_widget_set_margin_top(rb,pad_v); gtk_widget_set_margin_bottom(rb,pad_v);
	if(icon_name){
		GtkWidget *img=gtk_image_new_from_icon_name(icon_name);
		gtk_image_set_pixel_size(GTK_IMAGE(img),icon_size);
		gtk_box_append(GTK_BOX(rb),img);
	}
	GtkWidget *lbl=gtk_label_new(title);
	gtk_widget_set_halign(lbl,GTK_ALIGN_START); gtk_widget_set_hexpand(lbl,TRUE);
	gtk_box_append(GTK_BOX(rb),lbl);
	GtkWidget *row=gtk_list_box_row_new();
	gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row),rb);
	gtk_widget_add_css_class(GTK_WIDGET(row),"sb-row");
	g_object_set_data_full(G_OBJECT(row),"page-name",g_strdup(title),g_free);
	gtk_list_box_append(lb,row);
}

/* ================================================================== */
/* Main / activate                                                      */
/* ================================================================== */
int main(int argc, char **argv) {

	const char *jump_to = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			printf(
			       "Usage: mrsettings [OPTION]\n"
			       "\n"
			       "MrRobotOS System Settings\n"
			       "\n"
			       "Options:\n"
			       "  --account            Open directly to Account settings\n"
			       "  --wifi               Open directly to Wi-Fi settings\n"
			       "  --bluetooth          Open directly to Bluetooth settings\n"
			       "  --vpn                Open directly to VPN settings\n"
			       "  --displays           Open directly to Displays settings\n"
			       "  --sound              Open directly to Sound settings\n"
			       "  --keyboard           Open directly to Keyboard settings\n"
			       "  --battery            Open directly to Battery settings\n"
			       "  --keybindings        Open directly to Keybindings\n"
			       "  --clicks             Open directly to Clicks & Buttons\n"
			       "  --shortcuts          Open directly to Terminal Shortcuts\n"
			       "  --appearance         Open directly to Appearance settings\n"
			       "  --wallpaper          Open directly to Wallpaper settings\n"
			       "  --brightness         Open directly to Brightness settings\n"
			       "  --notifications      Open directly to Notifications settings\n"
			       "  --users              Open directly to Users & Groups settings\n"
			       "  --datetime           Open directly to Date & Time settings\n"
			       "  --region             Open directly to Region & Language settings\n"
			       "  --updates            Open directly to Software & Updates settings\n"
			       "  --sharing            Open directly to Sharing settings\n"
			       "  --applications       Open directly to Applications settings\n"
			       "  --about              Open directly to About\n"
			       "  -h, --help           Show this help message\n"
			       );
			return 0;

		}
		else if (!strcmp(argv[i], "--account"))       jump_to = "User";
		else if (!strcmp(argv[i], "--wifi"))          jump_to = "Wi-Fi";
		else if (!strcmp(argv[i], "--bluetooth"))     jump_to = "Bluetooth";
		else if (!strcmp(argv[i], "--vpn"))           jump_to = "VPN";
		else if (!strcmp(argv[i], "--displays"))      jump_to = "Displays";
		else if (!strcmp(argv[i], "--sound"))         jump_to = "Sound";
		else if (!strcmp(argv[i], "--keyboard"))      jump_to = "Keyboard";
		else if (!strcmp(argv[i], "--battery"))       jump_to = "Battery";
		else if (!strcmp(argv[i], "--appearance"))    jump_to = "Appearance";
		else if (!strcmp(argv[i], "--wallpaper"))     jump_to = "Wallpaper";
		else if (!strcmp(argv[i], "--brightness"))    jump_to = "Brightness";
		else if (!strcmp(argv[i], "--notifications")) jump_to = "Notifications";
		else if (!strcmp(argv[i], "--users"))         jump_to = "Users & Groups";
		else if (!strcmp(argv[i], "--datetime"))      jump_to = "Date & Time";
		else if (!strcmp(argv[i], "--region"))        jump_to = "Region & Language";
		else if (!strcmp(argv[i], "--updates"))       jump_to = "Software & Updates";
		else if (!strcmp(argv[i], "--sharing"))       jump_to = "Sharing";
		else if (!strcmp(argv[i], "--applications"))  jump_to = "Applications";
		else if (!strcmp(argv[i], "--about"))         jump_to = "About";
		else if (!strcmp(argv[i], "--keybindings"))   jump_to = "Keybindings";
		else if (!strcmp(argv[i], "--shortcuts"))     jump_to = "Shortcuts";
		else if (!strcmp(argv[i], "--clicks"))        jump_to = "Clicks & Buttons";
		else {
			fprintf(stderr, "mrsettings: unknown option '%s'\n", argv[i]);
			fprintf(stderr, "Try 'mrsettings --help' for more information.\n");
			return 1;
		}
	}


	GtkApplication *app=gtk_application_new("org.mrrobotos.mrsettings",G_APPLICATION_NON_UNIQUE);
	if (jump_to)
		g_object_set_data(G_OBJECT(app), "jump-to", (gpointer)jump_to);
	g_signal_connect(app,"activate",G_CALLBACK(activate),NULL);
	int status=g_application_run(G_APPLICATION(app),1,argv);
	//int status=g_application_run(G_APPLICATION(app),argc,argv);
	g_object_unref(app);
	return status;
}

static void
on_window_realize_icon(GtkWidget *widget, gpointer ud)
{
    (void)ud;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wunused-parameter"
    GdkPixbuf *pb = gdk_pixbuf_new_from_file(
        "/usr/share/icons/mrrobotos/scalable/apps/mrsettings.png", NULL);
    if (!pb) return;

    int w = gdk_pixbuf_get_width(pb);
    int h = gdk_pixbuf_get_height(pb);
    guchar *pixels   = gdk_pixbuf_get_pixels(pb);
    int     channels = gdk_pixbuf_get_n_channels(pb);
    int     rowstride= gdk_pixbuf_get_rowstride(pb);
    unsigned long *data = g_malloc((2 + w * h) * sizeof(unsigned long));
    data[0] = w; data[1] = h;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            guchar *p = pixels + y * rowstride + x * channels;
            guchar a = channels == 4 ? p[3] : 255;
            data[2 + y*w + x] = ((unsigned long)a    << 24) |
                                 ((unsigned long)p[0] << 16) |
                                 ((unsigned long)p[1] <<  8) |
                                  (unsigned long)p[2];
        }
    GdkSurface *surface  = gtk_native_get_surface(GTK_NATIVE(widget));
    GdkDisplay *display  = gdk_surface_get_display(surface);
    Display    *xdisplay = gdk_x11_display_get_xdisplay(display);
    Window      xwindow  = gdk_x11_surface_get_xid(surface);
    Atom net_wm_icon = XInternAtom(xdisplay, "_NET_WM_ICON", False);
    XChangeProperty(xdisplay, xwindow, net_wm_icon,
        XA_CARDINAL, 32, PropModeReplace,
        (unsigned char*)data, 2 + w * h);
    XFlush(xdisplay);
    g_free(data);
    g_object_unref(pb);
#pragma GCC diagnostic pop
}

static void activate(GtkApplication *app, gpointer user_data) {
	if (!window) {
		const char *username  = g_get_user_name();
		const char *real_name = g_get_real_name();
		//char *home=get_home_env();
		//char avatar_path[512];
		//snprintf(avatar_path,sizeof(avatar_path),"%s/.face",home?home:"/root");
		//free(home);

		window=gtk_application_window_new(app);
		gtk_window_set_resizable(GTK_WINDOW(window),TRUE);
		gtk_window_set_default_size(GTK_WINDOW(window),1100,700);
		GtkWidget *header=gtk_header_bar_new();
		gtk_window_set_titlebar(GTK_WINDOW(window),header);
		gtk_window_set_title(GTK_WINDOW(window),"Mr.Settings");
		g_signal_connect(window, "realize", G_CALLBACK(on_window_realize_icon), NULL);
		g_object_add_weak_pointer(G_OBJECT(window),(gpointer*)&window);

		/* CSS */
		GtkCssProvider *css=gtk_css_provider_new();
		gtk_css_provider_load_from_string(css,
						  "listbox.sb-list{background:white;}"
						  "listbox.sb-list>row{background:white;outline:none;box-shadow:none;border:none;}"
						  "listbox.sb-list>row.sb-row{background:white;color:black;}"
						  "listbox.sb-list>row.sb-row:hover{background:rgba(0,0,0,0.08);color:black;}"
						  "listbox.sb-list>row.sb-row:selected{background:#3584e4;color:white;}"
						  "listbox.sb-list>row.sb-row:selected label{color:white;}"
						  "listbox.sb-list>row.sb-row:selected image{color:white;-gtk-icon-style:symbolic;}"
						  "listbox.sb-list>row.sb-row:selected:hover{background:#1c71d8;color:white;}"
						  "listbox.sb-list>row.sb-skip{background:white;outline:none;box-shadow:none;border:none;}"
						  "listbox.sb-list>row.sb-skip:hover{background:white;}"
						  "listbox.sb-list>row.sb-skip:focus{background:white;outline:none;box-shadow:none;}"
						  "listbox.sb-list>row.sb-skip label{font-size:0.75em;font-weight:bold;color:#888;}"
						  "listbox.sb-list>row.sb-skip label.sb-group-label{font-size:0.7em;font-weight:800;color:rgba(0,0,0,0.35);}"
						  "listbox.wifi-net-list {"
						  "  border: 1px solid rgba(0,0,0,0.12);"
						  "  border-radius: 12px;"
						  "  background: white;"
						  "}"
						  "listbox.wifi-net-list > row {"
						  "  background: white;"
						  "}"
						  "listbox.wifi-net-list > row:first-child {"
						  "  border-radius: 12px 12px 0 0;"
						  "}"
						  "listbox.wifi-net-list > row:last-child {"
						  "  border-radius: 0 0 12px 12px;"
						  "}"
						  "listbox.wifi-net-list > row header {"
						  "  margin: 0;"
						  "}"
						  ".wp-picture{border-radius:16px;}"
						  ".wp-browse-card{border:2px dashed rgba(0,0,0,0.18);border-radius:10px;}"
						  ".wp-top-card{border-radius:16px;border:1px solid rgba(0,0,0,0.10);"
						  "background:white;}"
						  ".wp-preview-img{border-radius:0px;min-width:340px;min-height:210px;}"
						  ".wp-recents-card{border-radius:14px;border:1px solid rgba(0,0,0,0.10);"
						  "background:white;}"
						  ".wp-thumb-btn{border-radius:10px;padding:0;}"
						  ".wp-thumb-btn:hover{background:rgba(53,132,228,0.18);}"
						  ".wp-thumb{border-radius:10px;}"
						  ".wp-thumb-btn picture{border-radius:10px;}"
						  ".picker-item{border-radius:8px;padding:4px;}"
						  ".picker-item:hover{background:rgba(0,0,0,0.08);}"
						  ".picker-preview{border-radius:10px;border:1px solid rgba(0,0,0,0.15);}"
						  ".path-btn{padding:2px 6px;font-size:0.9em;}"
						  ".signal-bar-active{background:#3584e4;border-radius:2px;}"
						  ".signal-bar-dim{background:rgba(0,0,0,0.15);border-radius:2px;}"
						  ".wifi-active-name{font-weight:bold;color:#3584e4;}"
						  ".caption{font-size:0.8em;}"
						  ".bat-box{border-radius:12px;border:1px solid rgba(0,0,0,0.12);background:white;}"
						  ".bat-card{border-radius:8px;border:1px solid rgba(0,0,0,0.09);background:white;}"
						  ".bat-gov-btn{border-radius:10px;border:1px solid rgba(0,0,0,0.12);}"
						  ".bat-gov-btn:disabled{background:rgba(53,132,228,0.12);border-color:#3584e4;}"
						  ".bat-gov-btn:disabled label{color:#1c71d8;}"
						  ".bat-gov-btn:disabled image{color:#1c71d8;}"
						  ".disp-canvas-wrap{background:rgba(0,0,0,0.03);}"
						  ".kbd-key{"
						  "  background:white;"
						  "  border:1px solid rgba(0,0,0,0.28);"
						  "  border-bottom:4px solid rgba(0,0,0,0.35);"
						  "  border-radius:7px;"
						  "  padding:6px 14px;"
						  "  font-size:1.05em;"
						  "  font-weight:600;"
						  "  box-shadow:0 2px 4px rgba(0,0,0,0.12);"
						  "  color:#1a1a1a;"
						  "  min-width:28px;"
						  "}"
						  ".nav-action{"
						  "  font-size:1.0em;"
						  "  font-weight:600;"
						  "  color:#1a1a1a;"
						  "}"
						  ".nav-detail{"
						  "  font-size:0.85em;"
						  "  color:rgba(0,0,0,0.55);"
						  "}"

						  );
		gtk_style_context_add_provider_for_display(
							   gdk_display_get_default(),
							   GTK_STYLE_PROVIDER(css),
							   GTK_STYLE_PROVIDER_PRIORITY_USER);

		g_object_unref(css);
		GtkWidget *box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
		GtkWidget *sv=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
		gtk_widget_set_size_request(sv,240,-1);
		GtkWidget *se=gtk_search_entry_new();
		gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(se), "Search");
		gtk_widget_set_margin_start(se,10); gtk_widget_set_margin_end(se,10);
		gtk_widget_set_margin_top(se,10);   gtk_widget_set_margin_bottom(se,6);
		gtk_box_append(GTK_BOX(sv),se);
		gtk_box_append(GTK_BOX(sv),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
		GtkWidget *scr=gtk_scrolled_window_new();
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
		gtk_widget_set_vexpand(scr,TRUE);
		sidebar_listbox=gtk_list_box_new();
		gtk_widget_add_css_class(sidebar_listbox,"sb-list");
		gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(sidebar_listbox),TRUE);
		gtk_widget_set_can_focus(sidebar_listbox,TRUE);
		gtk_list_box_set_selection_mode(GTK_LIST_BOX(sidebar_listbox),GTK_SELECTION_SINGLE);
		gtk_list_box_set_filter_func(GTK_LIST_BOX(sidebar_listbox),sidebar_filter_func,se,NULL);
		GtkEventController *key=gtk_event_controller_key_new();
		gtk_event_controller_set_propagation_phase(key,GTK_PHASE_CAPTURE);
		g_signal_connect(key,"key-pressed",G_CALLBACK(sidebar_keynav),NULL);
		gtk_widget_add_controller(sidebar_listbox,key);
		GtkWidget *stack=gtk_stack_new();
		gtk_stack_set_transition_type(GTK_STACK(stack),GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN);
		gtk_widget_set_hexpand(stack,TRUE); gtk_widget_set_vexpand(stack,TRUE);
		g_signal_connect(se,"search-changed",G_CALLBACK(sidebar_search_changed),sidebar_listbox);
		g_signal_connect(sidebar_listbox,"row-selected",G_CALLBACK(sidebar_row_selected),stack);

		{
			GtkWidget *ub = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
			gtk_widget_set_margin_start  (ub, 12);
			gtk_widget_set_margin_end    (ub, 12);
			gtk_widget_set_margin_top    (ub, 16);
			gtk_widget_set_margin_bottom (ub, 16);

			//GtkWidget *av;
			//if (g_file_test (avatar_path, G_FILE_TEST_EXISTS))
			//    av = gtk_image_new_from_file (avatar_path);
			//else
			//    av = gtk_image_new_from_icon_name ("avatar-default-symbolic");
			//gtk_image_set_pixel_size (GTK_IMAGE (av), 64);
			//gtk_widget_set_halign (av, GTK_ALIGN_CENTER);
			GtkWidget *av = make_sidebar_avatar();
			gtk_box_append (GTK_BOX (ub), av);

			char dname[256];
			if (real_name && strlen (real_name) > 0 && strcmp (real_name, "Unknown") != 0)
				snprintf (dname, sizeof (dname), "%s", real_name);
			else
				snprintf (dname, sizeof (dname), "%s", username);

			GtkWidget *nl = gtk_label_new (dname);
			gtk_widget_set_halign    (nl, GTK_ALIGN_CENTER);
			gtk_widget_add_css_class (nl, "title-4");
			gtk_box_append (GTK_BOX (ub), nl);

			GtkWidget *ul = gtk_label_new (username);
			gtk_widget_set_halign    (ul, GTK_ALIGN_CENTER);
			gtk_widget_add_css_class (ul, "dim-label");
			gtk_box_append (GTK_BOX (ub), ul);

			GtkWidget *ur = gtk_list_box_row_new ();
			gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (ur), ub);
			gtk_widget_add_css_class   (GTK_WIDGET (ur), "sb-row");
			g_object_set_data_full     (G_OBJECT (ur), "page-name",
						    g_strdup ("User"), g_free);
			gtk_list_box_append (GTK_LIST_BOX (sidebar_listbox), ur);

			gtk_stack_add_named (GTK_STACK (stack), account_settings (), "User");
		}

		/* --- Sidebar items + stack pages --- */
		append_separator(GTK_LIST_BOX(sidebar_listbox));
		append_group_label(GTK_LIST_BOX(sidebar_listbox), "CONNECTIVITY");
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Wi-Fi",     "network-wireless-symbolic",  16, 8);
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Bluetooth", "bluetooth-symbolic",         16, 8);
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "VPN",       "network-vpn-symbolic",       16, 8);

		append_separator(GTK_LIST_BOX(sidebar_listbox));
		append_group_label(GTK_LIST_BOX(sidebar_listbox), "HARDWARE");
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Displays",  "video-display-symbolic",              16, 8);
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Sound",     "audio-volume-high-symbolic",          16, 8);
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Keyboard",  "input-keyboard-symbolic",             16, 8);
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Battery",   "battery-symbolic",                    16, 8);

		append_separator(GTK_LIST_BOX(sidebar_listbox));
		append_group_label(GTK_LIST_BOX(sidebar_listbox), "NAVIGATION");
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Keybindings",    "preferences-desktop-keyboard-shortcuts-symbolic",        16, 8);
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Clicks & Buttons",   "input-mouse-symbolic",           16, 8);
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Shortcuts", "utilities-terminal-symbolic",    16, 8);

		append_separator(GTK_LIST_BOX(sidebar_listbox));
		append_group_label(GTK_LIST_BOX(sidebar_listbox), "PERSONALISATION");
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Appearance",    "preferences-desktop-appearance-symbolic", 16, 8);
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Wallpaper",     "preferences-desktop-wallpaper-symbolic",  16, 8);
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Brightness",    "display-brightness-symbolic",             16, 8);
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Notifications", "preferences-system-notifications-symbolic", 16, 8);

		append_separator(GTK_LIST_BOX(sidebar_listbox));
		append_group_label(GTK_LIST_BOX(sidebar_listbox), "SYSTEM");
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Users & Groups",     "system-users-symbolic",               16, 8);
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Date & Time",        "preferences-system-time-symbolic",    16, 8);
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Region & Language",  "preferences-desktop-locale-symbolic", 16, 8);
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Software & Updates", "software-update-available-symbolic",  16, 8);

		append_separator(GTK_LIST_BOX(sidebar_listbox));
		append_group_label(GTK_LIST_BOX(sidebar_listbox), "PRIVACY & SECURITY");
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Sharing", "preferences-system-sharing-symbolic", 16, 8);

		append_separator(GTK_LIST_BOX(sidebar_listbox));
		append_group_label(GTK_LIST_BOX(sidebar_listbox), "APPLICATIONS");
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "Applications", "preferences-system-symbolic", 16, 8);

		append_separator(GTK_LIST_BOX(sidebar_listbox));
		append_group_label(GTK_LIST_BOX(sidebar_listbox), "ABOUT");
		append_page_row(GTK_LIST_BOX(sidebar_listbox), "About", "help-about-symbolic", 16, 8);

		/* stack pages — real implementations */
		//gtk_stack_add_named(GTK_STACK(stack), wifi_settings(),          "Wi-Fi");
		//gtk_stack_add_named(GTK_STACK(stack), bluetooth_settings(),     "Bluetooth");
		//gtk_stack_add_named(GTK_STACK(stack), vpn_settings(),           "VPN");
		//gtk_stack_add_named(GTK_STACK(stack), displays_settings(),      "Displays");
		//gtk_stack_add_named(GTK_STACK(stack), sound_settings(),         "Sound");
		//gtk_stack_add_named(GTK_STACK(stack), keyboard_settings(),      "Keyboard");
		//gtk_stack_add_named(GTK_STACK(stack), battery_settings(),       "Battery");
		//gtk_stack_add_named(GTK_STACK(stack), appearance_settings(),    "Appearance");
		//gtk_stack_add_named(GTK_STACK(stack), wallpaper_settings(),     "Wallpaper");
		//gtk_stack_add_named(GTK_STACK(stack), brightness_settings(),    "Brightness");
		//gtk_stack_add_named(GTK_STACK(stack), users_settings(),         "Users & Groups");
		//gtk_stack_add_named(GTK_STACK(stack), datetime_settings(),      "Date & Time");
		//gtk_stack_add_named(GTK_STACK(stack), region_settings(),        "Region & Language");
		//gtk_stack_add_named(GTK_STACK(stack), applications_settings(),  "Applications");
		//gtk_stack_add_named(GTK_STACK(stack), about_settings(),         "About");
		//gtk_stack_add_named(GTK_STACK(stack), notifications_settings(), "Notifications");
		//gtk_stack_add_named(GTK_STACK(stack), updates_settings(),       "Software & Updates");

		for (int i = 0; lazy_pages[i].name; i++) {
			gtk_stack_add_named(GTK_STACK(stack),
					    make_placeholder_page(lazy_pages[i].name),
					    lazy_pages[i].name);
		}
		stack_global = stack;

		//#define PLACEHOLDER(name, icon) do { \
		//		GtkWidget *_b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); \
		//		gtk_widget_set_valign(_b, GTK_ALIGN_CENTER); \
		//		gtk_widget_set_halign(_b, GTK_ALIGN_CENTER); \
		//		GtkWidget *_i = gtk_image_new_from_icon_name(icon); \
		//		gtk_image_set_pixel_size(GTK_IMAGE(_i), 64); \
		//		gtk_box_append(GTK_BOX(_b), _i); \
		//		gtk_box_append(GTK_BOX(_b), gtk_label_new(name)); \
		//		gtk_stack_add_named(GTK_STACK(stack), _b, name); \
		//	} while(0)
		//
		//        PLACEHOLDER("Sharing",     "preferences-system-sharing-symbolic");
		//#undef PLACEHOLDER

		GtkListBoxRow *first=gtk_list_box_get_row_at_index(GTK_LIST_BOX(sidebar_listbox),0);
		gtk_list_box_select_row(GTK_LIST_BOX(sidebar_listbox),first);
		gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr),sidebar_listbox);
		gtk_box_append(GTK_BOX(sv),scr);
		gtk_box_append(GTK_BOX(box),sv);
		gtk_box_append(GTK_BOX(box),gtk_separator_new(GTK_ORIENTATION_VERTICAL));
		gtk_box_append(GTK_BOX(box),stack);
		gtk_window_set_child(GTK_WINDOW(window),box);

		//GtkListBoxRow *first=gtk_list_box_get_row_at_index(GTK_LIST_BOX(sidebar_listbox),0);
		//gtk_list_box_select_row(GTK_LIST_BOX(sidebar_listbox),first);
		//gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scr),sidebar_listbox);
		//gtk_box_append(GTK_BOX(sv),scr);
		//gtk_box_append(GTK_BOX(box),sv);
		//gtk_box_append(GTK_BOX(box),gtk_separator_new(GTK_ORIENTATION_VERTICAL));
		//gtk_box_append(GTK_BOX(box),stack);
		//gtk_window_set_child(GTK_WINDOW(window),box);

		const char *jump = g_object_get_data(G_OBJECT(app), "jump-to");
		if (jump) {
			int total = 0;
			while (gtk_list_box_get_row_at_index(
				GTK_LIST_BOX(sidebar_listbox), total)) total++;
			for (int i = 0; i < total; i++) {
				GtkListBoxRow *r = gtk_list_box_get_row_at_index(
					GTK_LIST_BOX(sidebar_listbox), i);
				if (!r) continue;
				const char *name = g_object_get_data(G_OBJECT(r), "page-name");
				if (name && !strcmp(name, jump)) {
					gtk_list_box_select_row(GTK_LIST_BOX(sidebar_listbox), r);
					break;
				}
			}
		}
	}

	//if (!gtk_widget_get_visible(window)) gtk_widget_show(window);
	if (!gtk_widget_get_visible(window)) gtk_widget_set_visible(window, TRUE);
	else gtk_window_destroy(GTK_WINDOW(window));
}
