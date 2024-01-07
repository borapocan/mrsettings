#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#define MAX_LINES 3
#define MAX_LENGTH 1024
#define MAX_LINE_LENGTH 9
#define MAX_VALUES 20
#define MAX_DIGITS 21
#define MAX_INTERFACES 100


static void activate(GtkApplication* app, gpointer user_data);
static void activate_cb(GtkListView *list, guint position, gpointer unused);
static void add_row_dwm(GtkGrid *table, int row, GtkSizeGroup *size_group, const char *label_text, const char *hex, const char  **options);
static void add_row_panel(GtkGrid *table, int row, GtkSizeGroup *size_group, const char *label_text, const char *hex, const char  **options);
static void bind_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);
static GListModel *create_application_list(void);
static char *current_timezone(void);
static int current_timezone_position(void);
static GtkWidget *customisations(void);
static double *get_color_double_array(char* pattern, char *path, int* size);
static char **get_colors(char* pattern, char* path);
static char *get_home_env(void);
static char **get_network_interfaces(int *count);
static char *gdk_rgba_to_hex(double red, double green, double blue, double alpha);
static GdkRGBA hex_to_gdk_rgba(const char *hex);
static double *hex_to_gdk_rgba_doubles(const char *hex);
static GtkWidget *keybindings(void);
static GtkWidget *network_settings(void);
static void rgba_notify_callback_dwm(GObject *object, GParamSpec *pspec, gpointer user_data);
static void rgba_notify_callback_panel(GObject *object, GParamSpec *pspec, gpointer user_data);
static GtkWidget *select_timezone(void);
static void setup_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);
static void timezone_save_button_clicked(GtkWidget *widget, gpointer data);
static gboolean wired_state (char *interface);





typedef struct CallbackData {
	const char *hex;
	const char *pattern;
	const char *config_path;
	int index;
} CallbackData;

/* variables */
const char *timezones[] = {NULL};
GtkStringList *timezone_string_list;
GtkWidget *timezone_dropdown_menu;
char *colors_path = NULL;
char *pattern = NULL;


int main(int argc, char **argv) {
	GtkApplication *app;
	int status;
	app = gtk_application_new("org.chaos.settings_menu", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);
	return status;
}

void activate(GtkApplication* app, gpointer user_data) {
	static GtkWidget *window = NULL;
	static GtkWidget *apps_window = NULL;
	GtkWidget *sidebar, *nn;
	GtkWidget *stack;
	GtkWidget *box;
	GtkWidget *widget;
	GtkWidget *header;
	GtkWidget *main_box;
	GtkWidget *main_label;
	GtkWidget *bindings_vbox;
	GtkWidget *bindings_table;
	GtkWidget *tint2_vbox;


	const char * pages[] = {
		"Welcome to MrRobotOS Settings",
		"Key Bindings",
		"MrDWM Configurations",
		"System Settings",
		"Network Settings",
		"Display Settings",
		"Volume & Audio Settings",
		"Time & Language Settings",
		"Applications",
		"Software Updates & Info. Security",
		NULL
	};
	const char *c = NULL;
	guint i;

	if (!window) {
		window = gtk_application_window_new (app);
		gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
		header = gtk_header_bar_new ();
		gtk_window_set_titlebar (GTK_WINDOW(window), header);
		gtk_window_set_title (GTK_WINDOW(window), "MrRobotOS Settings Menu");
		g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
		box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		sidebar = gtk_stack_sidebar_new ();
		gtk_box_append (GTK_BOX (box), sidebar);
		stack = gtk_stack_new ();
		gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN);
		gtk_stack_sidebar_set_stack (GTK_STACK_SIDEBAR (sidebar), GTK_STACK (stack));
		gtk_widget_set_hexpand (stack, TRUE);
		gtk_box_append (GTK_BOX (box), stack);
		for (i=0; (c = *(pages+i)) != NULL; i++ ) {
			if (i == 0) {
				main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 25);
				gtk_widget_set_valign (main_box, GTK_ALIGN_CENTER);
				gtk_widget_set_halign (main_box, GTK_ALIGN_CENTER);
				widget = gtk_image_new_from_file("we_are_fsociety.png");
				gtk_widget_add_css_class (widget, "icon-dropshadow");
				gtk_image_set_pixel_size (GTK_IMAGE (widget), 500);
				gtk_box_append (GTK_BOX (main_box), widget);
				main_label = gtk_label_new("mrrobotOS - Settings & Usage Guide");
				gtk_box_append (GTK_BOX (main_box), main_label);
				gtk_stack_add_named (GTK_STACK (stack), main_box, c);
				g_object_set (gtk_stack_get_page (GTK_STACK (stack),
				main_box), "title", c, NULL);
			} else if (i == 1) {
				bindings_vbox = keybindings ();
				gtk_stack_add_named (GTK_STACK(stack),
				bindings_vbox, c);
				g_object_set (gtk_stack_get_page (GTK_STACK(stack), bindings_vbox), "title", c, NULL);

			} else if (i == 2) {
				widget = customisations();
				gtk_stack_add_named(GTK_STACK(stack), widget, c);
				g_object_set(gtk_stack_get_page(GTK_STACK(stack), widget), "title", c, NULL);

			} else if (i == 4) {
				widget = network_settings();
				gtk_stack_add_named(GTK_STACK(stack), widget, c);
				g_object_set(gtk_stack_get_page(GTK_STACK(stack), widget), "title", c, NULL);


			} else if (i == 7) {
				GtkWidget *timezone_box =
					gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
				GtkWidget *timezone_box2 = select_timezone ();
				GtkWidget *save_button =
					gtk_button_new_with_label("Save");
				g_signal_connect(G_OBJECT(save_button),
		     "clicked",
		     G_CALLBACK(timezone_save_button_clicked)
				, NULL);
				g_object_set (save_button,
		  "margin-start",  100,
		  "margin-top",    600,
		  "margin-end",    100,
		  "margin-bottom", 5,
				NULL );
				gtk_widget_set_margin_bottom (save_button, 5);
				gtk_box_append(GTK_BOX(timezone_box), timezone_box2);
				gtk_box_append(GTK_BOX(timezone_box), save_button);
				gtk_stack_add_named(GTK_STACK(stack), timezone_box, c);
				g_object_set(gtk_stack_get_page(GTK_STACK(stack),
		 timezone_box), "title", c, NULL);
			} else if (i == 8) {
				GtkWidget *apps_widget;
				if (!apps_window) {
					GtkWidget *list, *sw;
					GListModel *model;
					GtkListItemFactory *factory;
					gtk_window_set_default_size(GTK_WINDOW(apps_window),640, 320);
					gtk_window_set_title(GTK_WINDOW(apps_window), "Applications");
					g_object_add_weak_pointer(G_OBJECT(apps_window),
					(gpointer *) &apps_window);
					factory = gtk_signal_list_item_factory_new ();
					g_signal_connect(factory, "setup", G_CALLBACK(setup_listitem_cb), NULL);
					g_signal_connect(factory, "bind",
					G_CALLBACK(bind_listitem_cb), NULL);
					model = create_application_list ();
					list = gtk_list_view_new(GTK_SELECTION_MODEL(gtk_single_selection_new(model)),factory);
					g_signal_connect(list, "activate",
					G_CALLBACK(activate_cb), NULL);
					sw = gtk_scrolled_window_new();
					gtk_window_set_child(GTK_WINDOW (apps_window), sw);
					gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw),
					list);
					gtk_stack_add_named(GTK_STACK(stack), sw, c);
					g_object_set (gtk_stack_get_page(GTK_STACK(stack), sw), "title", c, NULL);
				}

			} else {
				widget = gtk_label_new (c);
				gtk_stack_add_named(GTK_STACK(stack), widget, c);
				g_object_set(gtk_stack_get_page(GTK_STACK(stack),widget), "title", c, NULL);
			}
		}
		gtk_window_set_child (GTK_WINDOW (window), box);
	}
	if (!gtk_widget_get_visible (window)) {
		gtk_widget_show (window);

	} else {
		gtk_window_destroy (GTK_WINDOW (window));

	}
}

