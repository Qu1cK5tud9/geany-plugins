/*
 *  scope.c
 *
 *  Copyright 2012 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "common.h"

GeanyPlugin *geany_plugin;
GeanyData *geany_data;
GeanyFunctions *geany_functions;

PLUGIN_VERSION_CHECK(215)

PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE, _("Scope Debugger"),
	_("Simple GDB front-end."), "0.76" , "Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>")

/* Keybinding(s) */
enum
{	/* Note: must match debug_menu_keys/items */
	SET_PROGRAM_KB,
	RUN_CONTINUE_KB,
	GOTO_CURSOR_KB,
	GOTO_SOURCE_KB,
	STEP_INTO_KB,
	STEP_OVER_KB,
	STEP_OUT_KB,
	TERMINATE_KB,
	BREAKPOINT_KB,
	GDB_COMMAND_KB,
#ifdef G_OS_UNIX
	SHOW_TERMINAL_KB,
#endif
	EVALUATE_KB,
	WATCH_KB,
	INSPECT_KB,
	COUNT_KB
};

PLUGIN_KEY_GROUP(scope, COUNT_KB)

static MenuKey debug_menu_keys[EVALUATE_KB] =
{
	{ "setup_program", N_("Setup program") },
	{ "run_continue",  N_("Run/continue") },
	{ "goto_cursor",   N_("Run to cursor") },
	{ "goto_source",   N_("Run to source") },
	{ "step_into",     N_("Step into") },
	{ "step_over",     N_("Step over") },
	{ "step_out",      N_("Step out") },
	{ "terminate",     N_("Terminate") },
	{ "breakpoint",    N_("Toggle breakpoint") },
#ifdef G_OS_UNIX
	{ "gdb_command",   N_("GDB command") },
	{ "show_terminal", N_("Show terminal") }
#else
	{ "gdb_command",   N_("GDB command") }
#endif
};

static void on_scope_gdb_command(G_GNUC_UNUSED const MenuItem *menu_item)
{
	view_command_line(NULL, NULL, NULL, FALSE);
}

static void on_scope_reset_markers(G_GNUC_UNUSED const MenuItem *menu_item)
{
	utils_remark(document_get_current());
}

static void on_scope_cleanup_files(G_GNUC_UNUSED const MenuItem *menu_item)
{
	guint i;

	foreach_document(i)
	{
		if (utils_attrib(documents[i], SCOPE_OPEN))
			document_close(documents[i]);
	}
}

static void on_scope_recent_item(G_GNUC_UNUSED const MenuItem *menu_item)
{
}

/* Menu item(s) */
#define DS_RUNNABLE (DS_INACTIVE | DS_DEBUG | DS_HANGING)
#define DS_JUMPABLE (DS_DEBUG | DS_EXTRA_2)
#define DS_SOURCABLE (DS_DEBUG | DS_EXTRA_3)
#define DS_STEPABLE (DS_DEBUG | DS_EXTRA_1)
#define DS_BREAKABLE (DS_NOT_BUSY | DS_EXTRA_2)
#define DS_RECENT (DS_INACTIVE | DS_EXTRA_4)

static MenuItem debug_menu_items[] =
{
	{ "program_setup",       on_program_setup,       0,            NULL, NULL },
	{ "debug_run_continue",  on_debug_run_continue,  DS_RUNNABLE,  NULL, NULL },
	{ "debug_goto_cursor",   on_debug_goto_cursor,   DS_JUMPABLE,  NULL, NULL },
	{ "debug_goto_source",   on_debug_goto_source,   DS_SOURCABLE, NULL, NULL },
	{ "debug_step_into",     on_debug_step_into,     DS_STEPABLE,  NULL, NULL },
	{ "debug_step_over",     on_debug_step_over,     DS_STEPABLE,  NULL, NULL },
	{ "debug_step_out",      on_debug_step_out,      DS_DEBUG,     NULL, NULL },
	{ "debug_terminate",     on_debug_terminate,     DS_ACTIVE,    NULL, NULL },
	{ "break_toggle",        on_break_toggle,        DS_BREAKABLE, NULL, NULL },
	{ "scope_gdb_command",   on_scope_gdb_command,   DS_ACTIVE,    NULL, NULL },
#ifdef G_OS_UNIX
	{ "terminal_show",       on_terminal_show,       0,            NULL, NULL },
#endif
	{ "scope_reset_markers", on_scope_reset_markers, 0,            NULL, NULL },
	{ "scope_cleanup_files", on_scope_cleanup_files, 0,            NULL, NULL },
	{ "scope_recent_item",   on_scope_recent_item,   DS_RECENT,    NULL, NULL },
	{ NULL, NULL, 0, NULL, NULL }
};

