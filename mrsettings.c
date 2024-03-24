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
#define MAX_PORTS 100
#define MAX_AP_LINE_LENGTH 1024
#define MAX_ACCESS_POINTS 100

typedef enum {
	INPUT_DEVICES = 0,
	OUTPUT_DEVICES
} pa_autoload_type;

static const char *pa_options[] = {
	[INPUT_DEVICES] = "sources",
	[OUTPUT_DEVICES] = "sinks"
};

typedef struct {
	uint32_t value;
	int value_percentage;
	float db;
} Volume;

typedef struct {
	char *channel;
	Volume *volume;
} Channel;

typedef struct {
	float balance;
	Volume *base_volume;
} Balance;

typedef struct {
	char *name;
	char *description;
	char *type;
	uint32_t priority;
	char *availability_group;
	char *availability;
} Port;

typedef struct {
	Port *ports;
	char *active_port;
} Ports;

typedef struct {
	uint32_t index;
	char *state;
	char *name;
	char *description;
	Channel *channel_map;
	int mute;
	Balance *balance_field;
	Ports *ports;
} Device;

typedef struct VolumeCallbackData {
	char *device_name;
	char *channel_name;
	int value_percentage;
	char value_percentage_str[32];
	float db;
} VolumeCallbackData;

typedef struct CallbackData {
	const char *hex;
	const char *pattern;
	const char *config_path;
	int index;
} CallbackData;

typedef struct NetworkDevice {
	char *interface;
	char *type;
	char *state;
	char *connection;
	int ifindex;
} NetworkDevice;

typedef struct {
	int active;
	char bssid[20];
	char ssid[100];
	char mode[20];
	int chan;
	int freq;
	int rate;
	int bandwidth;
	int signal;
	char bars[20];
	char security[100];
} WifiAccessPoint;

static void activate(GtkApplication* app, gpointer user_data);
static void activate_cb(GtkListView *list, guint position, gpointer unused);
static void add_row_dwm(GtkGrid *table, int row, GtkSizeGroup *size_group, const char *label_text, const char *hex, const char  **options);
static void add_row_panel(GtkGrid *table, int row, GtkSizeGroup *size_group, const char *label_text, const char *hex, const char  **options);
static void bind_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);
static GListModel *create_application_list(void);
static char *current_timezone(void);
static int current_timezone_position(void);
static GtkWidget *customisations(void);
static void default_button_toggled(GtkWidget *button, gpointer user_data);
static void free_device_ports(char **ports);
static WifiAccessPoint *get_access_points(int *count);
static double *get_color_double_array(char* pattern, char *path, int* size);
static char **get_colors(char* pattern, char* path);
GtkWidget *get_device_channel_box(char *device_name, Channel *channel_map, int size, int is_active);
uint32_t get_device_index(pa_autoload_type type, int index);
char *get_device_state(pa_autoload_type type, int index);
char *get_device_name(pa_autoload_type type, int index);
char *get_device_description(pa_autoload_type type, int index);
int get_device_channel_size(pa_autoload_type type, int index);
char **get_device_channel_names(pa_autoload_type type, int index, int size);
int get_device_mute(pa_autoload_type type, int index);
void get_device_channel_properties(pa_autoload_type type, int index, Channel *channel);
float get_device_balance(pa_autoload_type type, int index);
void get_device_base_volume_properties(pa_autoload_type type, int index, Balance *balance_field);
int get_device_port_size(pa_autoload_type type, int index);
char **get_device_ports(pa_autoload_type type, int device_index, int *count);
Ports *get_device_port_properties(pa_autoload_type type, int index);
char *get_device_active_port(pa_autoload_type type, int index);
int get_devices_size(pa_autoload_type type);
Device **get_devices(pa_autoload_type type);
void free_devices(Device **devices, int count);
char *get_device_default(pa_autoload_type option);
int compare_ports(const void *a, const void *b);
bool check_port_availability(const char *port_name);
void default_button_toggled(GtkWidget *button, gpointer user_data);
char *get_device_port_availability(pa_autoload_type type, int device_index, int port_index);
static char *get_home_env(void);
static char **get_network_interfaces(int *count);
static char *gdk_rgba_to_hex(double red, double green, double blue, double alpha);
static GdkRGBA hex_to_gdk_rgba(const char *hex);
static double *hex_to_gdk_rgba_doubles(const char *hex);
static GtkWidget *keybindings(void);
static void rgba_notify_callback_dwm(GObject *object, GParamSpec *pspec, gpointer user_data);
static void rgba_notify_callback_panel(GObject *object, GParamSpec *pspec, gpointer user_data);
void set_default_button_state(GtkDropDown *dropdown, gpointer user_data);
static void setup_listitem_cb(GtkListItemFactory *factory, GtkListItem *list_item);
static void timezone_save_button_clicked(GtkWidget *widget, gpointer data);
static GtkWidget *timezone_settings(void);
void toggle_channels_visibility(GtkWidget *button, gpointer user_data);
static GtkWidget *volume_audio_settings(void);
static int get_network_interfaces_size(void);
static NetworkDevice *get_network_devices(void);
static GtkWidget *network_settings(void);

int compare_devices(const void *a, const void *b) {
	const NetworkDevice *device_a = (const NetworkDevice *)a;
	const NetworkDevice *device_b = (const NetworkDevice *)b;
	return device_a->ifindex - device_b->ifindex;
}

char *aps[] = {NULL};
GtkStringList *aps_string_list;
GtkWidget *aps_dropdown_menu;

GtkWidget *network_settings(void) {
	GtkWidget *sw, *vbox;
	sw = gtk_scrolled_window_new();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_box_set_homogeneous (GTK_BOX (vbox), TRUE);
	int count = get_network_interfaces_size();
	NetworkDevice *devices = get_network_devices();
	NetworkDevice *current_device = devices;
	NetworkDevice *end = devices + count;

	while (current_device < end) {
		if (strcmp(current_device->state, "disconnected") == 0 || strcmp(current_device->interface, "lo") == 0) {
			current_device++;
		} else {
			GtkWidget *frame = gtk_frame_new(NULL);

			// Create labels for each member
			GtkWidget *interface_label = gtk_label_new(current_device->interface);
			GtkWidget *type_label = gtk_label_new(current_device->type);
			GtkWidget *state_label = gtk_label_new(current_device->state);
			GtkWidget *connection_label = gtk_label_new(current_device->connection ? current_device->connection : "NULL");
			GtkWidget *ifindex_label = gtk_label_new(NULL);

			char ifindex_text[50];
			snprintf(ifindex_text, sizeof(ifindex_text), "%d", current_device->ifindex);
			gtk_label_set_text(GTK_LABEL(ifindex_label), ifindex_text);

			GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

			gtk_box_append(GTK_BOX(box), interface_label);
			gtk_box_append(GTK_BOX(box), type_label);
			gtk_box_append(GTK_BOX(box), state_label);
			gtk_box_append(GTK_BOX(box), connection_label);
			gtk_box_append(GTK_BOX(box), ifindex_label);
			if (strcmp(current_device->interface, "wlo1") == 0) {
				int ap_count;
				WifiAccessPoint *access_points = get_access_points(&ap_count)
;
				aps_string_list = gtk_string_list_new((const char* const*)aps);

				for (int i = 0; i < ap_count; i++) {
					char *p = strchr(access_points[i].ssid, '\n');
					if (p) *p = '\0';
					gtk_string_list_append(aps_string_list, access_points[i].ssid);
				}
				aps_dropdown_menu = gtk_drop_down_new((void*)aps_string_list, NULL);
				gtk_drop_down_set_enable_search((void*)aps_dropdown_menu, TRUE);
				gtk_box_append(GTK_BOX(box), aps_dropdown_menu);
			}

			gtk_frame_set_child(GTK_FRAME(frame), box);

			gtk_box_append(GTK_BOX(vbox), frame);
		}

		current_device++;


	}
	for (int i = 0; i < count; ++i) {
		g_free(devices[i].interface);
		g_free(devices[i].type);
		g_free(devices[i].state);
		g_free(devices[i].connection);
	}
	g_free(devices);



	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), vbox);
	return sw;
}