void activate_cb(GtkListView  *list, guint position, gpointer unused) {
	GAppInfo *app_info;
	GdkAppLaunchContext *context;
	GError *error = NULL;
	app_info = g_list_model_get_item(G_LIST_MODEL(gtk_list_view_get_model(list)),position);
	context = gdk_display_get_app_launch_context(gtk_widget_get_display(GTK_WIDGET (list)));
	if (!g_app_info_launch (app_info, NULL, G_APP_LAUNCH_CONTEXT (context),
	&error)) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET (list))),
				  GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
				  GTK_MESSAGE_ERROR,
				  GTK_BUTTONS_CLOSE,
				  "Could not launch %s", g_app_info_get_display_name (app_info));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		g_clear_error (&error);
		gtk_widget_show (dialog);
	}
	g_object_unref (context);
	g_object_unref (app_info);
}

void add_row_dwm(GtkGrid *table, int row, GtkSizeGroup *size_group, const char *label_text, const char *hex, const char  **options) {
	GtkWidget *label;
	label = gtk_label_new_with_mnemonic (label_text);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);
	gtk_widget_set_hexpand (label, TRUE);
	gtk_grid_attach (table, label, 0, row, 1, 1);
	struct CallbackData *callback_data = g_malloc(sizeof(struct CallbackData));
	callback_data->hex = hex;
	callback_data->pattern = pattern;
	callback_data->config_path = colors_path;
	callback_data->index = row;
	GdkRGBA initial_color = hex_to_gdk_rgba(hex);
	GtkWidget *color_button = gtk_color_button_new_with_rgba(&initial_color);
	g_signal_connect(G_OBJECT(color_button), "notify::rgba", G_CALLBACK(rgba_notify_callback_dwm), callback_data);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), color_button);
	gtk_widget_set_halign (color_button, GTK_ALIGN_END);
	gtk_widget_set_valign (color_button, GTK_ALIGN_BASELINE);
	gtk_size_group_add_widget (size_group, color_button);
	gtk_grid_attach (table, color_button, 1, row, 1, 1);
}

void add_row_panel(GtkGrid *table, int row, GtkSizeGroup *size_group, const char *label_text, const char *hex, const char  **options) {
	GtkWidget *label;
	label = gtk_label_new_with_mnemonic (label_text);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);
	gtk_widget_set_hexpand (label, TRUE);
	gtk_grid_attach (table, label, 0, row, 1, 1);
	struct CallbackData *callback_data = g_malloc(sizeof(struct CallbackData));
	callback_data->hex = hex;
	callback_data->pattern = pattern;
	callback_data->config_path = colors_path;
	callback_data->index = row;
	size_t len = strlen(hex);
	char *hexx = (char*)hex;
	hexx[len - 2] = '\0';
	GdkRGBA initial_color = hex_to_gdk_rgba(hexx);
	GtkWidget *color_button = gtk_color_button_new_with_rgba(&initial_color);
	g_signal_connect(G_OBJECT(color_button), "notify::rgba", G_CALLBACK(rgba_notify_callback_panel), callback_data);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), color_button);
	gtk_widget_set_halign (color_button, GTK_ALIGN_END);
	gtk_widget_set_valign (color_button, GTK_ALIGN_BASELINE);
	gtk_size_group_add_widget (size_group, color_button);
	gtk_grid_attach (table, color_button, 1, row, 1, 1);
}