static guint debug_menu_extra_state(void)
{
	GeanyDocument *doc = document_get_current();

	return ((thread_state >= THREAD_AT_SOURCE) << DS_INDEX_1) |
		((doc && utils_source_document(doc)) << DS_INDEX_2) |
		((thread_state == THREAD_AT_ASSEMBLER) << DS_INDEX_3) |
		(recent_menu_items() << DS_INDEX_4);
}

static MenuInfo debug_menu_info = { debug_menu_items, debug_menu_extra_state, 0 };

static void on_scope_key(guint key_id)
{
	menu_item_execute(&debug_menu_info, debug_menu_items + key_id, FALSE);
}

typedef struct _ToolItem
{
	gint index;
	const char *icon[2];
	GtkWidget *widget;
} ToolItem;

static ToolItem toolbar_items[] =
{
	{ RUN_CONTINUE_KB, { "small_run_continue_icon", "large_run_continue_icon" }, NULL },
	{ GOTO_CURSOR_KB,  { "small_goto_cursor_icon",  "large_goto_cursor_icon"  }, NULL },
	{ GOTO_SOURCE_KB,  { "small_goto_source_icon",  "large_goto_source_icon"  }, NULL },
	{ STEP_INTO_KB,    { "small_step_into_icon",    "large_step_into_icon"    }, NULL },
	{ STEP_OVER_KB,    { "small_step_over_icon",    "large_step_over_icon"    }, NULL },
	{ STEP_OUT_KB,     { "small_step_out_icon",     "large_step_out_icon"     }, NULL },
	{ TERMINATE_KB,    { "small_terminate_icon",    "large_terminate_icon"    }, NULL },
	{ BREAKPOINT_KB,   { "small_breakpoint_icon",   "large_breakpoint_icon",  }, NULL },
	{ -1, { NULL, NULL }, NULL }
};

static void on_toolbar_button_clicked(G_GNUC_UNUSED GtkToolButton *toolbutton, gpointer gdata)
{
	menu_item_execute(&debug_menu_info, debug_menu_items + GPOINTER_TO_INT(gdata), TRUE);
}

static void on_toolbar_reconfigured(GtkToolItem *tool_item, ToolItem *item)
{
	GtkToolShell *shell = GTK_TOOL_SHELL(gtk_widget_get_parent(item->widget));
	gboolean large = gtk_tool_shell_get_icon_size(shell) > GTK_ICON_SIZE_MENU;
	gchar *tooltip = NULL;

	if (gtk_tool_shell_get_style(shell) == GTK_TOOLBAR_ICONS)
	{
		GtkMenuItem *menu_item = GTK_MENU_ITEM(debug_menu_items[item->index].widget);
		tooltip = g_strdup(gtk_menu_item_get_label(menu_item));
		utils_str_remove_chars(tooltip, "_");
	}

	gtk_tool_item_set_tooltip_text(tool_item, tooltip);
	g_free(tooltip);
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(tool_item),
		get_widget(item->icon[large]));
}

static void toolbar_update_state(DebugState state)
{
	static DebugState last_state = 0;
	state |= debug_menu_extra_state();

	if (state != last_state)
	{
		ToolItem *item;

		for (item = toolbar_items; item->index != -1; item++)
		{
			gtk_widget_set_sensitive(item->widget,
				menu_item_matches_state(debug_menu_items + item->index, state));
		}
	}
}