static GtkWidget *current_default_button = NULL;
static gboolean programmatic_toggling = FALSE;

/* variables */
static GtkWidget *window = NULL;
char *timezones[] = {NULL};
GtkStringList *timezone_string_list;
GtkWidget *timezone_dropdown_menu;

char *colors_path = NULL;
char *panel_path = NULL;
char *pattern = NULL;

int main(int argc, char **argv) {
	GtkApplication *app;
	int status;
	app = gtk_application_new("org.mrrobotos.settings_menu", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	status = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);
	return status;
}

void activate(GtkApplication* app, gpointer user_data) {
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
		window = gtk_application_window_new(app);
		gtk_window_set_resizable(GTK_WINDOW (window), TRUE);
		header = gtk_header_bar_new();
		gtk_window_set_titlebar(GTK_WINDOW(window), header);
		gtk_window_set_title(GTK_WINDOW(window), "MrRobotOS Settings Menu");
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
				g_object_set (gtk_stack_get_page (GTK_STACK (stack), main_box), "title", c, NULL);
			} else if (i == 1) {
				bindings_vbox = keybindings ();
				gtk_stack_add_named (GTK_STACK(stack),
				bindings_vbox, c);
				g_object_set (gtk_stack_get_page (GTK_STACK(stack), bindings_vbox), "title", c, NULL);
			} else if (i == 2) {
				widget = customisations();
				gtk_stack_add_named(GTK_STACK(stack), widget, c);
				g_object_set(gtk_stack_get_page(GTK_STACK(stack), widget), "title", c, NULL);
			}  else if (i == 4) {
				widget = network_settings();
				gtk_stack_add_named(GTK_STACK(stack), widget, c);
				g_object_set(gtk_stack_get_page(GTK_STACK(stack), widget), "title", c, NULL);
			}  else if (i == 6) {
				widget = volume_audio_settings();
				gtk_stack_add_named(GTK_STACK(stack), widget, c);
				g_object_set(gtk_stack_get_page(GTK_STACK(stack), widget), "title", c, NULL);
			}  else if (i == 7) {
				GtkWidget *timezone_box = timezone_settings();
				gtk_stack_add_named(GTK_STACK(stack), timezone_box, c);
				g_object_set(gtk_stack_get_page(GTK_STACK(stack), timezone_box), "title", c, NULL);
			} else if (i == 8) {
				GtkWidget *apps_widget;
				if (!apps_window) {
					GtkWidget *list, *sw;
					GListModel *model;
					GtkListItemFactory *factory;
					gtk_window_set_default_size(GTK_WINDOW(apps_window), 640, 320);
					gtk_window_set_title(GTK_WINDOW(apps_window), "Applications");
					g_object_add_weak_pointer(G_OBJECT(apps_window), (gpointer *) &apps_window);
					factory = gtk_signal_list_item_factory_new ();
					g_signal_connect(factory, "setup", G_CALLBACK(setup_listitem_cb), NULL);
					g_signal_connect(factory, "bind", G_CALLBACK(bind_listitem_cb), NULL);
					model = create_application_list ();
					list = gtk_list_view_new(GTK_SELECTION_MODEL(gtk_single_selection_new(model)),factory);
					g_signal_connect(list, "activate", G_CALLBACK(activate_cb), NULL);
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
		gtk_window_set_child(GTK_WINDOW (window), box);
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
	panel_path = (char*)malloc(sizeof(char) * (MAX_LENGTH + strlen(home_env) + 40));
	sprintf(panel_path, "%s/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-panel.xml", home_env);
	double *arr = get_color_double_array(pattern, panel_path, &size);
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



void free_device_ports(char **ports) {
    for (int i = 0; i < 3; ++i) {
        free(ports[i]);
    }
    free(ports);
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

	FILE *fp = popen(command, "r");
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
		snprintf(command, sizeof(command), "sed -i \"s/$(grep -A3 '%s' %s | awk -F '\"' '/value=/ {print $4}' | sed -n %dp)/%.17lf/g\" %s", data->pattern, panel_path, data->index + 1, result[0], panel_path);

		int res = system(command);

		snprintf(command, sizeof(command), "sed -i \"s/$(grep -A3 '%s' %s | awk -F '\"' '/value=/ {print $4}' | sed -n %dp)/%.17lf/g\" %s", data->pattern, panel_path, data->index + 2, result[1], panel_path);

		res = system(command);
		snprintf(command, sizeof(command), "sed -i \"s/$(grep -A3 '%s' %s | awk -F '\"' '/value=/ {print $4}' | sed -n %dp)/%.17lf/g\" %s", data->pattern, panel_path, data->index + 3, result[2], panel_path);

		res = system(command);
		free(result);
	}
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
	const char *timezone, *selected_timezone = gtk_string_list_get_string(timezone_string_list, position);
	while (selected_timezone[idx] != '\0') {
		if (selected_timezone[idx] <= 32 && selected_timezone[idx + 1] > 32)
			timezone = &selected_timezone[idx + 1];
		idx++;
	}
	char command[2048];
	sprintf(command, "ln -sf /usr/share/zoneinfo/%s /tmp/localtime", timezone);
	system(command);
}

GtkWidget *timezone_settings(void) {
	GtkWidget *timezone_vbox, *frame, *main_vbox, *vbox1, *vbox2, *vbox3, *label, *save_button;
	timezone_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	frame = gtk_frame_new(NULL);
	gtk_widget_set_vexpand(frame, TRUE);
	main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	//gtk_widget_set_vexpand(vb, TRUE);
	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	//gtk_widget_set_vexpand(vbox1, TRUE);
	label = gtk_label_new("Select a timezone");
	gtk_widget_set_margin_top(label, 5);
	gtk_widget_set_valign(label, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(vbox1), label);
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_vexpand(vbox2, TRUE);
	timezone_string_list = gtk_string_list_new((const char* const*)timezones);
	FILE *command = popen("find /usr/share/zoneinfo/ -type f -exec grep -IL . \"{}\" \\; | cut -d '/' -f5- | sort | nl", "r");
	char buffer[2048];
	int position = current_timezone_position();
	while (fgets(buffer, sizeof(buffer), command) != NULL) {
		char *p = strchr(buffer, '\n');
		if (p) *p = '\0';
		gtk_string_list_append(timezone_string_list, buffer);
	}
	pclose(command);
	timezone_dropdown_menu = gtk_drop_down_new((void*)timezone_string_list, NULL);
	gtk_drop_down_set_enable_search((void*)timezone_dropdown_menu, TRUE);
	gtk_drop_down_set_selected((void*)timezone_dropdown_menu, position - 1);
	gtk_box_append(GTK_BOX(vbox2), timezone_dropdown_menu);
	vbox3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	//gtk_widget_set_vexpand(vbox3, TRUE);
	save_button = gtk_button_new_with_label("Save");
	g_signal_connect(G_OBJECT(save_button), "clicked", G_CALLBACK(timezone_save_button_clicked), NULL);
	const gchar *css_style = "button { background: blue; color: white; } button:hover { background: darkblue; color: white; }";
	GtkCssProvider *css_provider = gtk_css_provider_new();
	gtk_css_provider_load_from_data(css_provider, css_style, -1);
	GtkStyleContext *style_context = gtk_widget_get_style_context(save_button);
	gtk_style_context_add_provider(style_context, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_widget_set_valign(save_button, GTK_ALIGN_END);
	gtk_box_append(GTK_BOX(vbox3), save_button);
	gtk_box_append(GTK_BOX(main_vbox), vbox1);
	gtk_box_append(GTK_BOX(main_vbox), vbox2);
	gtk_box_append(GTK_BOX(main_vbox), vbox3);
	gtk_frame_set_child(GTK_FRAME(frame), main_vbox);
	gtk_box_append(GTK_BOX(timezone_vbox), frame);
	return timezone_vbox;
}



void set_device_volume(GtkRange *scale, gpointer user_data) {
	VolumeCallbackData *data = (VolumeCallbackData*)user_data;
	char *device_name = data->device_name;
	char buffer[2048], command[2048];
	int new_value = gtk_range_get_value(scale);
	sprintf(data->value_percentage_str, "%d%% (%.2f dB)", new_value, data->db);
	sprintf(buffer, "%d", new_value);
	sprintf(command, "pamixer --sink %s --set-volume %s --allow-boost", device_name, buffer);
	system(command);


	gtk_scale_clear_marks(GTK_SCALE(scale));

	// Add back all marks with updated labels
	gtk_scale_add_mark(GTK_SCALE(scale), 0.00, GTK_POS_BOTTOM, "Silence");
	gtk_scale_add_mark(GTK_SCALE(scale), 0.00, GTK_POS_LEFT, data->channel_name);
	gtk_scale_add_mark(GTK_SCALE(scale), 100.0, GTK_POS_BOTTOM, "100% (0 dB)");
	gtk_scale_add_mark(GTK_SCALE(scale), 153.0, GTK_POS_RIGHT, data->value_percentage_str);


	//gtk_scale_add_mark(GTK_SCALE(scale), 153.0, GTK_POS_RIGHT, data->value_percentage_str);
}


GtkWidget *get_device_channel_box(char *device_name, Channel *channel_map, int size, int is_active) {
	GtkWidget *channels_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	for (int i = 0; i < size; i++) {
		// Create volume scale widget
		GtkWidget *channel_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		GtkWidget *volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 153.0, 1.0);
		gtk_widget_set_margin_start(volume_scale, 20);
		gtk_widget_set_margin_end(volume_scale, 20);
		gtk_scale_add_mark(GTK_SCALE(volume_scale), 0.00, GTK_POS_BOTTOM, "Silence");
		gtk_scale_add_mark(GTK_SCALE(volume_scale), 0.00, GTK_POS_LEFT, channel_map[i].channel);
		gtk_scale_add_mark(GTK_SCALE(volume_scale), 100.0, GTK_POS_BOTTOM, "100% (0 dB)");
		char device_volume_str[32];
		sprintf(device_volume_str, "%d%% (%.2f dB)", channel_map[i].volume->value_percentage, channel_map[i].volume->db);
		gtk_scale_add_mark(GTK_SCALE(volume_scale), 153.0, GTK_POS_RIGHT, device_volume_str);

		gtk_range_set_value((void*)volume_scale, channel_map[i].volume->value_percentage);
		gtk_scale_set_draw_value((void*)volume_scale, channel_map[i].volume->value_percentage);

		VolumeCallbackData *data = g_new(VolumeCallbackData, 1);
		data->device_name = device_name;
		data->channel_name = channel_map[i].channel;
		data->value_percentage = channel_map[i].volume->value_percentage;
		data->db = channel_map[i].volume->db;

		g_signal_connect(volume_scale, "value-changed", G_CALLBACK(set_device_volume), (gpointer)data);




		//g_signal_connect(volume_scale, "value-changed", G_CALLBACK(set_device_volume_label), volume_percentage_str);

		//gtk_scale_add_mark(GTK_SCALE(volume_scale), 153.0, GTK_POS_RIGHT, data->value_percentage_str);
		gtk_box_append(GTK_BOX(channel_box), volume_scale);

		if (strcmp(channel_map[i].channel, "front-left") == 0 && !is_active) {
			gtk_widget_set_visible(channel_box, FALSE);
		}

		gtk_widget_set_name(channel_box, channel_map[i].channel); // Set the name of the channel_box
		gtk_box_append(GTK_BOX(channels_box), channel_box);
	}

	return channels_box;
}

void update_channels_value_percentage(GtkWidget *channels_box, double new_value) {
	// Get the first child of channels_box
	GtkWidget *child = gtk_widget_get_first_child(channels_box);

	// Loop through the children
	while (child != NULL) {
		// Check if the child widget is the one we want to update
		const gchar *channel_name = gtk_widget_get_name(child);
		if (channel_name != NULL && strcmp(channel_name, "front-left") == 0) {
			// Update the value_percentage of the front-left channel
			GtkRange *volume_scale = GTK_RANGE(gtk_widget_get_first_child(child));
			gtk_range_set_value(volume_scale, new_value);
			break;
		}

		// Move to the next child
		child = gtk_widget_get_next_sibling(child);
	}
}

void toggle_channels_visibility(GtkWidget *button, gpointer user_data) {
	GtkWidget *channels_box = GTK_WIDGET(user_data);

	// Get the first child of channels_box
	GtkWidget *child = gtk_widget_get_first_child(channels_box);

	// Loop through the children
	while (child != NULL) {
		// Check if the child widget is the one we want to hide/show
		const gchar *channel_name = gtk_widget_get_name(child);
		if (channel_name != NULL && strcmp(channel_name, "front-left") == 0) {
			// Toggle visibility based on the state of the toggle button
			gboolean is_active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
			gtk_widget_set_visible(child, !is_active);

			// If the channel_lock_button is active, update the value_percentage of the front-left channel
			if (is_active) {
				// Get the value_percentage of the front-right channel
				GtkWidget *front_right_child = gtk_widget_get_next_sibling(child);
				GtkRange *front_right_scale = GTK_RANGE(gtk_widget_get_first_child(front_right_child));
				double new_value = gtk_range_get_value(front_right_scale);

				// Update the value_percentage of the front-left channel
				update_channels_value_percentage(channels_box, new_value);
			}

			break;
		}

		// Move to the next child
		child = gtk_widget_get_next_sibling(child);
	}
}

void set_default_button_state(GtkDropDown *dropdown, gpointer user_data) {

	int selected_index = gtk_drop_down_get_selected(GTK_DROP_DOWN(dropdown));
	GListModel *string_list = gtk_drop_down_get_model(GTK_DROP_DOWN(dropdown));
	const char *selected_port_name = gtk_string_list_get_string((void*)string_list, selected_index);

	char *active_port_name = g_object_get_data(G_OBJECT(dropdown), "active_port_name");
	char *default_device_name = g_object_get_data(G_OBJECT(dropdown), "default_device_name");
	char *current_default_device_name = get_device_default(OUTPUT_DEVICES);
	//printf("%s %s\n", default_device_name, current_default_device_name);
	if (strcmp(default_device_name, current_default_device_name) == 0) {
		if (check_port_availability(selected_port_name) && strcmp(selected_port_name, active_port_name) == 0) {
			//printf("%s %s\n", selected_port_name, active_port_name);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(current_default_button), TRUE);
		} else {
			//printf("%s %s\n", selected_port_name, active_port_name);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(current_default_button), FALSE);

		}
	}
}

GtkWidget *volume_audio_settings(void) {
	GtkWidget *stack = gtk_stack_new();
	GtkWidget *stackSwitcher = gtk_stack_switcher_new();
	gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(stackSwitcher), GTK_STACK(stack));
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_append(GTK_BOX(box), stackSwitcher);
	gtk_box_append(GTK_BOX(box), stack);

	GtkWidget *output_devices_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget *input_devices_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	int output_devices_size = get_devices_size(OUTPUT_DEVICES);
	int input_devices_size = get_devices_size(INPUT_DEVICES);
	Device **output_devices = get_devices(OUTPUT_DEVICES);
	Device **input_devices = get_devices(INPUT_DEVICES);

	//GtkWidget *default_buttons[output_devices_size];



	for (int i = 0; i < output_devices_size; i++) {
		GtkWidget *frame = gtk_frame_new(NULL);
		GtkWidget *frame_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

		GtkWidget *frame_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		gtk_box_set_homogeneous(GTK_BOX(frame_hbox), TRUE);
		gtk_widget_set_margin_top(frame_hbox, 5);
		gtk_widget_set_margin_bottom(frame_hbox, 5);

		GtkWidget *left_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		GtkWidget *right_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

		GtkWidget *label = gtk_label_new(output_devices[i]->description);
		GtkWidget *mute_button = gtk_toggle_button_new_with_label("🔇");
		GtkWidget *channel_lock_button = gtk_toggle_button_new_with_label("");

		GtkWidget *default_button = gtk_toggle_button_new_with_label("");
		//g_signal_connect(channel_lock_button, "toggled", G_CALLBACK(toggle_channels_visibility), NULL);
		//default_buttons[i] = default_button;


		gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
		gtk_widget_set_margin_start(left_hbox, 5);
		gtk_box_append(GTK_BOX(left_hbox), label);

		gtk_widget_set_halign(GTK_WIDGET(mute_button), GTK_ALIGN_END);
		gtk_widget_set_margin_end(right_hbox, 5);

		gtk_box_append(GTK_BOX(right_hbox), mute_button);
		gtk_box_append(GTK_BOX(right_hbox), channel_lock_button);
		gtk_box_append(GTK_BOX(right_hbox), default_button);

		gtk_box_append(GTK_BOX(frame_hbox), left_hbox);
		gtk_box_append(GTK_BOX(frame_hbox), right_hbox);

		gtk_widget_set_halign(GTK_WIDGET(left_hbox), GTK_ALIGN_START);
		gtk_widget_set_halign(GTK_WIDGET(right_hbox), GTK_ALIGN_END);

		gtk_box_append(GTK_BOX(frame_vbox), frame_hbox);

		GtkWidget *ports_label = gtk_label_new("Port: ");

		Ports *ports = output_devices[i]->ports;
		int port_size = get_device_port_size(OUTPUT_DEVICES, i);
		Port *device_ports = ports->ports;


		qsort(device_ports, port_size, sizeof(Port), compare_ports);

		int active_port_index;
		char *active_port_name = get_device_active_port(OUTPUT_DEVICES, i);
		char *device_ports_list[] = {NULL};
		GtkStringList *device_ports_string_list = gtk_string_list_new((const char* const*)device_ports_list);

		for (int j = 0; j < port_size; j++) {
			gtk_string_list_append(device_ports_string_list, device_ports[j].name);
			if (strcmp(active_port_name, device_ports->name) == 0) {
				active_port_index = j;
			}
		}

		GtkWidget *ports_dropdown_menu = gtk_drop_down_new((void*)device_ports_string_list, NULL);

		if (strcmp(output_devices[i]->name, get_device_default(OUTPUT_DEVICES)) == 0) {
			//printf("%s\n", output_devices[i]->name);
			//printf("%s\n", get_device_default(OUTPUT_DEVICES));
			for (int j = 0; j < port_size; j++) {
				// Check if the port is available
				if (strcmp(device_ports[j].availability, "available") == 0 || strcmp(device_ports[j].availability, "availability unknown") == 0) {
					// Set the selected option in the dropdown menu
					gtk_drop_down_set_selected((void*)ports_dropdown_menu, j);
					// Toggle the default button
					gtk_toggle_button_set_active((void*)default_button, TRUE);
					current_default_button = default_button;

				}
			}
		}

		g_object_set_data(G_OBJECT(default_button), "option", GINT_TO_POINTER(OUTPUT_DEVICES));
		g_signal_connect(default_button, "toggled", G_CALLBACK(default_button_toggled), output_devices[i]->name);
		g_object_set_data(G_OBJECT(ports_dropdown_menu), "default_device_name", get_device_default(OUTPUT_DEVICES));
		g_object_set_data(G_OBJECT(ports_dropdown_menu), "active_port_name", active_port_name);
		//g_object_set_data(G_OBJECT(ports_dropdown_menu), "ports_string_list", device_ports_string_list);
		//g_object_set_data(G_OBJECT(ports_dropdown_menu), "default_button", default_button);
		//g_object_set_data(G_OBJECT(ports_dropdown_menu), "option_toggled", GINT_TO_POINTER(TRUE));
		//g_signal_connect_after(ports_dropdown_menu, "notify::selected", G_CALLBACK(set_default_button_state), NULL);
		g_signal_connect_after(ports_dropdown_menu, "notify::selected", G_CALLBACK(set_default_button_state), NULL);

		frame_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		//gtk_box_set_homogeneous(GTK_BOX(frame_hbox), FALSE);
		gtk_widget_set_margin_top(frame_hbox, 5);
		gtk_widget_set_margin_bottom(frame_hbox, 5);

		left_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		right_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		//gtk_box_set_homogeneous(GTK_BOX(right_hbox), TRUE);
		gtk_widget_set_hexpand(right_hbox, TRUE);

		gtk_widget_set_halign(GTK_WIDGET(ports_label), GTK_ALIGN_START);
		gtk_widget_set_margin_start(left_hbox, 5);
		gtk_box_append(GTK_BOX(left_hbox), ports_label);

		gtk_widget_set_halign(GTK_WIDGET(ports_dropdown_menu), GTK_ALIGN_CENTER);
		gtk_widget_set_margin_end(right_hbox, 5);
		gtk_widget_set_hexpand(ports_dropdown_menu, TRUE);
		gtk_widget_set_halign(right_hbox, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(right_hbox), ports_dropdown_menu);

		gtk_box_append(GTK_BOX(frame_hbox), left_hbox);
		gtk_box_append(GTK_BOX(frame_hbox), right_hbox);

		gtk_widget_set_halign(GTK_WIDGET(left_hbox), GTK_ALIGN_START);
		//gtk_widget_set_halign(GTK_WIDGET(right_hbox), GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(frame_vbox), frame_hbox);



		float balance = output_devices[i]->balance_field->balance;
		int is_active = balance == 0.00 ?  1 : 0;

		GtkWidget *channels_box = get_device_channel_box(output_devices[i]->name, output_devices[i]->channel_map, get_device_channel_size(OUTPUT_DEVICES, i), !is_active);
		gtk_box_append(GTK_BOX(frame_vbox), channels_box);
		if (is_active)
			gtk_toggle_button_set_active((void*)channel_lock_button, TRUE);

		//gtk_box_append(GTK_BOX(output_devices_vbox), channels_box);

		// Connect toggle_channels_visibility function to channel_lock_button
		g_signal_connect(channel_lock_button, "toggled", G_CALLBACK(toggle_channels_visibility), channels_box);



		//g_signal_connect(channel_lock_button, "toggled", G_CALLBACK(toggle_channels_visibility), channel_box);
		//g_signal_connect(channel_lock_button, "toggled", G_CALLBACK(toggle_channels_visibility), channels_box);

		////gtk_widget_set_halign(GTK_WIDGET(right_hbox), GTK_ALIGN_END);
		////gtk_widget_set_halign(GTK_WIDGET(right_hbox), GTK_ALIGN_START);
		gtk_frame_set_child(GTK_FRAME(frame), frame_vbox);
		gtk_box_append(GTK_BOX(output_devices_vbox), frame);

	}



	for (int i = 0; i < input_devices_size; i++) {
		GtkWidget *frame = gtk_frame_new(NULL);
		GtkWidget *frame_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

		GtkWidget *frame_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		gtk_box_set_homogeneous(GTK_BOX(frame_hbox), TRUE);
		gtk_widget_set_margin_top(frame_hbox, 5);
		gtk_widget_set_margin_bottom(frame_hbox, 5);

		GtkWidget *left_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		GtkWidget *right_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

		GtkWidget *label = gtk_label_new(input_devices[i]->description);
		GtkWidget *mute_button = gtk_toggle_button_new_with_label("🔇");
		GtkWidget *channel_lock_button = gtk_toggle_button_new_with_label("");

		GtkWidget *default_button = gtk_toggle_button_new_with_label("");
		//g_signal_connect(channel_lock_button, "toggled", G_CALLBACK(toggle_channels_visibility), NULL);
		//default_buttons[i] = default_button;


		gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
		gtk_widget_set_margin_start(left_hbox, 5);
		gtk_box_append(GTK_BOX(left_hbox), label);

		gtk_widget_set_halign(GTK_WIDGET(mute_button), GTK_ALIGN_END);
		gtk_widget_set_margin_end(right_hbox, 5);

		gtk_box_append(GTK_BOX(right_hbox), mute_button);
		gtk_box_append(GTK_BOX(right_hbox), channel_lock_button);
		gtk_box_append(GTK_BOX(right_hbox), default_button);

		gtk_box_append(GTK_BOX(frame_hbox), left_hbox);
		gtk_box_append(GTK_BOX(frame_hbox), right_hbox);

		gtk_widget_set_halign(GTK_WIDGET(left_hbox), GTK_ALIGN_START);
		gtk_widget_set_halign(GTK_WIDGET(right_hbox), GTK_ALIGN_END);

		gtk_box_append(GTK_BOX(frame_vbox), frame_hbox);

		GtkWidget *ports_label = gtk_label_new("Port: ");

		Ports *ports = input_devices[i]->ports;
		int port_size = get_device_port_size(INPUT_DEVICES, i);
		Port *device_ports = ports->ports;


		qsort(device_ports, port_size, sizeof(Port), compare_ports);

		int active_port_index;
		char *active_port_name = get_device_active_port(INPUT_DEVICES, i);
		char *device_ports_list[] = {NULL};
		GtkStringList *device_ports_string_list = gtk_string_list_new((const char* const*)device_ports_list);

		for (int j = 0; j < port_size; j++) {
			gtk_string_list_append(device_ports_string_list, device_ports[j].name);
			if (strcmp(active_port_name, device_ports->name) == 0) {
				active_port_index = j;
			}
		}

		GtkWidget *ports_dropdown_menu = gtk_drop_down_new((void*)device_ports_string_list, NULL);

		if (strcmp(input_devices[i]->name, get_device_default(INPUT_DEVICES)) == 0) {
			//printf("%s\n", output_devices[i]->name);
			//printf("%s\n", get_device_default(OUTPUT_DEVICES));
			for (int j = 0; j < port_size; j++) {
				// Check if the port is available
				if (strcmp(device_ports[j].availability, "available") == 0 || strcmp(device_ports[j].availability, "availability unknown") == 0) {
					// Set the selected option in the dropdown menu
					gtk_drop_down_set_selected((void*)ports_dropdown_menu, j);
					// Toggle the default button
					gtk_toggle_button_set_active((void*)default_button, TRUE);
					current_default_button = default_button;

				}
			}
		}

		g_object_set_data(G_OBJECT(default_button), "option", GINT_TO_POINTER(INPUT_DEVICES));
		g_signal_connect(default_button, "toggled", G_CALLBACK(default_button_toggled), input_devices[i]->name);
		g_object_set_data(G_OBJECT(ports_dropdown_menu), "default_device_name", get_device_default(INPUT_DEVICES));
		g_object_set_data(G_OBJECT(ports_dropdown_menu), "active_port_name", active_port_name);
		//g_object_set_data(G_OBJECT(ports_dropdown_menu), "ports_string_list", device_ports_string_list);
		//g_object_set_data(G_OBJECT(ports_dropdown_menu), "default_button", default_button);
		//g_object_set_data(G_OBJECT(ports_dropdown_menu), "option_toggled", GINT_TO_POINTER(TRUE));
		//g_signal_connect_after(ports_dropdown_menu, "notify::selected", G_CALLBACK(set_default_button_state), NULL);
		g_signal_connect_after(ports_dropdown_menu, "notify::selected", G_CALLBACK(set_default_button_state), NULL);

		frame_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		//gtk_box_set_homogeneous(GTK_BOX(frame_hbox), FALSE);
		gtk_widget_set_margin_top(frame_hbox, 5);
		gtk_widget_set_margin_bottom(frame_hbox, 5);

		left_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		right_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		//gtk_box_set_homogeneous(GTK_BOX(right_hbox), TRUE);
		gtk_widget_set_hexpand(right_hbox, TRUE);

		gtk_widget_set_halign(GTK_WIDGET(ports_label), GTK_ALIGN_START);
		gtk_widget_set_margin_start(left_hbox, 5);
		gtk_box_append(GTK_BOX(left_hbox), ports_label);

		gtk_widget_set_halign(GTK_WIDGET(ports_dropdown_menu), GTK_ALIGN_CENTER);
		gtk_widget_set_margin_end(right_hbox, 5);
		gtk_widget_set_hexpand(ports_dropdown_menu, TRUE);
		gtk_widget_set_halign(right_hbox, GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(right_hbox), ports_dropdown_menu);

		gtk_box_append(GTK_BOX(frame_hbox), left_hbox);
		gtk_box_append(GTK_BOX(frame_hbox), right_hbox);

		gtk_widget_set_halign(GTK_WIDGET(left_hbox), GTK_ALIGN_START);
		//gtk_widget_set_halign(GTK_WIDGET(right_hbox), GTK_ALIGN_START);
		gtk_box_append(GTK_BOX(frame_vbox), frame_hbox);



		float balance = input_devices[i]->balance_field->balance;
		int is_active = balance == 0.00 ?  1 : 0;

		GtkWidget *channels_box = get_device_channel_box(input_devices[i]->name, input_devices[i]->channel_map, get_device_channel_size(INPUT_DEVICES, i), !is_active);
		gtk_box_append(GTK_BOX(frame_vbox), channels_box);
		if (is_active)
			gtk_toggle_button_set_active((void*)channel_lock_button, TRUE);

		//gtk_box_append(GTK_BOX(output_devices_vbox), channels_box);

		// Connect toggle_channels_visibility function to channel_lock_button
		g_signal_connect(channel_lock_button, "toggled", G_CALLBACK(toggle_channels_visibility), channels_box);



		//g_signal_connect(channel_lock_button, "toggled", G_CALLBACK(toggle_channels_visibility), channel_box);
		//g_signal_connect(channel_lock_button, "toggled", G_CALLBACK(toggle_channels_visibility), channels_box);

		////gtk_widget_set_halign(GTK_WIDGET(right_hbox), GTK_ALIGN_END);
		////gtk_widget_set_halign(GTK_WIDGET(right_hbox), GTK_ALIGN_START);
		gtk_frame_set_child(GTK_FRAME(frame), frame_vbox);
		gtk_box_append(GTK_BOX(input_devices_vbox), frame);

	}

	gtk_stack_add_titled(GTK_STACK(stack), output_devices_vbox, NULL, "Output Devices");
	gtk_stack_add_titled(GTK_STACK(stack), input_devices_vbox, NULL, "Input Devices");
	return box;
}