void bind_listitem_cb (GtkListItemFactory *factory, GtkListItem  *list_item) {
	GtkWidget *image;
	GtkWidget *label;
	GAppInfo *app_info;
	image = gtk_widget_get_first_child(gtk_list_item_get_child (list_item));
	label = gtk_widget_get_next_sibling (image);
	app_info = gtk_list_item_get_item(list_item);
	gtk_image_set_from_gicon(GTK_IMAGE(image), g_app_info_get_icon(app_info));
	gtk_label_set_label(GTK_LABEL(label), g_app_info_get_display_name(app_info));
}

GListModel *create_application_list (void) {
	GListStore *store;
	GList *apps, *l;
	store = g_list_store_new (G_TYPE_APP_INFO);
	apps = g_app_info_get_all ();
	for (l = apps; l; l = l->next)
		g_list_store_append (store, l->data);
	g_list_free_full (apps, g_object_unref);
	return G_LIST_MODEL (store);
}

char *current_timezone(void) {
	char buffer[2048];
	FILE *command = popen("readlink /etc/localtime | cut -d '/' -f5-", "r");
	while (fgets (buffer, sizeof (buffer), command) != NULL) {
		char *p = strchr (buffer, '\n');
		if (p) *p = '\0';
	}
	pclose(command);
	char *current_localtime = buffer;
	return current_localtime;
}

int current_timezone_position(void) {
	int position;
	char full_command[2048];
	char *command = "find /usr/share/zoneinfo/ -type f -exec grep -IL . \"{}\" \\; | cut -d '/' -f5- | sort | grep -nx \'";
	char *current_localtime = current_timezone ();
	char *rest_of_command = "\' | cut -d ':' -f1";
	strcpy (full_command, command);
	strcat (full_command, current_localtime);
	strcat (full_command, rest_of_command);
	FILE *process = popen (full_command, "r");
	char buffer[2048];
	while (fgets (buffer, sizeof (buffer), process) != NULL) {
		char *p = strchr (buffer, '\n');
		if (p) *p = '\0';
	}
	pclose(process);
	sscanf(buffer, "%d", &position);
	return position;
}