static GtkStatusbar *geany_statusbar;
static GtkWidget *debug_statusbar;
static GtkLabel *debug_state_label;

void statusbar_update_state(DebugState state)
{
	static DebugState last_state = DS_INACTIVE;

	if (thread_state == THREAD_AT_ASSEMBLER)
		state = DS_EXTRA_1;

	if (state != last_state)
	{
		static const char *const states[] = { N_("Busy"), N_("Ready"), N_("Debug"),
			N_("Hang"), N_("Assem"), N_("Load"), NULL };
		guint i;

		for (i = 0; states[i]; i++)
			if (state & (DS_BUSY << i))
				break;

		gtk_label_set_text(debug_state_label, _(states[i]));

		if (state == DS_INACTIVE)
		{
			gtk_widget_hide(debug_statusbar);
			gtk_statusbar_set_has_resize_grip(geany_statusbar, TRUE);
		}
		else if (last_state == DS_INACTIVE)
		{
			gtk_statusbar_set_has_resize_grip(geany_statusbar, FALSE);
			gtk_widget_show(debug_statusbar);
		}

		last_state = state;
	}
}

static guint blink_id = 0;

static gboolean plugin_unblink(G_GNUC_UNUSED gpointer gdata)
{
	gtk_widget_set_state(debug_statusbar, GTK_STATE_NORMAL);
	blink_id = 0;
	return FALSE;
}

void plugin_blink(void)
{
	if (pref_visual_beep_length)
	{
		if (blink_id)
			g_source_remove(blink_id);
		else
			gtk_widget_set_state(debug_statusbar, GTK_STATE_SELECTED);

		blink_id = plugin_timeout_add(geany_plugin, pref_visual_beep_length * 10,
			plugin_unblink, NULL);
	}
}

void plugin_beep(void)
{
	if (geany_data->prefs->beep_on_errors)
		gdk_beep();
	else
		plugin_blink();
}

void update_state(DebugState state)
{
	menu_update_state(state);
	program_update_state(state);
	toolbar_update_state(state);
	statusbar_update_state(state);
	views_update_state(state);
}

static void on_document_new(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer gdata)
{
	prefs_apply(doc);
}

static void on_document_open(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	G_GNUC_UNUSED gpointer gdata)
{
	prefs_apply(doc);
	breaks_mark(doc);

	if (debug_state() != DS_INACTIVE)
		threads_mark(doc);
}

static guint resync_id = 0;

static gboolean resync_readonly(G_GNUC_UNUSED gpointer gdata)
{
	guint i;

	foreach_document(i)
	{
		documents[i]->readonly = scintilla_send_message(documents[i]->editor->sci,
			SCI_GETREADONLY, 0, 0);
	}

	resync_id = 0;
	return FALSE;
}

static void unlock_readonly(void)
{
	guint i;

	foreach_document(i)
	{
		if (utils_attrib(documents[i], SCOPE_LOCK))
		{
			documents[i]->readonly = FALSE;

			if (!resync_id)
				resync_id = plugin_idle_add(geany_plugin, resync_readonly, NULL);
		}
	}
}

static void on_session_save(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GKeyFile *keyfile,
	G_GNUC_UNUSED gpointer gdata)
{
	unlock_readonly();
}

static gboolean on_editor_notify(G_GNUC_UNUSED GObject *obj, GeanyEditor *editor,
	SCNotification *nt, G_GNUC_UNUSED gpointer gdata)
{
	GeanyDocument *doc = editor->document;

	if (nt->nmhdr.code == SCN_MODIFIED && nt->linesAdded && utils_source_document(doc))
	{
		gboolean active = debug_state() != DS_INACTIVE;
		ScintillaObject *sci = editor->sci;
		gint start = sci_get_line_from_position(sci, nt->position);

		if (active)
			threads_delta(sci, doc->real_path, start, nt->linesAdded);

		breaks_delta(sci, doc->real_path, start, nt->linesAdded, active);
	}

	return FALSE;
}