char *get_device_port_availability(pa_autoload_type type, int device_index, int port_index) {
	Device **devices = get_devices(type);
	Device *device = devices[device_index];

	if (device_index < 0 || device_index >= get_devices_size(type)) {
		return NULL; // Invalid index
	}

	if (port_index < 0 || port_index >= get_device_port_size(type, device_index)) {
		return NULL; // Invalid index
	}

	Port *ports = device->ports->ports;

	if (strcmp(ports[port_index].availability, "available") == 0) {
		return strdup("available");
	} else if (strcmp(ports[port_index].availability, "availability unknown") == 0) {
		return strdup("availability unknown");
	} else {
		return strdup("unavailable");
	}
}

void default_button_toggled(GtkWidget *button, gpointer user_data) {
	char *device_name = (char*)user_data;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) {
		if (current_default_button != NULL && !programmatic_toggling) {
			if (current_default_button == button) {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(current_default_button), TRUE);
				return;
			} else {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(current_default_button), FALSE);
			}
		}
		current_default_button = button;
		pa_autoload_type option = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "option"));
		switch (option) {
			case OUTPUT_DEVICES:
				system(g_strdup_printf("pactl set-default-sink %s", device_name));
			break;
			case INPUT_DEVICES:
				system(g_strdup_printf("pactl set-default-source %s", device_name));
			break;
			default:
			break;
		}
	}
}