GtkWidget *customisations(void) {
	GtkWidget *scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	char *home_env = get_home_env();
	colors_path = (char*)malloc(sizeof(char) * (MAX_LENGTH + strlen(home_env) + 40));
	sprintf(colors_path, "%s/.local/src/suckless/dwm/colors.h", home_env);
	GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	GtkWidget *label = gtk_label_new ("MRDWM Configurations");
	gtk_box_append(GTK_BOX(vbox), label);
	gtk_widget_set_margin_start (vbox, 5);
	gtk_widget_set_margin_end (vbox, 5);
	gtk_widget_set_margin_top (vbox, 5);
	gtk_widget_set_margin_bottom (vbox, 5);
	GtkSizeGroup *size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroupMode new_mode = GTK_SIZE_GROUP_HORIZONTAL;
	g_object_set_data_full (G_OBJECT (scroll), "size-group", size_group, g_object_unref);
	GtkWidget *frame = gtk_frame_new(NULL);
	gtk_box_append (GTK_BOX (vbox), frame);
	GtkWidget *table = gtk_grid_new ();
	gtk_widget_set_margin_start (table, 5);
	gtk_widget_set_margin_end (table, 5);
	gtk_widget_set_margin_top (table, 5);
	gtk_widget_set_margin_bottom (table, 5);
	gtk_grid_set_row_spacing (GTK_GRID (table), 5);
	gtk_grid_set_column_spacing (GTK_GRID (table), 10);
	gtk_frame_set_child (GTK_FRAME (frame), table);
	pattern = "sch_norm_";
	char **scheme_norm = get_colors(pattern, colors_path);

	add_row_dwm (GTK_GRID (table), 0, size_group, "_Scheme Normal Background Color:", scheme_norm[0], NULL);
	add_row_dwm (GTK_GRID (table), 1, size_group, "_Scheme Normal Foreground Color:", scheme_norm[1], NULL);
	add_row_dwm (GTK_GRID (table), 2, size_group, "_Scheme Normal Border Color:", scheme_norm[2], NULL);

	frame = gtk_frame_new (NULL);
	gtk_box_append (GTK_BOX (vbox), frame);
	table = gtk_grid_new ();
	gtk_widget_set_margin_start (table, 5);
	gtk_widget_set_margin_end (table, 5);
	gtk_widget_set_margin_top (table, 5);
	gtk_widget_set_margin_bottom (table, 5);
	gtk_grid_set_row_spacing (GTK_GRID (table), 5);
	gtk_grid_set_column_spacing (GTK_GRID (table), 10);
	gtk_frame_set_child (GTK_FRAME (frame), table);
	pattern = "sch_sel_";
	char **scheme_sel = get_colors(pattern, colors_path);

	add_row_dwm (GTK_GRID (table), 0, size_group, "_Scheme Selected Background Color:", scheme_sel[0], NULL);
	add_row_dwm (GTK_GRID (table), 1, size_group, "_Scheme Selected Foreground Color:", scheme_sel[1], NULL);
	add_row_dwm (GTK_GRID (table), 2, size_group, "_Scheme Selected Border Color:", scheme_sel[2], NULL);

	frame = gtk_frame_new (NULL);
	gtk_box_append (GTK_BOX (vbox), frame);
	table = gtk_grid_new ();
	gtk_widget_set_margin_start (table, 5);
	gtk_widget_set_margin_end (table, 5);
	gtk_widget_set_margin_top (table, 5);
	gtk_widget_set_margin_bottom (table, 5);
	gtk_grid_set_row_spacing (GTK_GRID (table), 5);
	gtk_grid_set_column_spacing (GTK_GRID (table), 10);
	gtk_frame_set_child (GTK_FRAME (frame), table);
	pattern = "sch_hov";
	char **scheme_hov = get_colors(pattern, colors_path);

	add_row_dwm (GTK_GRID (table), 0, size_group, "_Scheme Hover Background Color:", scheme_hov[0], NULL);
	add_row_dwm (GTK_GRID (table), 1, size_group, "_Scheme Hover Foreground Color:", scheme_hov[1], NULL);
	add_row_dwm (GTK_GRID (table), 2, size_group, "_Scheme Hover Border Color:", scheme_hov[2], NULL);

	frame = gtk_frame_new (NULL);
	gtk_box_append (GTK_BOX (vbox), frame);
	table = gtk_grid_new ();
	gtk_widget_set_margin_start (table, 5);
	gtk_widget_set_margin_end (table, 5);
	gtk_widget_set_margin_top (table, 5);
	gtk_widget_set_margin_bottom (table, 5);
	gtk_grid_set_row_spacing (GTK_GRID (table), 5);
	gtk_grid_set_column_spacing (GTK_GRID (table), 10);
	gtk_frame_set_child (GTK_FRAME (frame), table);
	pattern = "sch_hid";
	char **scheme_hid = get_colors(pattern, colors_path);

	add_row_dwm (GTK_GRID (table), 0, size_group, "_Scheme Hidden Background Color:", scheme_hid[0], NULL);
	add_row_dwm (GTK_GRID (table), 1, size_group, "_Scheme Hidden Foreground Color:", scheme_hid[1], NULL);
	add_row_dwm (GTK_GRID (table), 2, size_group, "_Scheme Hidden Border Color:", scheme_hid[2], NULL);

	frame = gtk_frame_new (NULL);
	gtk_box_append (GTK_BOX (vbox), frame);
	table = gtk_grid_new ();
	gtk_widget_set_margin_start (table, 5);
	gtk_widget_set_margin_end (table, 5);
	gtk_widget_set_margin_top (table, 5);
	gtk_widget_set_margin_bottom (table, 5);
	gtk_grid_set_row_spacing (GTK_GRID (table), 5);
	gtk_grid_set_column_spacing (GTK_GRID (table), 10);
	gtk_frame_set_child (GTK_FRAME (frame), table);
	pattern = "sch_status";
	char **scheme_status = get_colors(pattern, colors_path);

	add_row_dwm (GTK_GRID (table), 0, size_group, "_Scheme Status Background Color:", scheme_status[0], NULL);
	add_row_dwm (GTK_GRID (table), 1, size_group, "_Scheme Status Foreground Color:", scheme_status[1], NULL);
	add_row_dwm (GTK_GRID (table), 2, size_group, "_Scheme Status Border Color:", scheme_status[2], NULL);

	frame = gtk_frame_new (NULL);
	gtk_box_append (GTK_BOX (vbox), frame);
	table = gtk_grid_new ();
	gtk_widget_set_margin_start (table, 5);
	gtk_widget_set_margin_end (table, 5);
	gtk_widget_set_margin_top (table, 5);
	gtk_widget_set_margin_bottom (table, 5);
	gtk_grid_set_row_spacing (GTK_GRID (table), 5);
	gtk_grid_set_column_spacing (GTK_GRID (table), 10);
	gtk_frame_set_child (GTK_FRAME (frame), table);
	pattern = "sch_tags_sel";
	char **scheme_tags_sel = get_colors(pattern, colors_path);

	add_row_dwm (GTK_GRID (table), 0, size_group, "_Scheme Tags Selected Background Color:", scheme_tags_sel[0], NULL);
	add_row_dwm (GTK_GRID (table), 1, size_group, "_Scheme Tags Selected Foreground Color:", scheme_tags_sel[1], NULL);
	add_row_dwm (GTK_GRID (table), 2, size_group, "_Scheme Tags Selected Border Color:", scheme_tags_sel[2], NULL);

	frame = gtk_frame_new (NULL);
	gtk_box_append (GTK_BOX (vbox), frame);
	table = gtk_grid_new ();
	gtk_widget_set_margin_start (table, 5);
	gtk_widget_set_margin_end (table, 5);
	gtk_widget_set_margin_top (table, 5);
	gtk_widget_set_margin_bottom (table, 5);
	gtk_grid_set_row_spacing (GTK_GRID (table), 5);
	gtk_grid_set_column_spacing (GTK_GRID (table), 10);
	gtk_frame_set_child (GTK_FRAME (frame), table);
	pattern = "sch_tags_norm";
	char **scheme_tags_norm = get_colors(pattern, colors_path);

	add_row_dwm (GTK_GRID (table), 0, size_group, "_Scheme Tags Normal Background Color:", scheme_tags_norm[0], NULL);
	add_row_dwm (GTK_GRID (table), 1, size_group, "_Scheme Tags Normal Foreground Color:", scheme_tags_norm[1], NULL);
	add_row_dwm (GTK_GRID (table), 2, size_group, "_Scheme Tags Normal Border Color:", scheme_tags_norm[2], NULL);

	frame = gtk_frame_new (NULL);
	gtk_box_append (GTK_BOX (vbox), frame);
	table = gtk_grid_new ();
	gtk_widget_set_margin_start (table, 5);
	gtk_widget_set_margin_end (table, 5);
	gtk_widget_set_margin_top (table, 5);
	gtk_widget_set_margin_bottom (table, 5);
	gtk_grid_set_row_spacing (GTK_GRID (table), 5);
	gtk_grid_set_column_spacing (GTK_GRID (table), 10);
	gtk_frame_set_child (GTK_FRAME (frame), table);
	pattern = "sch_info_sel";
	char **scheme_info_sel = get_colors(pattern, colors_path);

	add_row_dwm (GTK_GRID (table), 0, size_group, "_Scheme Info Selected Background Color:", scheme_info_sel[0], NULL);
	add_row_dwm (GTK_GRID (table), 1, size_group, "_Scheme Info Selected Foreground Color:", scheme_info_sel[1], NULL);
	add_row_dwm (GTK_GRID (table), 2, size_group, "_Scheme Info Selected Border Color:", scheme_info_sel[2], NULL);

	frame = gtk_frame_new (NULL);
	gtk_box_append (GTK_BOX (vbox), frame);
	table = gtk_grid_new ();
	gtk_widget_set_margin_start (table, 5);
	gtk_widget_set_margin_end (table, 5);
	gtk_widget_set_margin_top (table, 5);
	gtk_widget_set_margin_bottom (table, 5);
	gtk_grid_set_row_spacing (GTK_GRID (table), 5);
	gtk_grid_set_column_spacing (GTK_GRID (table), 10);
	gtk_frame_set_child (GTK_FRAME (frame), table);
	pattern = "sch_info_norm";
	char **scheme_info_norm = get_colors(pattern, colors_path);

	add_row_dwm (GTK_GRID (table), 0, size_group, "_Scheme Info Normal Background Color:", scheme_info_norm[0], NULL);
	add_row_dwm (GTK_GRID (table), 1, size_group, "_Scheme Info Normal Foreground Color:", scheme_info_norm[1], NULL);
	add_row_dwm (GTK_GRID (table), 2, size_group, "_Scheme Info Normal Border Color:", scheme_info_norm[2], NULL);

	label = gtk_label_new ("Panel Configurations");
	gtk_box_append(GTK_BOX(vbox), label);
	gtk_widget_set_margin_start (vbox, 5);
	gtk_widget_set_margin_end (vbox, 5);
	gtk_widget_set_margin_top (vbox, 5);
	gtk_widget_set_margin_bottom (vbox, 5);

	int size;
	pattern = "background-rgba";
	sprintf(colors_path, "%s/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml", home_env);
	double *arr = get_color_double_array(pattern, colors_path, &size);
	char *hexString = gdk_rgba_to_hex(arr[0], arr[1], arr[2], arr[3]);
	if (hexString == NULL) {
		printf("Error getting hex string.\n");
	}

	frame = gtk_frame_new (NULL);
	gtk_box_append (GTK_BOX (vbox), frame);
	table = gtk_grid_new ();
	gtk_widget_set_margin_start (table, 5);
	gtk_widget_set_margin_end (table, 5);
	gtk_widget_set_margin_top (table, 5);
	gtk_widget_set_margin_bottom (table, 5);
	gtk_grid_set_row_spacing (GTK_GRID (table), 5);
	gtk_grid_set_column_spacing (GTK_GRID (table), 10);
	gtk_frame_set_child (GTK_FRAME (frame), table);
	add_row_panel(GTK_GRID (table), 0, size_group, "_Panel Background Color:", hexString, NULL);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), vbox);
	return scroll;
}