static void on_document_filetype_set(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
	G_GNUC_UNUSED GeanyFiletype *filetype_old, G_GNUC_UNUSED gpointer gdata)
{
	utils_lock_unlock(doc, debug_state() != DS_INACTIVE && utils_source_document(doc));
}

static void on_project_open(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GKeyFile *config)
{
	program_context_changed();
}

static gboolean change_context(G_GNUC_UNUSED gpointer gdata)
{
	program_context_changed();
	return FALSE;
}

static void on_project_close(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GKeyFile *config)
{
	plugin_idle_add(geany_plugin, change_context, NULL);
}

static void on_geany_startup_complete(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED gpointer gdata)
{
	if (!geany->app->project)
		program_context_changed();
}

typedef struct _ScopeCallback  /* we don't want callbacks on builder init failure */
{
	const char *name;
	GCallback callback;
} ScopeCallback;

static const ScopeCallback scope_callbacks[] =
{
	{ "document-new",             G_CALLBACK(on_document_new) },
	{ "document-open",            G_CALLBACK(on_document_open) },
	{ "document-reload",          G_CALLBACK(on_document_open) },
	{ "save-settings",            G_CALLBACK(on_session_save) },
	{ "editor-notify",            G_CALLBACK(on_editor_notify) },
	{ "document-filetype-set",    G_CALLBACK(on_document_filetype_set) },
	{ "project-before-save",      G_CALLBACK(on_session_save) },
	{ "project-open",             G_CALLBACK(on_project_open) },
	{ "project-close",            G_CALLBACK(on_project_close) },
	{ "geany-startup-complete",   G_CALLBACK(on_geany_startup_complete) },
	{ NULL, NULL }
};

static GtkBuilder *builder = NULL;

GObject *get_object(const char *name)
{
#ifdef G_DISABLE_ASSERT
	return gtk_builder_get_object(builder, name);
#else  /* G_DISABLE_ASSERT */
	GObject *object = gtk_builder_get_object(builder, name);

	if (!object)
	{
		fprintf(stderr, "Scope: object %s is missing\n", name);
		abort();
	}

	return object;
#endif  /* G_DISABLE_ASSERT */
}

GtkWidget *get_widget(const char *name)
{
#ifdef G_DISABLE_ASSERT
	return GTK_WIDGET(get_object(name));
#else  /* !SC_DISABLE_ASSERT */
	GObject *object = get_object(name);

	if (!GTK_IS_WIDGET(object))
	{
		fprintf(stderr, "Scope: object %s is not a widget\n", name);
		abort();
	}

	return GTK_WIDGET(object);
#endif  /* G_DISABLE_ASSERT */
}

void scope_configure(void)
{
	guint item;
	ToolItem *tool_item = toolbar_items;

	for (item = 0; tool_item->index != -1; item++, tool_item++)
		gtk_widget_set_visible(tool_item->widget, pref_show_toolbar_items & (1 << item));
}

void plugin_help()
{
	char *helpfile = g_build_filename(PLUGINHTMLDOCDIR, "scope.html", NULL);
	utils_open_browser(helpfile);
	g_free(helpfile);
}

#define DEBUG_MENU_ITEM_POS 7
static GtkWidget *debug_item;
static GtkWidget *debug_panel;

void open_debug_panel(void)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(geany->main_widgets->message_window_notebook);
	msgwin_switch_tab(gtk_notebook_page_num(notebook, debug_panel), TRUE);
	gtk_widget_grab_focus(debug_panel);
}