bool check_port_availability(const char *port_name) {
	if (strcmp(port_name, "not available") == 0) {
		return false;
	} else {
		return true;
	}
}

int compare_ports(const void *a, const void *b) {
	Port *port1 = (Port *)a;
	Port *port2 = (Port *)b;
	if (strcmp(port1->availability, "available") == 0) {
		if (strcmp(port2->availability, "available") != 0) {
			return -1;
		}
	} else if (strcmp(port1->availability, "availability unknown") == 0) {
		if (strcmp(port2->availability, "available") == 0) {
			return 1;
		} else if (strcmp(port2->availability, "availability unknown") != 0) {
			return -1;
		}
	} else {
		if (strcmp(port2->availability, "not available") != 0) {
			return 1;
		}
	}
	if (port1->priority < port2->priority) {
		return 1;
	} else if (port1->priority > port2->priority) {
		return -1;
	}
	return 0;
}

Device **get_devices(pa_autoload_type type) {
	int count = get_devices_size(type);
	if (count <= 0) {
		return NULL;
	}
	Device **devices = malloc(count * sizeof(Device *));
	if (!devices) {
		fprintf(stderr, "Memory allocation failed for devices\n");
		return NULL;
	}
	for (int i = 0; i < count; i++) {
		devices[i] = malloc(sizeof(Device));
		if (!devices[i]) {
			free_devices(devices, i);
			fprintf(stderr, "Memory allocation failed for device at index %d\n", i);
			return NULL;
		}
		devices[i]->index = get_device_index(type, i);
		devices[i]->state = get_device_state(type, i);
		devices[i]->name = get_device_name(type, i);
		devices[i]->description = get_device_description(type, i);
		int channel_size = get_device_channel_size(type, i);
		char **channel_names = get_device_channel_names(type, i, channel_size);
		devices[i]->channel_map = malloc(channel_size * sizeof(Channel));
		if (!devices[i]->channel_map) {
			free_devices(devices, i + 1);
			fprintf(stderr, "Memory allocation failed for channel map at index %d\n", i);
			return NULL;
		}
		for (int j = 0; j < channel_size; j++) {
			devices[i]->channel_map[j].channel = channel_names[j];
			get_device_channel_properties(type, i, &devices[i]->channel_map[j]);

		}
		free(channel_names);
		devices[i]->mute = get_device_mute(type, i);
		devices[i]->balance_field = malloc(sizeof(Balance));
		if (!devices[i]->balance_field) {
			free_devices(devices, i + 1);
			fprintf(stderr, "Memory allocation failed for balance field at index %d\n", i);
			return NULL;
		}
		devices[i]->balance_field->balance = get_device_balance(type, i);
		get_device_base_volume_properties(type, i, devices[i]->balance_field);
		devices[i]->ports = malloc(sizeof(Ports));
		if (!devices[i]->ports) {
			free_devices(devices, i + 1);
			fprintf(stderr, "Memory allocation failed for ports at index %d\n", i);
			return NULL;
		}
		devices[i]->ports = get_device_port_properties(type, i);
		devices[i]->ports->active_port = get_device_active_port(type, i);
	}
	return devices;
}