double *get_color_double_array(char* pattern, char *path, int* size) {
	char command[256];
	sprintf(command, "grep -A4 '%s' %s | awk -F'\"' '/value=/ {print $4}'", pattern, path);

	FILE *fp = popen(command, "r");
	if (fp == NULL) {
		perror("Error opening process");
		return NULL;
	}

	double* values = malloc(MAX_VALUES * sizeof(double));
	if (values == NULL) {
		perror("Error allocating memory");
		return NULL;
	}

	*size = 0;
	char buffer[MAX_DIGITS];
	while (fgets(buffer, sizeof(buffer), fp) != NULL && *size < MAX_VALUES) {
		size_t len = strlen(buffer);

		if (len > 0 && buffer[len - 1] == '\n') {
			buffer[len - 1] = '\0';
			len--;
		}

		if (sscanf(buffer, "%lf", &values[*size]) == 1) {
			(*size)++;
		} else {
			printf("Error parsing double value.\n");
		}
	}
	pclose(fp);
	return values;
}

char **get_colors(char* pattern, char* path) {
	char command[256];
	sprintf(command, "grep '%s' %s | cut -d '\"' -f2", pattern, path);

	FILE* fp = popen(command, "r");
	if (fp == NULL) {
		perror("Error opening process");
		exit(EXIT_FAILURE);
	}

	int line_count = 0;
	char line[MAX_LINE_LENGTH];

	char** lines = (char**)malloc(MAX_LINES * sizeof(char*));
	for (int i = 0; i < MAX_LINES; ++i) {
		lines[i] = (char*)malloc(MAX_LINE_LENGTH);
	}

	while (fgets(line, sizeof(line), fp) != NULL && line_count < MAX_LINES) {
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') {
			line[len - 1] = '\0';
		}
		strncpy(lines[line_count], line, MAX_LINE_LENGTH - 1);
		lines[line_count][MAX_LINE_LENGTH - 1] = '\0';
		++line_count;
	}

	pclose(fp);

	return lines;
}

char *get_home_env(void) {
	char *home_tmp, *home_str = NULL;
	if ((home_tmp = getenv("HOME")) != NULL) {
		home_str = strdup(home_tmp);
	}
	return home_str;
}

char **get_network_interfaces(int *count) {
	DIR *dir;
	struct dirent *entry;
	char **interfaces = (char**)malloc(MAX_INTERFACES * sizeof(char*));
	if (interfaces == NULL) {
		perror("Memory allocation error");
		exit(EXIT_FAILURE);
	}
	dir = opendir("/sys/class/net");
	if (dir == NULL) {
		perror("Error opening /sys/class/net");
		exit(EXIT_FAILURE);
	}
	*count = 0;
	while ((entry = readdir(dir)) != NULL && *count < MAX_INTERFACES) {
		if (entry->d_type == DT_LNK && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
			interfaces[*count] = strdup(entry->d_name);
			(*count)++;
		}
	}
	closedir(dir);
	return interfaces;
}