void plugin_init(G_GNUC_UNUSED GeanyData *gdata)
{
	GError *gerror = NULL;
	GtkWidget *menubar1 = find_widget(geany->main_widgets->window, "menubar1");
	guint item;
	const MenuKey *menu_key = debug_menu_keys;
	ToolItem *tool_item = toolbar_items;
	const ScopeCallback *scb;
	char *gladefile = g_build_filename(PLUGINDATADIR, "scope.glade", NULL);

	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	builder = gtk_builder_new();
	gtk_builder_set_translation_domain(builder, GETTEXT_PACKAGE);

	if (!gtk_builder_add_from_file(builder, gladefile, &gerror))
	{
		msgwin_status_add(_("Scope: %s: %s."), gladefile, gerror->message);
		g_error_free(gerror);
		g_object_unref(builder);
		builder = NULL;
	}

	g_free(gladefile);
	if (!builder)
		return;

	/* interface */
#ifndef G_OS_UNIX
	gtk_widget_hide(get_widget("terminal_show"));
#endif
	debug_item = get_widget("debug_item");
	if (menubar1)
		gtk_menu_shell_insert(GTK_MENU_SHELL(menubar1), debug_item, DEBUG_MENU_ITEM_POS);
	else
		gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), debug_item);

	menu_connect("debug_menu", &debug_menu_info, NULL);
	ui_add_document_sensitive(get_widget("scope_reset_markers"));
	ui_add_document_sensitive(get_widget("scope_cleanup_files"));

	for (item = 0; item < EVALUATE_KB; item++, menu_key++)
	{
		keybindings_set_item(plugin_key_group, item, on_scope_key, 0, 0, menu_key->name,
			_(menu_key->label), debug_menu_items[item].widget);
	}

	geany_statusbar = GTK_STATUSBAR(gtk_widget_get_parent(geany->main_widgets->progressbar));
	debug_statusbar = get_widget("debug_statusbar");
	debug_state_label = GTK_LABEL(get_widget("debug_state_label"));
	gtk_box_pack_end(GTK_BOX(geany_statusbar), debug_statusbar, FALSE, FALSE, 0);

	debug_panel = get_widget("debug_panel");
	gtk_notebook_append_page(GTK_NOTEBOOK(geany->main_widgets->message_window_notebook),
		debug_panel, get_widget("debug_label"));

	/* startup */
	gtk216_init();
	program_init();
	prefs_init();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(debug_panel), pref_panel_tab_pos);
	conterm_init();
	inspect_init();
	parse_init();
	debug_init();
	views_init();
	thread_init();
	break_init();
	watch_init();
	stack_init();
	local_init();
	menu_init();
	menu_set_popup_keybindings(item);

	for (item = 0; tool_item->index != -1; item++, tool_item++)
	{
		GtkMenuItem *menu_item = GTK_MENU_ITEM(debug_menu_items[tool_item->index].widget);
		GtkToolItem *button = gtk_tool_button_new(NULL, gtk_menu_item_get_label(menu_item));

		gtk_tool_button_set_use_underline(GTK_TOOL_BUTTON(button),
			gtk_menu_item_get_use_underline(menu_item));
		g_signal_connect(button, "clicked", G_CALLBACK(on_toolbar_button_clicked),
			GINT_TO_POINTER(tool_item->index));
		g_signal_connect(button, "toolbar-reconfigured",
			G_CALLBACK(on_toolbar_reconfigured), tool_item);
		tool_item->widget = GTK_WIDGET(button);
		plugin_add_toolbar_item(geany_plugin, button);
	}

	toolbar_update_state(DS_INACTIVE);
	watches_update_state(DS_INACTIVE);
	scope_configure();

	g_signal_connect(debug_panel, "switch-page", G_CALLBACK(on_view_changed), NULL);
	for (scb = scope_callbacks; scb->name; scb++)
		plugin_signal_connect(geany_plugin, NULL, scb->name, FALSE, scb->callback, NULL);
}

void plugin_cleanup(void)
{
	ToolItem *item;

	if (!builder)
		return;

	gtk_widget_destroy(debug_item);
	gtk_widget_destroy(debug_statusbar);
	gtk_widget_destroy(debug_panel);

	for (item = toolbar_items; item->index != -1; item++)
		gtk_widget_destroy(item->widget);

	/* shutdown */
	tooltip_finalize();
	program_finalize();
	conterm_finalize();
	inspect_finalize();
	thread_finalize();
	break_finalize();
	utils_finalize();
	views_finalize();
	debug_finalize();
	parse_finalize();
	prefs_finalize();
	menu_finalize();

	g_object_unref(builder);
}