int get_devices_size(pa_autoload_type type) {
	int count = 0;
	char buffer[2048];
	char command[2048];
	if (type < 0 || type >= sizeof(pa_options) / sizeof(pa_options[0])) {
		return 0;
	}
	sprintf(command, "pactl -f json list %s | jq length", pa_options[type]);
	FILE *cmd = popen(command, "r");
if (cmd) {
		if (fgets(buffer, sizeof(buffer), cmd) != NULL) {
			sscanf(buffer, "%d", &count);
		}
		pclose(cmd);
	}
	return count;
}

uint32_t get_device_index(pa_autoload_type type, int index) {
	uint32_t device_index;
	char buffer[2048], command[2048];
	sprintf(command, "pactl -f json list %s | jq '.[%d].index'", pa_options[type], index);
	FILE *process = popen(command, "r");
	if (!process) {
		fprintf(stderr, "Failed to execute command: %s\n", command);
	}
	while (fgets(buffer, sizeof(buffer), process) != NULL) {
		device_index = (uint32_t)strtoul(buffer, NULL, 10);
	}
	pclose(process);
	return device_index;
}

char *get_device_name(pa_autoload_type type, int index) {
	char buffer[2048], command[2048];
	sprintf(command, "pactl -f json list %s | jq '.[%d].name' | tr -d '\"' ", pa_options[type], index);
	FILE *process = popen(command, "r");
	if (!process) {
		fprintf(stderr, "Failed to execute command: %s\n", command);
		return NULL;
	}
	if (fgets(buffer, sizeof(buffer), process) != NULL) {
		char *p = strchr(buffer, '\n');
		if (p) *p = '\0';
	}
	pclose(process);
	char *name = strdup(buffer);
	if (!name) {
		fprintf(stderr, "Memory allocation failed for state\n");
		return NULL;
	}
	return name;
}