char *gdk_rgba_to_hex(double red, double green, double blue, double alpha) {
	int intRed = (int)(red * 255);
	int intGreen = (int)(green * 255);
	int intBlue = (int)(blue * 255);
	int intAlpha = (int)(alpha * 255);

	char* hexString = malloc(18);
	if (hexString == NULL) {
		perror("Error allocating memory");
		return NULL;
	}

	snprintf(hexString, 18, "#%02X%02X%02X%02X", intRed, intGreen, intBlue, intAlpha);
	return hexString;
}

GdkRGBA hex_to_gdk_rgba(const char *hex) {
	GdkRGBA rgba = {0.0, 0.0, 0.0, 1.0};

	if (hex[0] == '#' && (strlen(hex) == 7 || strlen(hex) == 9)) {
		unsigned int hexValue = strtoul(hex + 1, NULL, 16);
		rgba.red = ((hexValue >> 16) & 0xFF) / 255.0;
		rgba.green = ((hexValue >> 8) & 0xFF) / 255.0;
		rgba.blue = (hexValue & 0xFF) / 255.0;

		if (strlen(hex) == 9) {
			rgba.alpha = ((hexValue >> 24) & 0xFF) / 255.0;
		}
	}

	return rgba;
}

double *hex_to_gdk_rgba_doubles(const char *hex) {
	double* result = (double*)malloc(4 * sizeof(double));
	unsigned int hexValue;
	if (result == NULL) {
		perror("Error allocating memory");
		return NULL;
	}
	if (hex[0] == '#') {
		hex++;
	}
	if (sscanf(hex, "%x", &hexValue) == 1) {
		result[0] = ((hexValue >> 16) & 0xFF) / 255.0;
		result[1] = ((hexValue >> 8) & 0xFF) / 255.0;
		result[2] = (hexValue & 0xFF) / 255.0;
		result[3] = 1.0;
	} else {
		fprintf(stderr, "Invalid hex color code: %s\n", hex);
		free(result);
		return NULL;
	}
	return result;
}

GtkWidget *keybindings(void) {
	GtkWidget *scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
				GTK_POLICY_AUTOMATIC,
	GTK_POLICY_AUTOMATIC);
	GtkWidget* grid = gtk_grid_new();
	gtk_grid_set_column_homogeneous(GTK_GRID(grid), FALSE);

	GtkWidget *label_str1 = gtk_label_new("ACTIONS");
	gtk_widget_set_halign(label_str1, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(label_str1, GTK_ALIGN_CENTER);
	gtk_grid_attach(GTK_GRID(grid), label_str1, 0, 0, 1, 1);

	GtkWidget *label_str2 = gtk_label_new("KEYBINDINGS");
	gtk_widget_set_halign(label_str2, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(label_str2, GTK_ALIGN_CENTER);
	gtk_grid_attach(GTK_GRID(grid), label_str2, 2, 0, 1, 1);

	GtkWidget *separator_h_header = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_hexpand(separator_h_header, TRUE);
	gtk_widget_set_name(separator_h_header, "separator");
	gtk_grid_attach(GTK_GRID(grid), separator_h_header, 0, 1, 3, 1);

	const char *keys[] = {
		"Launch a Terminal",
		"Close Window",
		"Launch Dmenu",
		"Toggle Bar",
		"Focus Stack Visible +1",
		"Focus Stack Visible -1",
		"Focus Stack Hidden +1",
		"Focus Stack Hidden -1",
		"Increase Master Size +1",
		"Decrease Master Size -1",
		"Set Master Stack -1",
		"Set Master Stack +1",
		"Zoom",
		"View",
		"Set Tiled Layout",
		"Set Floating Layout",
		"Set Monocle Layout",
		"Set Layout",
		"Toggle Floating",
		"View All",
		"Tag All",
		"Focus Monitor -1",
		"Focus Monitor +1",
		"Tag Monitor -1",
		"Tag Monitor +1",
		"Set Gaps -1",
		"Set Gaps +1",
		"Set Gaps As Default",
		"Show Window",
		"Show All Windows",
		"Hide Window",
		"Workspace 1",
		"Workspace 2",
		"Workspace 3",
		"Workspace 4",
		"Workspace 5",
		"Workspace 6",
		"Workspace 7",
		"Workspace 8",
		"Workspace 9",
		"Quit DWM",
		"Restart DWM",
	};

	const char *actions[] = {
		"MODKEY + Shift + Enter",
		"MODKEY + Shift + c",
		"MODKEY + p",
		"MODKEY + b",
		"MODKEY + j",
		"MODKEY + k",
		"MODKEY + Shift + j",
		"MODKEY + Shift + k",
		"MODKEY + i",
		"MODKEY + d",
		"MODKEY + h",
		"MODKEY + l",
		"MODKEY + Return",
		"MODKEY + Tab",
		"MODKEY + t",
		"MODKEY + f",
		"MODKEY + m",
		"MODKEY + Space",
		"MODKEY + Shift + Space",
		"MODKEY + 0",
		"MODKEY + Shift + 0",
		"MODKEY + Comma",
		"MODKEY + Period",
		"MODKEY + Shift + Comma",
		"MODKEY + Shift + Period",
		"MODKEY + Minus",
		"MODKEY + Equal",
		"MODKEY + Shift + Equal",
		"MODKEY + s",
		"MODKEY + Shift + s",
		"MODKEY + Shift + h",
		"MODKEY + 1",
		"MODKEY + 2",
		"MODKEY + 3",
		"MODKEY + 4",
		"MODKEY + 5",
		"MODKEY + 6",
		"MODKEY + 7",
		"MODKEY + 8",
		"MODKEY + 9",
		"MODKEY + Shift + q",
		"MODKEY + Control + Shift + q",
	};

	for (int i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
		GtkWidget *label_key = gtk_label_new(keys[i]);
		gtk_widget_set_halign(label_key, GTK_ALIGN_CENTER);
		gtk_widget_set_valign(label_key, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), label_key, 0, i * 2 + 2, 1, 1);

		GtkWidget *label_action = gtk_label_new(actions[i]);
		gtk_widget_set_halign(label_action, GTK_ALIGN_CENTER);
		gtk_widget_set_valign(label_action, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(grid), label_action, 2, i * 2 + 2, 1, 1);

		GtkWidget *separator_h = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
		gtk_widget_set_hexpand(separator_h, TRUE);
		gtk_widget_set_name(separator_h, "separator");
		gtk_widget_set_size_request(separator_h, -1, 2);
		gtk_grid_attach(GTK_GRID(grid), separator_h, 0, i * 2 + 3, 3, 1);
	}

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), grid);
	GtkCssProvider *cssProvider = gtk_css_provider_new();
	gtk_css_provider_load_from_data(cssProvider, "separator { margin: 5px; }", -1);
	gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	return scroll;
}