char *get_device_state(pa_autoload_type type, int index) {
	char buffer[2048], command[2048];
	sprintf(command, "pactl -f json list %s | jq '.[%d].state' | tr -d '\"' ", pa_options[type], index);
	FILE *process = popen(command, "r");
	if (!process) {
		fprintf(stderr, "Failed to execute command: %s\n", command);
		return NULL;
	}
	if (fgets(buffer, sizeof(buffer), process) != NULL) {
		char *p = strchr(buffer, '\n');
		if (p) *p = '\0';
	}
	pclose(process);
	char *state = strdup(buffer);
	if (!state) {
		fprintf(stderr, "Memory allocation failed for state\n");
		return NULL;
	}
	return state;
}

char *get_device_description(pa_autoload_type type, int index) {
	char buffer[2048], command[2048];
	sprintf(command, "pactl -f json list %s | jq '.[%d].description' | tr -d '\"' ", pa_options[type], index);
	FILE *process = popen(command, "r");
	if (!process) {
		fprintf(stderr, "Failed to execute command: %s\n", command);
		return NULL;
	}
	if (fgets(buffer, sizeof(buffer), process) != NULL) {
		char *p = strchr(buffer, '\n');
		if (p) *p = '\0';
	}
	pclose(process);
	char *desc = strdup(buffer);
	if (!desc) {
		fprintf(stderr, "Memory allocation failed for state\n");
		return NULL;
	}
	return desc;
}

int get_device_channel_size(pa_autoload_type type, int index) {
	int size;
	char command[2048], buffer[2048];
	sprintf(command, "pactl -f json list %s | jq '.[%d].channel_map' | tr -d '\"' | tr ',' ' ' | wc -w", pa_options[type], index);
	FILE *process = popen(command, "r");
	if (!process) {
		fprintf(stderr, "Failed to execute command: %s\n", command);
	}
	if (fgets(buffer, sizeof(buffer), process) != NULL) {
		char *p = strchr(buffer, '\0');
		if (p) *p = '\0';
	}
	sscanf(buffer, "%d", &size);
	return size;
}

char **get_device_channel_names(pa_autoload_type type, int index, int size) {
	int i = 0;
	FILE *process;
	char command[2048], buffer[2048], **channel_names;
	channel_names = (char**)malloc(size * sizeof(char*));
	sprintf(command, "pactl -f json list %s | jq '.[%d].channel_map' | tr -d '\"' | tr ',' '\\n'", pa_options[type], index);
	process = popen(command, "r");
	while (fgets(buffer, sizeof(buffer), process) != NULL) {
		if (i >= size) {
			fprintf(stderr, "Exceeded specified size\n");
			break;
		}
		char *p = strchr(buffer, '\n');
		if (p) *p = '\0';
		*(channel_names + i) = (char*)malloc((strlen(buffer) + 1) * sizeof(char));
		if (channel_names[i] == NULL) {
			perror("Memory allocation failed");
			exit(EXIT_FAILURE);
		}
		strcpy(channel_names[i], buffer);
		i++;
	}
	return channel_names;
}

int get_device_mute(pa_autoload_type type, int index) {
	FILE *process;
	char command[2048], buffer[2048];
	sprintf(command, "pactl -f json list %s | jq '.[%d].mute'", pa_options[type], index);
	int result;

	process = popen(command, "r");
	if (!process) {
		perror("Error opening pipe");
		exit(EXIT_FAILURE);
	}

	if (fgets(buffer, sizeof(buffer), process) != NULL) {
		char *p = strchr(buffer, '\n');
		if (p) *p = '\0';
		if (strcmp(buffer, "false") == 0) {
			result = 0;
		} else if (strcmp(buffer, "true") == 0) {
			result = 1;
		} else {
			fprintf(stderr, "Unexpected output format: %s\n", buffer);
		}
} else {
		fprintf(stderr, "No output received\n");
	}

	pclose(process);
	return result;
}

void get_device_channel_properties(pa_autoload_type type, int index, Channel *channel) {
	char command[2048], buffer[2048];
	sprintf(command, "pactl -f json list %s | jq '.[%d].volume.\"%s\" | .value, .value_percent, .db' | sed 's/\"//g; s/%%//; s/ dB//'", pa_options[type], index, channel->channel);
	FILE *process = popen(command, "r");
	if (!process) {
		perror("Failed to execute command");
		exit(EXIT_FAILURE);
	}
	channel->volume = malloc(sizeof(Volume));
	if (!channel->volume) {
		perror("Memory allocation failed");
		exit(EXIT_FAILURE);
	}
	int i = 0;
	while (fgets(buffer, sizeof(buffer), process) != NULL) {
		char *p = strchr(buffer, '\n');
		if (p) *p = '\0';
		if (i == 0) {
			sscanf(buffer, "%u", &channel->volume->value);
		} else if (i == 1) {
			sscanf(buffer, "%d", &channel->volume->value_percentage);
		} else if (i == 2) {
			sscanf(buffer, "%f", &channel->volume->db);
		}
		i++;
	}
	pclose(process);
}

float get_device_balance(pa_autoload_type type, int index) {
	float balance;
	char command[2048], buffer[2048];
	sprintf(command, "pactl -f json list %s | jq '.[%d].balance'", pa_options[type], index);
	FILE *process = popen(command, "r");
	if (!process) {
		fprintf(stderr, "Failed to execute command: %s\n", command);
	}
	if (fgets(buffer, sizeof(buffer), process) != NULL) {
		char *p = strchr(buffer, '\0');
		if (p) *p = '\0';
	}
	sscanf(buffer, "%f", &balance);
	return balance;
}

void get_device_base_volume_properties(pa_autoload_type type, int index, Balance *balance_field) {
	char command[2048], buffer[2048];
	sprintf(command, "pactl -f json list %s | jq '.[%d].base_volume | .value, .value_percent, .db' | sed 's/\"//g; s/%%//; s/ dB//'", pa_options[type], index);
	FILE *process = popen(command, "r");
	if (!process) {
		perror("Failed to execute command");
		exit(EXIT_FAILURE);
	}
	balance_field->base_volume = malloc(sizeof(Volume));
	if (!balance_field->base_volume) {
		perror("Memory allocation failed");
		exit(EXIT_FAILURE);
	}
	int i = 0;
	while (fgets(buffer, sizeof(buffer), process) != NULL) {
		char *p = strchr(buffer, '\n');
		if (p) *p = '\0';
		if (i == 0) {
			sscanf(buffer, "%u", &balance_field->base_volume->value);
		} else if (i == 1) {
			sscanf(buffer, "%d", &balance_field->base_volume->value_percentage);
		} else if (i == 2) {
			sscanf(buffer, "%f", &balance_field->base_volume->db);
		}
		i++;
	}
	pclose(process);
}

int get_device_port_size(pa_autoload_type type, int index) {
	int size;
	char command[2048], buffer[2048];
	sprintf(command, "pactl -f json list %s | jq '.[%d].ports' | jq length", pa_options[type], index);
	FILE *process = popen(command, "r");
	if (!process) {
		fprintf(stderr, "Failed to execute command: %s\n", command);
	}
	if (fgets(buffer, sizeof(buffer), process) != NULL) {
		char *p = strchr(buffer, '\0');
		if (p) *p = '\0';
	}
	sscanf(buffer, "%d", &size);
	return size;
}

Ports *get_device_port_properties(pa_autoload_type type, int index) {
	char command[2048], buffer[2048];
	int port_size = get_device_port_size(type, index);
	Ports *ports = malloc(sizeof(Ports));
	if (!ports) {
		perror("Memory allocation failed for ports");
		exit(EXIT_FAILURE);
	}
	ports->ports = malloc(port_size * sizeof(Port));
	if (!ports->ports) {
		perror("Memory allocation failed for ports array");
		free(ports);
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < port_size; i++) {
		sprintf(command, "pactl -f json list %s | jq '.[%d].ports.[%d] | .name, .description, .type, .priority, .availability_group, .availability' | tr -d '\"'", pa_options[type], index, i);
		FILE *process = popen(command, "r");
		if (!process) {
			perror("Failed to execute command");
			exit(EXIT_FAILURE);
		}
		Port *port = &(ports->ports[i]);
		int j = 0;
		while (fgets(buffer, sizeof(buffer), process) != NULL) {
			char *p = strchr(buffer, '\n');
			if (p) *p = '\0';
			switch (j) {
				case 0:
					port->name = strdup(buffer);
				break;
				case 1:
					port->description = strdup(buffer);
				break;
				case 2:
					port->type = strdup(buffer);
				break;
				case 3:
					sscanf(buffer, "%u", &port->priority);
				break;
				case 4:
					port->availability_group = strdup(buffer);
				break;
				case 5:
					port->availability = strdup(buffer);
				break;
			}
			j++;
		}
	pclose(process);
	}
	return ports;
}