static GtkWidget *network_settings(void) {
	GtkWidget *sw, *vbox;
	sw = gtk_scrolled_window_new();

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_box_set_homogeneous (GTK_BOX (vbox), TRUE);
	int count;
	char **network_interfaces = get_network_interfaces(&count);
	char* wireless_interfaces[MAX_INTERFACES];
	char* wireless_adapter_interfaces[MAX_INTERFACES];
	char* wired_interfaces[MAX_INTERFACES];
	char* bridge_interfaces[MAX_INTERFACES];
	char* tunnel_interfaces[MAX_INTERFACES];

	int wired_count = 0, wireless_count = 0, wireless_adapter_count = 0, bridge_count = 0, tunnel_count = 0;

	char wireless_interface[256];
	FILE *wirelessFile = fopen("/proc/net/wireless", "r");
	if (wirelessFile == NULL) {
		perror("Error opening /proc/net/wireless");
		exit(EXIT_FAILURE);
	}

	char line[256];
	while (fgets(line, sizeof(line), wirelessFile) != NULL) {
		sscanf(line, "%s", wireless_interface);
	}

	fclose(wirelessFile);

	for (int i = 0; i < count; ++i) {
		char *interface = network_interfaces[i];

		if (strstr(interface, "en") || strstr(interface, "et")) {
			wired_interfaces[wired_count++] = strdup(interface);
		} else if (strstr(interface, "wl")) {
			char ueventPath[256];
			sprintf(ueventPath, "/sys/class/net/%s/device/uevent", interface);

			FILE *ueventFile = fopen(ueventPath, "r");


			char ueventLine[256];
			fgets(ueventLine, sizeof(ueventLine), ueventFile);
			fclose(ueventFile);

			char *devType = strstr(ueventLine, "DEVTYPE");
			if (devType != NULL) {
				char value[256];
				sscanf(devType, "DEVTYPE=%s", value);

				if (strcmp(value, "usb_interface") == 0) {
					wireless_adapter_interfaces[wireless_adapter_count++] = strdup(interface);
				}
			} else {
				wireless_interfaces[wireless_count++] = strdup(interface);
			}
		} else if (strstr(interface, "br")) {
			bridge_interfaces[bridge_count++] = strdup(interface);
		} else if (strstr(interface, "tun") || strstr(interface, "vpn")) {
			tunnel_interfaces[tunnel_count++] = strdup(interface);
		}
	}

	for (int i = 0; i < wired_count; ++i) {
		GtkWidget *frame = gtk_frame_new(NULL);
		GtkWidget *label = gtk_label_new(wired_interfaces[i]);
		GtkWidget *wired_switch = gtk_switch_new();
		gtk_frame_set_child(GTK_FRAME(frame), wired_switch);
		gtk_frame_set_child(GTK_FRAME(frame), label);
		//gtk_widget_set_valign(label, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(vbox), frame);


	}

	for (int i = 0; i < wireless_count; ++i) {
		GtkWidget *frame = gtk_frame_new(NULL);
		GtkWidget *label = gtk_label_new(wireless_interfaces[i]);
		GtkWidget *wireless_switch = gtk_switch_new();
		gtk_frame_set_child(GTK_FRAME(frame), label);
		gtk_frame_set_child(GTK_FRAME(frame), wireless_switch);
		gtk_widget_set_valign(label, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(vbox), frame);
	}

	for (int i = 0; i < wireless_adapter_count; ++i) {
		GtkWidget *frame = gtk_frame_new(NULL);
		GtkWidget *label = gtk_label_new(wireless_adapter_interfaces[i]);
		GtkWidget *wireless_adapter_switch = gtk_switch_new();
		gtk_frame_set_child(GTK_FRAME(frame), label);
		gtk_frame_set_child(GTK_FRAME(frame), wireless_adapter_switch);
		gtk_widget_set_valign(label, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(vbox), frame);
	}

	for (int i = 0; i < bridge_count; ++i) {
		GtkWidget *frame = gtk_frame_new(NULL);
		GtkWidget *label = gtk_label_new(bridge_interfaces[i]);
		GtkWidget *bridge_switch = gtk_switch_new();
		gtk_frame_set_child(GTK_FRAME(frame), label);
		gtk_frame_set_child(GTK_FRAME(frame), bridge_switch);
		gtk_widget_set_valign(label, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(vbox), frame);
	}

	for (int i = 0; i < tunnel_count; ++i) {
		GtkWidget *frame = gtk_frame_new(NULL);
		GtkWidget *label = gtk_label_new(tunnel_interfaces[i]);
		GtkWidget *tunnel_switch = gtk_switch_new();
		gtk_frame_set_child(GTK_FRAME(frame), label);
		gtk_frame_set_child(GTK_FRAME(frame), tunnel_switch);
		gtk_widget_set_valign(label, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(vbox), frame);
	}


	for (int i = 0; i < count; ++i) {
		free(network_interfaces[i]);
	}

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), vbox);
	return sw;
}