char *get_device_active_port(pa_autoload_type type, int index) {
	char buffer[2048], command[2048];
	sprintf(command, "pactl -f json list %s | jq '.[%d].active_port' | tr -d '\"' ", pa_options[type], index);
	FILE *process = popen(command, "r");
	if (!process) {
		fprintf(stderr, "Failed to execute command: %s\n", command);
		return NULL;
	}
	if (fgets(buffer, sizeof(buffer), process) != NULL) {
		char *p = strchr(buffer, '\n');
		if (p) *p = '\0';
	}
	pclose(process);
	char *active_port = strdup(buffer);
	if (!active_port) {
		fprintf(stderr, "Memory allocation failed for active_port\n");
		return NULL;
	}
	return active_port;
}

char *get_device_default(pa_autoload_type option) {
	char command[100];
	char *result;
	if (option == OUTPUT_DEVICES) {
		snprintf(command, sizeof(command), "pactl get-default-sink");
	} else if (option == INPUT_DEVICES) {
		snprintf(command, sizeof(command), "pactl get-default-source");
} else {
		return NULL;
	}
	FILE *cmd = popen(command, "r");
	if (!cmd) {
		return NULL;
	}
	char buffer[128];
	fgets(buffer, sizeof(buffer), cmd);
	char *p = strchr (buffer, '\n');
	if (p) *p = '\0';
	pclose(cmd);
	result = strdup(buffer);
	return result;
}

char **get_device_ports(pa_autoload_type type, int device_index, int *count) {
	char command[2048];
	if (type == OUTPUT_DEVICES) {
		sprintf(command, "pactl list sinks | awk '/^Sink #%d$/,/Active Port:/ {if (/Ports:/) start=1; if (start && /:\\s+/ && !/Active Port:/) {gsub(/[: ]/, \"\\n\"); print $1}}'", device_index);
	} else if (type == INPUT_DEVICES) {
		sprintf(command, "pactl list sources | awk '/^Source #%d$/,/Active Port:/ {if (/Ports:/) start=1; if (start && /:\\s+/ && !/Active Port:/) {gsub(/[: ]/, \"\\n\"); print $1}}'", device_index);
} else {
		return NULL;
	}
	FILE *cmd = popen(command, "r");
	if (!cmd) {
		return NULL;
	}
	char **ports = (char **)malloc(MAX_PORTS * sizeof(char *));
	char buffer[256];
	*count = 0;
	while (fgets(buffer, sizeof(buffer), cmd) != NULL && *count < MAX_PORTS) {
		buffer[strcspn(buffer, "\n")] = 0;
		ports[*count] = strdup(buffer);
		if (!ports[*count]) {
			fprintf(stderr, "Error allocating memory.\n");
			break;
		}
		(*count)++;
	}
	pclose(cmd);
	return ports;
}

void free_devices(Device **devices, int count) {
	if (!devices) return;
	for (int i = 0; i < count; i++) {
		if (!devices[i]) continue;
		free(devices[i]->state);
		free(devices[i]->name);
		free(devices[i]->description);
		if (devices[i]->channel_map) {
			for (int j = 0; j < get_device_channel_size(INPUT_DEVICES, i); j++) {
				free(devices[i]->channel_map[j].channel);
			}
			free(devices[i]->channel_map);
		}
		free(devices[i]);
	}
	free(devices);
}

WifiAccessPoint *get_access_points(int *count) {
	FILE *fp = popen("nmcli -f ACTIVE,BSSID,SSID,MODE,CHAN,FREQ,RATE,BANDWIDTH,SIGNAL,BARS,SECURITY dev wifi list --show-secrets | tail -n +2", "r");
	if (fp == NULL) {
		perror("Error executing command");
	}

	char line[MAX_AP_LINE_LENGTH];
	int ap_count = 0;

	WifiAccessPoint* access_points = malloc(MAX_ACCESS_POINTS * sizeof(WifiAccessPoint));
	if (access_points == NULL) {
		fprintf(stderr, "Memory allocation failed.\n");
	}
	while (fgets(line, sizeof(line), fp) != NULL && ap_count < MAX_ACCESS_POINTS) {
		char *token = strtok(line, " ");

		int active, chan, freq, rate, bandwidth, signal;
		char bssid[20], ssid[100], mode[20], bars[20], security[100];

		//strcpy(active, token);
		active = (strcmp(token, "yes") == 0) ? 1 : 0;

		token = strtok(NULL, " ");
		strcpy(bssid, token);

		token = strtok(NULL, " ");
		strcpy(ssid, token);

		token = strtok(NULL, " ");
		strcpy(mode, token);

		token = strtok(NULL, " ");
		chan = atoi(token);

		token = strtok(NULL, " ");
		freq = atoi(token);

		token = strtok(NULL, " MHz");
		rate = atoi(token);

		token = strtok(NULL, " Mbit/s");
		bandwidth = atoi(token);

		token = strtok(NULL, " MHz");
		signal = atoi(token);
		token = strtok(NULL, " ");
		strcpy(bars, token);
		token = strtok(NULL, "");
		char *whitespace = strpbrk(token, " \t");
		if (whitespace != NULL) {
			while (*whitespace == ' ' || *whitespace == '\t') {
				whitespace++;
			}
			strcpy(security, whitespace);
		} else {
			strcpy(security, token);
		}

		access_points[ap_count].active = active;
		strcpy(access_points[ap_count].bssid, bssid);
		strcpy(access_points[ap_count].ssid, ssid);
		strcpy(access_points[ap_count].mode, mode);
		access_points[ap_count].chan = chan;
		access_points[ap_count].freq = freq;
		access_points[ap_count].bandwidth = bandwidth;
		access_points[ap_count].signal = signal;
		strcpy(access_points[ap_count].bars, bars);
		strcpy(access_points[ap_count].security, security);
		ap_count++;

	}
	*count = ap_count;
	pclose(fp);
	return access_points;

}

static NetworkDevice *get_network_devices(void) {
	FILE *fp = popen("nmcli -t device", "r");
	if (fp == NULL) {
		perror("Error executing command");
		exit(EXIT_FAILURE);
	}

	char line[256];
	int count = get_network_interfaces_size();
	if (count < 0) {
		fprintf(stderr, "Failed to get device count.\n");
		exit(EXIT_FAILURE);
	}

	NetworkDevice *devices = (NetworkDevice*)malloc(count * sizeof(NetworkDevice));
	if (devices == NULL) {
		fprintf(stderr, "Memory allocation failed.\n");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < count; ++i) {
		if (fgets(line, sizeof(line), fp) == NULL) {
			fprintf(stderr, "Error reading from nmcli output.\n");
			exit(EXIT_FAILURE);
		}
		char interface[100], type[100], state[100], connection[100];
		sscanf(line, "%[^:]:%[^:]:%[^:]:%[^*]", interface, type, state, connection);
		char cmd[256];
		sprintf(cmd, "grep 'IFINDEX' /sys/class/net/%s/uevent 2>/dev/null | cut -d '=' -f2", interface);
		FILE *ifindex_fp = popen(cmd, "r");
		int ifindex;
		if (ifindex_fp) {
			fscanf(ifindex_fp, "%d", &ifindex);
			pclose(ifindex_fp);
		} else {
			ifindex = -1;
		}

		devices[i].interface = strdup(interface);
		devices[i].type = strdup(type);
		devices[i].state = strdup(state);
		devices[i].connection = connection[0] != '\0' ? strdup(connection) : NULL;
		devices[i].ifindex = ifindex;
	}
	qsort(devices, count, sizeof(NetworkDevice), compare_devices);

	pclose(fp);
	return devices;
}

int get_network_interfaces_size(void) {
	int count = -1;
	FILE *fp = popen("nmcli -t device | wc -l", "r");
	if (fp) {
		fscanf(fp, "%d", &count);
		pclose(fp);
	}
	return count;
}