void rgba_notify_callback_dwm(GObject *object, GParamSpec *pspec, gpointer user_data) {
	GdkRGBA color;
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(object), &color);

	guint red = color.red * 255;
	guint green = color.green * 255;
	guint blue = color.blue * 255;

	char str[8], command[256];
	snprintf(str, sizeof(str), "#%02X%02X%02X", red, green, blue);
	struct CallbackData *data = (struct CallbackData *)user_data;

	snprintf(command, sizeof(command), "sed -i '/%s/s/'\"$(grep '%s' %s | cut -d '\"' -f2 | sed -n '%dp')\"'/""%s/g' %s", data->pattern, data->pattern, colors_path, data->index + 1, str, colors_path);
	int result = system(command);

	if (result == 0) {
		printf("Command executed successfully.\n");
	} else {
		printf("Command execution failed.\n");
	}
}

void rgba_notify_callback_panel(GObject *object, GParamSpec *pspec, gpointer user_data) {
	GdkRGBA color;
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(object), &color);

	guint red = color.red * 255;
	guint green = color.green * 255;
	guint blue = color.blue * 255;

	char str[8], command[256];
	snprintf(str, sizeof(str), "#%02X%02X%02X", red, green, blue);
	struct CallbackData *data = (struct CallbackData *)user_data;
	double *result = hex_to_gdk_rgba_doubles(str);

	if (result != NULL) {
		snprintf(command, sizeof(command), "sed -i \"s/$(grep -A3 '%s' %s | awk -F '\"' '/value=/ {print $4}' | sed -n %dp)/%.17lf/g\" %s", data->pattern, data->config_path, data->index + 1, result[0], data->config_path);

		int res = system(command);

		snprintf(command, sizeof(command), "sed -i \"s/$(grep -A3 '%s' %s | awk -F '\"' '/value=/ {print $4}' | sed -n %dp)/%.17lf/g\" %s", data->pattern, data->config_path, data->index + 2, result[1], data->config_path);

		res = system(command);
		snprintf(command, sizeof(command), "sed -i \"s/$(grep -A3 '%s' %s | awk -F '\"' '/value=/ {print $4}' | sed -n %dp)/%.17lf/g\" %s", data->pattern, data->config_path, data->index + 3, result[2], data->config_path);

		res = system(command);
		free(result);
	}
}

GtkWidget *select_timezone(void) {
	GtkWidget *timezone_box, *label;
	timezone_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 15);
	gtk_box_set_homogeneous (GTK_BOX (timezone_box), TRUE);
	label = gtk_label_new ("Select a timezone:");
	gtk_box_append (GTK_BOX (timezone_box), label);
	timezone_string_list = gtk_string_list_new((const char* const*)timezones);
	FILE *command = popen ("find /usr/share/zoneinfo/ -type f -exec grep -IL . \"{}\" \\; | cut -d '/' -f5- | sort | nl", "r");
	char buffer[2048];
	int position = current_timezone_position ();
	while (fgets (buffer, sizeof (buffer), command) != NULL) {
		char *p = strchr (buffer, '\n');
		if (p) *p = '\0';
		gtk_string_list_append(timezone_string_list, buffer);
	}
	pclose(command);
	timezone_dropdown_menu = gtk_drop_down_new(timezone_string_list, NULL);
	gtk_drop_down_set_enable_search (timezone_dropdown_menu, TRUE);
	g_object_set (timezone_dropdown_menu,
	       "margin-start",  100,
	       "margin-end",    100,
	NULL );
	gtk_drop_down_set_selected (timezone_dropdown_menu, position - 1);
	gtk_box_append (GTK_BOX (timezone_box), timezone_dropdown_menu);
	return timezone_box;
}

void setup_listitem_cb (GtkListItemFactory *factory, GtkListItem *list_item) {
	GtkWidget *apps_box;
	GtkWidget *image;
	GtkWidget *label;
	apps_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	image = gtk_image_new ();
	gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
	gtk_box_append (GTK_BOX (apps_box), image);
	label = gtk_label_new ("");
	gtk_box_append (GTK_BOX (apps_box), label);
	gtk_list_item_set_child (list_item, apps_box);
}

void timezone_save_button_clicked(GtkWidget *widget, gpointer data) {
	guint selected_timezone_position = gtk_drop_down_get_selected(GTK_DROP_DOWN(timezone_dropdown_menu));
	int position = selected_timezone_position, idx = 0;
	char *timezone, *selected_timezone = gtk_string_list_get_string(timezone_string_list, position);
	while (selected_timezone[idx] != '\0') {
		if (selected_timezone[idx] <= 32 &&
			selected_timezone[idx + 1] > 32)
			timezone = &selected_timezone[idx + 1];
		idx++;
	}
	char command[2048];
	sprintf(command, "ln -sf /usr/share/zoneinfo/%s /tmp/localtime", timezone);
	system(command);
}

static gboolean wired_state (char *interface){
	char command[] = "cat /sys/class/net/";
	strcat(command, interface);
	char carrier[] = "/carrier";
	strcat(command, carrier);
	FILE *file = popen(command, "r");
	char state[256];
	char *key = "1";
	while (fgets(state, sizeof(state), file));
	pclose(file);
	char *p = strchr(state, '\n'); *p = '\0';
	if (strcmp(key, state) == 0)
		return TRUE;
	else
		return FALSE;
}
