/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "fcintl.h"
#include "log.h"
#include "support.h"

#include "government.h"
#include "packets.h"
#include "unittype.h"

#include "civclient.h"
#include "clinet.h"
#include "connectdlg_common.h"
#include "multiselect.h"
#include "myai.h"
#include "options.h"
#include "peptool.h"

#include "chatline.h"
#include "cityrep.h"
#include "dialogs.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "mapctrl.h"
#include "messagewin.h"
#include "optiondlg.h"
#include "pages.h"
#include "plrdlg.h"
#include "repodlgs.h"
#include "style.h"
#include "wldlg.h"

#include "finddlg_g.h"
#include "gotodlg_g.h"
#include "helpdlg_g.h"
#include "mapview_g.h"
#include "messagedlg_g.h"
#include "ratesdlg_g.h"
#include "spaceshipdlg_g.h"

#include "menu.h"

/* Use it to manage buf size for char variables */
#define LOG_MENU LOG_VERBOSE

static GtkWidget *main_menubar = NULL;
static GtkUIManager *main_uimanager = NULL;

static GtkActionGroup *action_group_game = NULL;

static GtkActionGroup *action_group_government = NULL;
static GtkActionGroup *action_group_government_type = NULL;

static GtkActionGroup *action_group_view = NULL;
static GtkActionGroup *toggle_action_group_view = NULL;

static GtkActionGroup *action_group_order = NULL;

static GtkActionGroup *action_group_delayed_goto = NULL;
static GtkActionGroup *toggle_action_group_delayed_goto_inclusive = NULL;
static GtkActionGroup *toggle_action_group_delayed_goto_exclusive = NULL;
static GtkActionGroup *toggle_action_group_delayed_goto_automatic[DELAYED_GOTO_NUM];
static GtkActionGroup *radio_action_group_delayed_goto_utype = NULL;
static GtkActionGroup *radio_action_group_delayed_goto_place = NULL;
static GtkActionGroup *radio_action_group_delayed_goto_utype_place = NULL;
static bool block_delayed_goto_utype_place = TRUE;

static GtkActionGroup *action_group_airlift = NULL;
static GtkActionGroup *radio_action_group_airlift_unit[AIRLIFT_QUEUE_NUM];

static GtkActionGroup *action_group_auto_caravan = NULL;

static GtkActionGroup *action_group_multi_selection = NULL;
static GtkActionGroup *toggle_action_group_multi_selection = NULL;
static GtkActionGroup *toggle_action_group_multi_selection_inclusive = NULL;
static GtkActionGroup *toggle_action_group_multi_selection_exclusive = NULL;
static GtkActionGroup *radio_action_group_multi_selection_utype = NULL;
static GtkActionGroup *radio_action_group_multi_selection_place = NULL;

static GtkActionGroup *action_group_miscellaneous = NULL;
static GtkActionGroup *toggle_action_group_miscellaneous = NULL;
static GtkActionGroup *radio_action_group_miscellaneous_diplomat = NULL;
static GtkActionGroup *radio_action_group_miscellaneous_caravan = NULL;
static GtkActionGroup *radio_action_group_miscellaneous_unit = NULL;

static GtkActionGroup *action_group_report = NULL;

static GtkActionGroup *action_group_help = NULL;

static enum unit_activity road_activity;

/****************************************************************
  This is the response callback for the dialog with the message:
  Leaving a local game will end it!
****************************************************************/
static void leave_local_game_response(GtkWidget* dialog, gint response)
{
  gtk_widget_destroy(dialog);
  if (response == GTK_RESPONSE_OK) {
    /* It might be killed already */
    if (aconnection.used) {
      /* It will also kill the server */
      disconnect_from_server();
    }
  }
}

/****************************************************************
...
*****************************************************************/
static void menu_set_sensitive(GtkActionGroup *paction_group,
                               const char *actionname,
                               int sensitive)
{
  GtkAction *action;

  action = gtk_action_group_get_action(paction_group, actionname);

  if (!action) {
    freelog(LOG_ERROR,
            "Can't set sensitivity for non-existent menu %s in action group",
            actionname);
    return;
  }
  gtk_action_set_sensitive(action, sensitive);
}

/****************************************************************
...
*****************************************************************/
static void menu_toggle_set_sensitive(GtkActionGroup *ptoggle_action_group,
                                      const char *actionname,
                                      int sensitive)
{
  GtkAction *action;

  action = gtk_action_group_get_action(ptoggle_action_group, actionname);

  if (!action) {
    freelog(LOG_ERROR,
            "Can't set sensitivity for non-existent menu %s in toggle"
            " action group",
            actionname);
    return;
  }

  gtk_action_set_sensitive(action, sensitive);
}

/****************************************************************
...
*****************************************************************/
static void menu_toggle_set_active(GtkActionGroup *ptoggle_action_group,
                                   const char *actionname,
                                   int active)
{
  GtkAction *action;

  action = gtk_action_group_get_action(ptoggle_action_group, actionname);

  if (!action) {
    freelog(LOG_ERROR,
            "Can't set active for non-existent menu %s in toggle action group",
            actionname);
    return;
  }
  gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), (gboolean) active);
}

/****************************************************************
...
*****************************************************************/
static void menu_radio_set_active(GtkActionGroup *pradio_action_group,
                                  const char *actionname)
{
  GtkAction *action;

  action = gtk_action_group_get_action(pradio_action_group, actionname);

  if (!action) {
    freelog(LOG_ERROR,
            "Can't set active for non-existent menu %s in radio action group",
            actionname);
    return;
  }
  gtk_action_activate(action);
}

/****************************************************************
  ...
*****************************************************************/
static void update_delayed_goto_inclusive_filter_menu(void)
{
  menu_toggle_set_active(toggle_action_group_delayed_goto_inclusive,
                         "DELAYED_GOTO_INCLUSIVE_ALL",
                         delayed_goto_inclusive_filter & FILTER_ALL);
  menu_toggle_set_active(toggle_action_group_delayed_goto_inclusive,
                         "DELAYED_GOTO_INCLUSIVE_NEW",
                         delayed_goto_inclusive_filter & FILTER_NEW);
  menu_toggle_set_active(toggle_action_group_delayed_goto_inclusive,
                         "DELAYED_GOTO_INCLUSIVE_FORTIFIED",
                         delayed_goto_inclusive_filter & FILTER_FORTIFIED);
  menu_toggle_set_active(toggle_action_group_delayed_goto_inclusive,
                         "DELAYED_GOTO_INCLUSIVE_SENTRIED",
                         delayed_goto_inclusive_filter & FILTER_SENTRIED);
  menu_toggle_set_active(toggle_action_group_delayed_goto_inclusive,
                         "DELAYED_GOTO_INCLUSIVE_VETERAN",
                         delayed_goto_inclusive_filter & FILTER_VETERAN);
  menu_toggle_set_active(toggle_action_group_delayed_goto_inclusive,
                         "DELAYED_GOTO_INCLUSIVE_AUTO",
                         delayed_goto_inclusive_filter & FILTER_AUTO);
  menu_toggle_set_active(toggle_action_group_delayed_goto_inclusive,
                         "DELAYED_GOTO_INCLUSIVE_IDLE",
                         delayed_goto_inclusive_filter & FILTER_IDLE);
  menu_toggle_set_active(toggle_action_group_delayed_goto_inclusive,
                         "DELAYED_GOTO_INCLUSIVE_ABLE_TO_MOVE",
                         delayed_goto_inclusive_filter & FILTER_ABLE_TO_MOVE);
  menu_toggle_set_active(toggle_action_group_delayed_goto_inclusive,
                         "DELAYED_GOTO_INCLUSIVE_MILITARY",
                         delayed_goto_inclusive_filter & FILTER_MILITARY);
  menu_toggle_set_active(toggle_action_group_delayed_goto_inclusive,
                         "DELAYED_GOTO_INCLUSIVE_OFF",
                         delayed_goto_inclusive_filter & FILTER_OFF);
}

/****************************************************************
  ...
*****************************************************************/
static void update_delayed_goto_exclusive_filter_menu(void)
{
  menu_toggle_set_active(toggle_action_group_delayed_goto_exclusive,
                         "DELAYED_GOTO_EXCLUSIVE_ALL",
                         delayed_goto_exclusive_filter & FILTER_ALL);
  menu_toggle_set_active(toggle_action_group_delayed_goto_exclusive,
                         "DELAYED_GOTO_EXCLUSIVE_NEW",
                         delayed_goto_exclusive_filter & FILTER_NEW);
  menu_toggle_set_active(toggle_action_group_delayed_goto_exclusive,
                         "DELAYED_GOTO_EXCLUSIVE_FORTIFIED",
                         delayed_goto_exclusive_filter & FILTER_FORTIFIED);
  menu_toggle_set_active(toggle_action_group_delayed_goto_exclusive,
                         "DELAYED_GOTO_EXCLUSIVE_SENTRIED",
                         delayed_goto_exclusive_filter & FILTER_SENTRIED);
  menu_toggle_set_active(toggle_action_group_delayed_goto_exclusive,
                         "DELAYED_GOTO_EXCLUSIVE_VETERAN",
                         delayed_goto_exclusive_filter & FILTER_VETERAN);
  menu_toggle_set_active(toggle_action_group_delayed_goto_exclusive,
                         "DELAYED_GOTO_EXCLUSIVE_AUTO",
                         delayed_goto_exclusive_filter & FILTER_AUTO);
  menu_toggle_set_active(toggle_action_group_delayed_goto_exclusive,
                         "DELAYED_GOTO_EXCLUSIVE_IDLE",
                         delayed_goto_exclusive_filter & FILTER_IDLE);
  menu_toggle_set_active(toggle_action_group_delayed_goto_exclusive,
                         "DELAYED_GOTO_EXCLUSIVE_ABLE_TO_MOVE",
                         delayed_goto_exclusive_filter & FILTER_ABLE_TO_MOVE);
  menu_toggle_set_active(toggle_action_group_delayed_goto_exclusive,
                         "DELAYED_GOTO_EXCLUSIVE_MILITARY",
                         delayed_goto_exclusive_filter & FILTER_MILITARY);
  menu_toggle_set_active(toggle_action_group_delayed_goto_exclusive,
                         "DELAYED_GOTO_EXCLUSIVE_OFF",
                         delayed_goto_exclusive_filter & FILTER_OFF);
}

/****************************************************************
  ...
*****************************************************************/
static void update_multi_selection_inclusive_filter_menu(void)
{
  menu_toggle_set_active(toggle_action_group_multi_selection_inclusive,
                         "MULTI_SELECTION_INCLUSIVE_ALL",
                         multi_select_inclusive_filter & FILTER_ALL);
  menu_toggle_set_active(toggle_action_group_multi_selection_inclusive,
                         "MULTI_SELECTION_INCLUSIVE_NEW",
                         multi_select_inclusive_filter & FILTER_NEW);
  menu_toggle_set_active(toggle_action_group_multi_selection_inclusive,
                         "MULTI_SELECTION_INCLUSIVE_FORTIFIED",
                         multi_select_inclusive_filter & FILTER_FORTIFIED);
  menu_toggle_set_active(toggle_action_group_multi_selection_inclusive,
                         "MULTI_SELECTION_INCLUSIVE_SENTRIED",
                         multi_select_inclusive_filter & FILTER_SENTRIED);
  menu_toggle_set_active(toggle_action_group_multi_selection_inclusive,
                         "MULTI_SELECTION_INCLUSIVE_VETERAN",
                         multi_select_inclusive_filter & FILTER_VETERAN);
  menu_toggle_set_active(toggle_action_group_multi_selection_inclusive,
                         "MULTI_SELECTION_INCLUSIVE_AUTO",
                         multi_select_inclusive_filter & FILTER_AUTO);
  menu_toggle_set_active(toggle_action_group_multi_selection_inclusive,
                         "MULTI_SELECTION_INCLUSIVE_IDLE",
                         multi_select_inclusive_filter & FILTER_IDLE);
  menu_toggle_set_active(toggle_action_group_multi_selection_inclusive,
                         "MULTI_SELECTION_INCLUSIVE_ABLE_TO_MOVE",
                         multi_select_inclusive_filter & FILTER_ABLE_TO_MOVE);
  menu_toggle_set_active(toggle_action_group_multi_selection_inclusive,
                         "MULTI_SELECTION_INCLUSIVE_MILITARY",
                         multi_select_inclusive_filter & FILTER_MILITARY);
  menu_toggle_set_active(toggle_action_group_multi_selection_inclusive,
                         "MULTI_SELECTION_INCLUSIVE_OFF",
                         multi_select_inclusive_filter & FILTER_OFF);
}

/****************************************************************
  ...
*****************************************************************/
static void update_multi_selection_exclusive_filter_menu(void)
{
  menu_toggle_set_active(toggle_action_group_multi_selection_exclusive,
                         "MULTI_SELECTION_EXCLUSIVE_ALL",
                         multi_select_exclusive_filter & FILTER_ALL);
  menu_toggle_set_active(toggle_action_group_multi_selection_exclusive,
                         "MULTI_SELECTION_EXCLUSIVE_NEW",
                         multi_select_exclusive_filter & FILTER_NEW);
  menu_toggle_set_active(toggle_action_group_multi_selection_exclusive,
                         "MULTI_SELECTION_EXCLUSIVE_FORTIFIED",
                         multi_select_exclusive_filter & FILTER_FORTIFIED);
  menu_toggle_set_active(toggle_action_group_multi_selection_exclusive,
                         "MULTI_SELECTION_EXCLUSIVE_SENTRIED",
                         multi_select_exclusive_filter & FILTER_SENTRIED);
  menu_toggle_set_active(toggle_action_group_multi_selection_exclusive,
                         "MULTI_SELECTION_EXCLUSIVE_VETERAN",
                         multi_select_exclusive_filter & FILTER_VETERAN);
  menu_toggle_set_active(toggle_action_group_multi_selection_exclusive,
                         "MULTI_SELECTION_EXCLUSIVE_AUTO",
                         multi_select_exclusive_filter & FILTER_AUTO);
  menu_toggle_set_active(toggle_action_group_multi_selection_exclusive,
                         "MULTI_SELECTION_EXCLUSIVE_IDLE",
                         multi_select_exclusive_filter & FILTER_IDLE);
  menu_toggle_set_active(toggle_action_group_multi_selection_exclusive,
                         "MULTI_SELECTION_EXCLUSIVE_ABLE_TO_MOVE",
                         multi_select_exclusive_filter & FILTER_ABLE_TO_MOVE);
  menu_toggle_set_active(toggle_action_group_multi_selection_exclusive,
                         "MULTI_SELECTION_EXCLUSIVE_MILITARY",
                         multi_select_exclusive_filter & FILTER_MILITARY);
  menu_toggle_set_active(toggle_action_group_multi_selection_exclusive,
                         "MULTI_SELECTION_EXCLUSIVE_OFF",
                         delayed_goto_exclusive_filter & FILTER_OFF);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_options(GtkAction *action, gpointer user_data)
{
  popup_option_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_msg_options(GtkAction *action, gpointer user_data)
{
  popup_messageopt_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_chatline_colors(GtkAction *action, gpointer user_data)
{
  popup_chatline_config_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_style(GtkAction *action, gpointer user_data)
{
  popup_style_config_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_save_settings(GtkAction *action, gpointer user_data)
{
  save_options();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_pepsettings(GtkAction *action, gpointer user_data)
{
  create_pepsetting_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_pepsettings_load(GtkAction *action,
                                           gpointer user_data)
{
  load_all_settings();
  init_menus();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_pepsettings_save(GtkAction *action,
                                           gpointer user_data)
{
  save_all_settings();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_server_options1(GtkAction *action, 
                                          gpointer user_data)
{
  send_report_request(REPORT_SERVER_OPTIONS1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_server_options2(GtkAction *action, 
                                          gpointer user_data)
{
  send_report_request(REPORT_SERVER_OPTIONS2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_save_game(GtkAction *action, gpointer user_data)
{
  popup_save_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_save_quick(GtkAction *action, gpointer user_data)
{
  send_save_game(NULL);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_output_log(GtkAction *action, gpointer user_data)
{
  log_output_window();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_clear_output(GtkAction *action, gpointer user_data)
{
  clear_output_window();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_leave(GtkAction *action, gpointer user_data)
{
  if (is_server_running()) {
    GtkWidget* dialog =
      gtk_message_dialog_new(NULL,
                             0,
                             GTK_MESSAGE_WARNING,
                             GTK_BUTTONS_OK_CANCEL,
                             _("Leaving a local game will end it!"));
    setup_dialog(dialog, toplevel);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
    g_signal_connect(dialog, "response",
                     G_CALLBACK(leave_local_game_response), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
  } else {
    disconnect_from_server();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_game_quit(GtkAction *action, gpointer user_data)
{
  popup_quit_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static const char *load_menu_game(void)
{
  static char buf[1024];

  GtkActionEntry entries_game[] = {
    {"GAME", NULL, N_("_Game"), NULL, NULL, NULL},
    {"GAME_OPTIONS", GTK_STOCK_PREFERENCES, N_("Local _Options"),
     NULL, N_("Local _Options"), G_CALLBACK(callback_game_options)},
    {"GAME_MSG_OPTIONS", GTK_STOCK_PREFERENCES, N_("_Message Options"),
     NULL, N_("_Message Options"), G_CALLBACK(callback_game_msg_options)},
    {"GAME_CHATLINE_COLORS", GTK_STOCK_SELECT_COLOR, N_("_Chat Colors"),
     NULL, N_("_Chat Colors"), G_CALLBACK(callback_game_chatline_colors)},
    {"GAME_STYLE", GTK_STOCK_SELECT_FONT, N_("_Font Size"),
     NULL, N_("_Font Size"), G_CALLBACK(callback_game_style)},
    {"GAME_SAVE_SETTINGS", GTK_STOCK_SAVE_AS, N_("Sa_ve Settings"),
     NULL, N_("Sa_ve Settings"), G_CALLBACK(callback_game_save_settings)},
    {"GAME_PEPSETTINGS", GTK_STOCK_PREFERENCES, N_("PepClient _Options"),
     NULL, N_("PepClient _Options"), G_CALLBACK(callback_game_pepsettings)},
    {"GAME_PEPSETTINGS_LOAD", GTK_STOCK_REDO, N_("Lo_ad PepClient Settings"),
     NULL, N_("Lo_ad PepClient Settings"),
     G_CALLBACK(callback_game_pepsettings_load)},
    {"GAME_PEPSETTINGS_SAVE", GTK_STOCK_SAVE_AS, N_("Sa_ve PepClient Settings"),
     NULL, N_("Sa_ve PepClient Settings"),
     G_CALLBACK(callback_game_pepsettings_save)},
    {"GAME_SERVER_OPTIONS1", GTK_STOCK_PREFERENCES,
     N_("_Initial Server Options"),
     NULL, N_("_Initial Server Options"),
     G_CALLBACK(callback_game_server_options1)},
    {"GAME_SERVER_OPTIONS2", GTK_STOCK_PREFERENCES, N_("Server Optio_ns"),
     NULL, N_("Server Optio_ns"), G_CALLBACK(callback_game_server_options2)},
    {"GAME_SAVE_GAME", GTK_STOCK_SAVE_AS, N_("_Save Game _As..."),
     NULL, N_("_Save Game _As..."), G_CALLBACK(callback_game_save_game)},
    {"GAME_SAVE_QUICK", GTK_STOCK_SAVE, N_("Save Game"),
     NULL, N_("Save Game"), G_CALLBACK(callback_game_save_quick)},
    {"GAME_OUTPUT_LOG", GTK_STOCK_FILE, N_("E_xport Log"),
     NULL, N_("E_xport Log"), G_CALLBACK(callback_game_output_log)},
    {"GAME_CLEAR_OUTPUT", GTK_STOCK_CLEAR, N_("Clear _Log"),
     NULL, N_("Clear _Log"), G_CALLBACK(callback_game_clear_output)},
    {"GAME_LEAVE", GTK_STOCK_DISCONNECT, N_("L_eave"),
     NULL, N_("L_eave"), G_CALLBACK(callback_game_leave)},
    {"GAME_QUIT", GTK_STOCK_QUIT, N_("_Quit"),
     NULL, N_("_Quit"), G_CALLBACK(callback_game_quit)}
  };

  action_group_game = gtk_action_group_new("GroupGame");
  gtk_action_group_set_translation_domain(action_group_game,
                                          textdomain(PACKAGE));
  gtk_action_group_add_actions(action_group_game, entries_game,
                               G_N_ELEMENTS(entries_game),
                               NULL);
  gtk_ui_manager_insert_action_group(main_uimanager, action_group_game, 0);

  my_snprintf(buf, sizeof(buf),
              "<menu action=\"GAME\">\n"
              "<menuitem action=\"GAME_OPTIONS\" />\n"
              "<menuitem action=\"GAME_MSG_OPTIONS\" />\n"
              "<menuitem action=\"GAME_CHATLINE_COLORS\" />\n"
              "<menuitem action=\"GAME_STYLE\" />\n"
              "<menuitem action=\"GAME_SAVE_SETTINGS\" />\n"
              "<separator/>\n"
              "<menuitem action=\"GAME_PEPSETTINGS\" />\n"
              "<menuitem action=\"GAME_PEPSETTINGS_LOAD\" />\n"
              "<menuitem action=\"GAME_PEPSETTINGS_SAVE\" />\n"
              "<separator/>\n"
              "<menuitem action=\"GAME_SERVER_OPTIONS1\" />\n"
              "<menuitem action=\"GAME_SERVER_OPTIONS2\" />\n"
              "<separator/>\n"
              "<menuitem action=\"GAME_SAVE_GAME\" />\n"
              "<menuitem action=\"GAME_SAVE_QUICK\" />\n"
              "<separator/>\n"
              "<menuitem action=\"GAME_OUTPUT_LOG\" />\n"
              "<menuitem action=\"GAME_CLEAR_OUTPUT\" />\n"
              "<separator/>\n"
              "<menuitem action=\"GAME_LEAVE\" />\n"
              "<menuitem action=\"GAME_QUIT\" />\n"
              "</menu>\n");

  freelog(LOG_MENU, "Strlen of buf = %d; Size of load_menu_game %d",
          strlen(buf), sizeof(buf));

  return buf;
}

/****************************************************************
  ...
*****************************************************************/
static void callback_government_tax_rate(GtkAction *action,
                                         gpointer user_data)
{
  popup_rates_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_government_find_city(GtkAction *action,
                                          gpointer user_data)
{
  popup_find_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_government_worklists(GtkAction *action,
                                          gpointer user_data)
{
  struct unit *punit = get_unit_in_focus();

  if (punit && unit_flag(punit, F_SETTLERS)) {
    key_unit_connect(ACTIVITY_RAILROAD);
  } else {
    popup_worklists_report();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_government_clear_selected_worklists(GtkAction *action,
                                                         gpointer user_data)
{
  clear_worklists_in_selected_cities();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_government_revolution(GtkAction *action,
                                           gpointer user_data)
{
  int i = GPOINTER_TO_INT(user_data);

  popup_revolution_dialog(i);
}

/****************************************************************
  ...
*****************************************************************/
static const char *load_menu_government_type(const char *actionname)
{
  static char retbuf[1024];
  char buf[256];
  GtkActionEntry entries_government_type;
  /* warning need max government set to 10*/
  char name[10][256];
  char label[10][256];
  char stock[10][256];
  GdkPixbuf *pixbuf;
  GdkPixbuf *pixbuf2;  
  GdkColormap *colormap;
  GdkScreen *screen;
  GtkIconSet* icon;
  GtkIconFactory *icon_factory;
  
  screen = gdk_screen_get_default();
  colormap = gdk_screen_get_default_colormap(screen);

  action_group_government_type = gtk_action_group_new("GroupGovernmentType");
  gtk_action_group_set_translation_domain(action_group_government_type,
                                          textdomain(PACKAGE));

  my_snprintf(buf, sizeof(buf), "%s_-1", actionname);

  my_snprintf(retbuf, sizeof(retbuf),
              "<menuitem action=\"%s\" />\n<separator/>\n", buf);

  sz_strlcpy(name[0], buf);
  entries_government_type.name = name[0];
  entries_government_type.stock_id = NULL;
  entries_government_type.label = N_("_Revolution...");
  entries_government_type.accelerator = NULL;
  entries_government_type.tooltip = N_("_Revolution...");
  entries_government_type.callback =
    G_CALLBACK(callback_government_revolution);

  gtk_action_group_add_actions(action_group_government_type,
                               &entries_government_type,
                               1,
                               GINT_TO_POINTER(-1));

  government_iterate(g) {
    if (g->index != game.ruleset_control.government_when_anarchy) {
      struct Sprite *gsprite;

      assert ((g->index+1) < 10);
      
      icon_factory = gtk_icon_factory_new();
      
      my_snprintf(buf, sizeof(buf), "%s_%d", actionname, g->index);
      sz_strlcpy(name[g->index+1], buf);
      cat_snprintf(retbuf, sizeof(retbuf),
                   "<menuitem action=\"%s\" />\n", buf);
      
      my_snprintf(buf, sizeof(buf), _("%s..."), g->name);
      sz_strlcpy(label[g->index+1], buf);
      
      entries_government_type.name = name[g->index+1];
      entries_government_type.label = label[g->index+1];
      
      if ((gsprite = g->sprite)) {
        my_snprintf(buf, sizeof(buf), "STOCK_GOUV_%d", (g->index+1));
        sz_strlcpy(stock[g->index+1], buf);
        
        pixbuf = NULL;
        pixbuf = gdk_pixbuf_get_from_drawable(pixbuf,
                                              gsprite->pixmap,
                                              colormap,
                                              0, 0, 0, 0,
                                              gsprite->width,
                                              gsprite->height);

        pixbuf2 = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 
                                 gsprite->height, gsprite->height);

        gdk_pixbuf_fill(pixbuf2, 0xffffff00);

        gdk_pixbuf_composite(pixbuf, pixbuf2, 0, 0, 
                             gsprite->width, gsprite->height, 0, 0, 1, 1,
                             GDK_INTERP_HYPER, 255);
        
        freelog(LOG_MENU, "pixbuf= %p index=%d", pixbuf, g->index);
        
        icon = gtk_icon_set_new_from_pixbuf(pixbuf2);
        gtk_icon_factory_add(icon_factory, stock[g->index+1], icon);

        g_object_unref(pixbuf2);
        g_object_unref(pixbuf);
        
        entries_government_type.stock_id = stock[g->index+1];
      } else {
        entries_government_type.stock_id = NULL;
      }
      
      entries_government_type.accelerator = NULL;
      entries_government_type.tooltip = label[g->index+1];
      entries_government_type.callback =
        G_CALLBACK(callback_government_revolution);

      gtk_action_group_set_translation_domain(action_group_government_type,
                                              textdomain(PACKAGE));

      gtk_action_group_add_actions(action_group_government_type,
                                   &entries_government_type,
                                   1,
                                   GINT_TO_POINTER(g->index));
      gtk_icon_factory_add_default(icon_factory);
    }
  } government_iterate_end;


  gtk_ui_manager_insert_action_group(main_uimanager,
                                     action_group_government_type,
                                     0);

  freelog(LOG_MENU, "Strlen of buf = %d; Size of load_menu_government_type %d",
          strlen(retbuf), sizeof(retbuf));

  return retbuf;

}

/****************************************************************
  ...
*****************************************************************/
static const char *load_menu_government(void)
{
  static char buf[1024];

  GtkActionEntry entries_government[] = {
    {"GOVERNMENT", NULL, N_("Gov_ernment"), NULL, NULL, NULL},
    {"GOVERNMENT_TAX_RATE", NULL, N_("_Tax Rates"),
     "<Shift>t", N_("_Tax Rates"), G_CALLBACK(callback_government_tax_rate)},
    {"GOVERNMENT_FIND_CITY", NULL, N_("_Find City"),
     "<Shift>f", N_("_Find City"), G_CALLBACK(callback_government_find_city)},
    {"GOVERNMENT_WORKLISTS", NULL, N_("_Worklists"),
     "<Control>l", N_("_Worklists"), G_CALLBACK(callback_government_worklists)},
    {"GOVERNMENT_CLEAR_SELECTED_WORKLISTS", NULL,
     N_("_Clear Selected Worklists"),
     "<Control>w", N_("_Clear Selected Worklists"),
     G_CALLBACK(callback_government_clear_selected_worklists)},
    {"GOVERNMENT_CHANGE", NULL, N_("_Change Government"), NULL, NULL, NULL},
  };

  action_group_government = gtk_action_group_new("GroupGovernment");
  gtk_action_group_set_translation_domain(action_group_government,
                                          textdomain(PACKAGE));

  gtk_action_group_add_actions(action_group_government, entries_government,
                               G_N_ELEMENTS(entries_government),
                               NULL);
  gtk_ui_manager_insert_action_group(main_uimanager, 
                                     action_group_government, 0);

  my_snprintf(buf, sizeof(buf),
              "<menu action=\"GOVERNMENT\">\n"
              "<menuitem action=\"GOVERNMENT_TAX_RATE\" />\n"
              "<separator/>\n"
              "<menuitem action=\"GOVERNMENT_FIND_CITY\" />\n"
              "<menuitem action=\"GOVERNMENT_WORKLISTS\" />\n"
              "<menuitem action=\"GOVERNMENT_CLEAR_SELECTED_WORKLISTS\" />\n"
              "<separator/>\n"
              "<menu action=\"GOVERNMENT_CHANGE\">\n"
              "%s"
              "</menu>\n"
              "</menu>\n",
              load_menu_government_type("GOVERNMENT_TYPE"));

  freelog(LOG_MENU, "Strlen of buf = %d; Size of load_menu_government %d",
          strlen(buf), sizeof(buf));

  return buf;
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_center_view(GtkAction *action, gpointer user_data)
{
  center_on_unit();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_map_grid(GtkToggleAction *action,
                                        gpointer user_data)
{
  if (draw_map_grid ^ gtk_toggle_action_get_active(action)) {
    key_map_grid_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_national_borders(GtkToggleAction *action,
                                                gpointer user_data)
{
  if (draw_borders ^ gtk_toggle_action_get_active(action)) {
    key_map_borders_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_city_names(GtkToggleAction *action,
                                          gpointer user_data)
{
  if (!tiles_hilited_cities) {
    if (draw_city_names ^ gtk_toggle_action_get_active(action)) {
      key_city_names_toggle();
      menu_toggle_set_sensitive(toggle_action_group_view,
                                "VIEW_SHOW_CITY_GROWTH_TURNS",
                                draw_city_names);
      menu_toggle_set_sensitive(toggle_action_group_view,
                                "VIEW_SHOW_CITY_TRADEROUTES",
                                draw_city_names);
    }
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_city_growth_turns(GtkToggleAction *action,
                                                 gpointer user_data)
{
  if (draw_city_growth ^ gtk_toggle_action_get_active(action)) {
    key_city_growth_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_city_productions(GtkToggleAction *action,
                                                gpointer user_data)
{
  if (draw_city_productions ^ gtk_toggle_action_get_active(action)) {
    key_city_productions_toggle();
    menu_toggle_set_sensitive(toggle_action_group_view,
                              "VIEW_SHOW_CITY_PRODUCTION_BUY_COST",
                              draw_city_productions);
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_city_production_buy_cost(GtkToggleAction *action,
                                                        gpointer user_data)
{
  if (draw_city_production_buy_cost ^ gtk_toggle_action_get_active(action)) {
    key_city_production_buy_cost_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_city_traderoutes(GtkToggleAction *action,
                                                gpointer user_data)
{
  if (draw_city_traderoutes ^ gtk_toggle_action_get_active(action)) {
    key_city_traderoutes_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_terrain(GtkToggleAction *action,
                                       gpointer user_data)
{
  if (draw_terrain ^ gtk_toggle_action_get_active(action)) {
    key_terrain_toggle();
    menu_toggle_set_sensitive(toggle_action_group_view,
                              "VIEW_SHOW_COASTLINE",
                              !draw_terrain);
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_coastline(GtkToggleAction *action,
                                         gpointer user_data)
{
  if (draw_coastline ^ gtk_toggle_action_get_active(action)) {
    key_coastline_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_roads_rails(GtkToggleAction *action,
                                           gpointer user_data)
{
  if (draw_roads_rails ^ gtk_toggle_action_get_active(action)) {
    key_roads_rails_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_irrigation(GtkToggleAction *action,
                                          gpointer user_data)
{
  if (draw_irrigation ^ gtk_toggle_action_get_active(action)) {
    key_irrigation_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_mines(GtkToggleAction *action,
                                     gpointer user_data)
{
  if (draw_mines ^ gtk_toggle_action_get_active(action)) {
    key_mines_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_fortress_airbase(GtkToggleAction *action,
                                                gpointer user_data)
{
  if (draw_fortress_airbase ^ gtk_toggle_action_get_active(action)) {
    key_fortress_airbase_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_specials(GtkToggleAction *action,
                                        gpointer user_data)
{
  if (draw_specials ^ gtk_toggle_action_get_active(action)) {
    key_specials_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_pollution(GtkToggleAction *action,
                                         gpointer user_data)
{
  if (draw_pollution ^ gtk_toggle_action_get_active(action)) {
    key_pollution_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_cities(GtkToggleAction *action,
                                      gpointer user_data)
{
  if (draw_cities ^ gtk_toggle_action_get_active(action)) {
    key_cities_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_units(GtkToggleAction *action,
                                     gpointer user_data)
{
  if (draw_units ^ gtk_toggle_action_get_active(action)) {
    key_units_toggle();
    menu_toggle_set_sensitive(toggle_action_group_view,
                              "VIEW_SHOW_FOCUS_UNIT",
                              !draw_units);
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_focus_unit(GtkToggleAction *action,
                                          gpointer user_data)
{
  if (draw_focus_unit ^ gtk_toggle_action_get_active(action)) {
    key_focus_unit_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_view_show_fog_of_war(GtkToggleAction *action,
                                          gpointer user_data)
{
  if (draw_fog_of_war ^ gtk_toggle_action_get_active(action)) {
    key_fog_of_war_toggle();
  }
}

/****************************************************************
  ...
*****************************************************************/
static const char *load_menu_view(void)
{
  static char buf[2048];

  GtkActionEntry entries_view[] = {
    {"VIEW", NULL, N_("_View"), NULL, NULL, NULL},
    {"VIEW_IMPROVEMENTS", NULL, N_("_Improvements"), NULL, NULL, NULL},
    {"VIEW_CENTER_VIEW", NULL, N_("Center _View"),
     "c", N_("Center _View"), G_CALLBACK(callback_view_center_view)}
  };

  GtkToggleActionEntry toggle_entries_view[] = {
    {"VIEW_SHOW_MAP_GRID", NULL, N_("Map _Grid"),
     "<Control>g", N_("Map _Grid"), G_CALLBACK(callback_view_show_map_grid), TRUE},
    {"VIEW_SHOW_NATIONAL_BORDERS", NULL, N_("National _Borders"),
     NULL, N_("National _Borders"), 
     G_CALLBACK(callback_view_show_national_borders), TRUE},
    {"VIEW_SHOW_CITY_NAMES", NULL, N_("City _Names"),
     "<Control>n", N_("City _Names"),
     G_CALLBACK(callback_view_show_city_names), TRUE},
    {"VIEW_SHOW_CITY_GROWTH_TURNS", NULL, N_("City G_rowth"),
     NULL, N_("City G_rowth"),
     G_CALLBACK(callback_view_show_city_growth_turns), TRUE},
    {"VIEW_SHOW_CITY_PRODUCTIONS", NULL, N_("City _Productions"),
     "<Control>p", N_("City _Productions"),
     G_CALLBACK(callback_view_show_city_productions), TRUE},
    {"VIEW_SHOW_CITY_PRODUCTION_BUY_COST", NULL,
     N_("City Production Buy _Cost"),
     NULL, N_("City Production Buy _Cost"),
     G_CALLBACK(callback_view_show_city_production_buy_cost), TRUE},
    {"VIEW_SHOW_CITY_TRADEROUTES", NULL, N_("City _Trade Routes"),
     "<Control>t", N_("City _Trade Routes"),
     G_CALLBACK(callback_view_show_city_traderoutes), TRUE},
    {"VIEW_SHOW_TERRAIN", NULL, N_("Te_rrain"),
     NULL, N_("Te_rrain"), G_CALLBACK(callback_view_show_terrain), TRUE},
    {"VIEW_SHOW_COASTLINE", NULL, N_("C_oastline"),
     NULL, N_("C_oastline"), G_CALLBACK(callback_view_show_coastline), TRUE},
    {"VIEW_SHOW_ROADS_RAILS", NULL, N_("_Roads & Rails"),
     NULL, N_("_Roads & Rails"), 
     G_CALLBACK(callback_view_show_roads_rails), TRUE},
    {"VIEW_SHOW_IRRIGATION", NULL, N_("_Irrigation"),
     NULL, N_("_Irrigation"), G_CALLBACK(callback_view_show_irrigation), TRUE},
    {"VIEW_SHOW_MINES", NULL, N_("_Mines"),
     NULL, N_("_Mines"), G_CALLBACK(callback_view_show_mines), TRUE},
    {"VIEW_SHOW_FORTRESS_AIRBASE", NULL, N_("_Fortress & Airbase"),
     NULL, N_("_Fortress & Airbase"),
     G_CALLBACK(callback_view_show_fortress_airbase), TRUE},
    {"VIEW_SHOW_SPECIALS", NULL, N_("_Specials"), NULL,
     N_("_Specials"), G_CALLBACK(callback_view_show_specials), TRUE},
    {"VIEW_SHOW_POLLUTION", NULL, N_("Poll_ution & Fallout"),
     NULL, N_("Poll_ution & Fallout"),
     G_CALLBACK(callback_view_show_pollution), TRUE},
    {"VIEW_SHOW_CITIES", NULL, N_("C_ities"),
     NULL, N_("C_ities"), G_CALLBACK(callback_view_show_cities), TRUE},
    {"VIEW_SHOW_UNITS", NULL, N_("_Units"),
     NULL, N_("_Units"), G_CALLBACK(callback_view_show_units), TRUE},
    {"VIEW_SHOW_FOCUS_UNIT", NULL, N_("_Focus Unit"),
     NULL, N_("_Focus Unit"), G_CALLBACK(callback_view_show_focus_unit), TRUE},
    {"VIEW_SHOW_FOG_OF_WAR", NULL, N_("Fog of _War"),
     NULL, N_("Fog of _War"), G_CALLBACK(callback_view_show_fog_of_war), TRUE}
  };

  action_group_view = gtk_action_group_new("GroupView");
  gtk_action_group_set_translation_domain(action_group_view,
                                          textdomain(PACKAGE));
  gtk_action_group_add_actions(action_group_view, entries_view,
                               G_N_ELEMENTS(entries_view),
                               NULL);
  gtk_ui_manager_insert_action_group(main_uimanager, action_group_view, 0);

  toggle_action_group_view = gtk_action_group_new("ToggleGroupView");
  gtk_action_group_set_translation_domain(toggle_action_group_view,
                                          textdomain(PACKAGE));

  gtk_action_group_add_toggle_actions(toggle_action_group_view,
                                      toggle_entries_view,
                                      G_N_ELEMENTS(toggle_entries_view),
                                      NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     toggle_action_group_view, 0);

  my_snprintf(buf, sizeof(buf), "%s",
              "<menu action=\"VIEW\">\n"
              "<menuitem action=\"VIEW_SHOW_MAP_GRID\" />\n"
              "<menuitem action=\"VIEW_SHOW_NATIONAL_BORDERS\" />\n"
              "<menuitem action=\"VIEW_SHOW_CITY_NAMES\" />\n"
              "<menuitem action=\"VIEW_SHOW_CITY_GROWTH_TURNS\" />\n"
              "<menuitem action=\"VIEW_SHOW_CITY_PRODUCTIONS\" />\n"
              "<menuitem action=\"VIEW_SHOW_CITY_PRODUCTION_BUY_COST\" />\n"
              "<menuitem action=\"VIEW_SHOW_CITY_TRADEROUTES\" />\n"
              "<separator/>\n"
              "<menuitem action=\"VIEW_SHOW_TERRAIN\" />\n"
              "<menuitem action=\"VIEW_SHOW_COASTLINE\" />\n"
              "<menu action=\"VIEW_IMPROVEMENTS\">\n"
              " <menuitem action=\"VIEW_SHOW_ROADS_RAILS\" />\n"
              " <menuitem action=\"VIEW_SHOW_IRRIGATION\" />\n"
              " <menuitem action=\"VIEW_SHOW_MINES\" />\n"
              " <menuitem action=\"VIEW_SHOW_FORTRESS_AIRBASE\" />\n"
              "</menu>\n"
              "<menuitem action=\"VIEW_SHOW_SPECIALS\" />\n"
              "<menuitem action=\"VIEW_SHOW_POLLUTION\" />\n"
              "<menuitem action=\"VIEW_SHOW_CITIES\" />\n"
              "<menuitem action=\"VIEW_SHOW_UNITS\" />\n"
              "<menuitem action=\"VIEW_SHOW_FOCUS_UNIT\" />\n"
              "<menuitem action=\"VIEW_SHOW_FOG_OF_WAR\" />\n"
              "<separator/>\n"
              "<menuitem action=\"VIEW_CENTER_VIEW\" />\n"
              "</menu>\n");

  freelog(LOG_MENU, "Strlen of buf = %d; Size of load_menu_view %d",
          strlen(buf), sizeof(buf));

  return buf;
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_build_city(GtkAction *action, gpointer user_data)
{
  /* Also used to build wonder and add to city. */
  key_unit_build();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_road(GtkAction *action, gpointer user_data)
{
  if (get_unit_in_focus()) {
    if (unit_can_est_traderoute_here(get_unit_in_focus())) {
      key_unit_traderoute();
    } else {
      key_unit_road();
    }
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_irrigate(GtkAction *action, gpointer user_data)
{
  key_unit_irrigate();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_mine(GtkAction *action, gpointer user_data)
{
  key_unit_mine();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_transform(GtkAction *action, gpointer user_data)
{
  key_unit_transform();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_fortress(GtkAction *action, gpointer user_data)
{
  if (get_unit_in_focus()) {
    if (can_unit_do_activity(get_unit_in_focus(), ACTIVITY_FORTRESS)) {
      key_unit_fortress();
    } else {
      key_unit_fortify();
    }
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_airbase(GtkAction *action, gpointer user_data)
{
  if (get_unit_in_focus()) {
    if (can_unit_do_activity(get_unit_in_focus(), ACTIVITY_AIRBASE)) {
      key_unit_airbase();
    } else {
      key_unit_air_patrol();
    }
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_pollution(GtkAction *action, gpointer user_data)
{
  if (get_unit_in_focus()) {
    if (can_unit_paradrop(get_unit_in_focus())) {
      key_unit_paradrop();
    } else {
      key_unit_pollution();
    }
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_fallout(GtkAction *action, gpointer user_data)
{
  key_unit_fallout();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_sleep(GtkAction *action, gpointer user_data)
{
  key_unit_sleep();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_sentry(GtkAction *action, gpointer user_data)
{
  key_unit_sentry();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_pillage(GtkAction *action, gpointer user_data)
{
  key_unit_pillage();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_homecity(GtkAction *action, gpointer user_data)
{
  key_unit_homecity();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_load(GtkAction *action, gpointer user_data)
{
  key_unit_load();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_unload_transporter(GtkAction *action,
                                              gpointer user_data)
{
  struct unit *punit = get_unit_in_focus();

  if (punit && get_transporter_occupancy(punit) > 0) {
    key_unit_unload_all();
  } else {
    airlift_queue_clear(0);
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_unload(GtkAction *action, gpointer user_data)
{
  key_unit_unload();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_wakeup_others(GtkAction *action, gpointer user_data)
{
  key_unit_wakeup_others();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_auto_settler(GtkAction *action, gpointer user_data)
{
  if(get_unit_in_focus()) {
    enable_auto_mode();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_auto_explore(GtkAction *action, gpointer user_data)
{
  key_unit_auto_explore();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_connect_road(GtkAction *action, gpointer user_data)
{
  key_unit_connect(ACTIVITY_ROAD);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_connect_rail(GtkAction *action, gpointer user_data)
{
  struct unit *punit = get_unit_in_focus();

  if (punit && unit_flag(punit, F_SETTLERS)) {
    key_unit_connect(ACTIVITY_RAILROAD);
  } else {
    popup_worklists_report();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_connect_irrigate(GtkAction *action,
                                            gpointer user_data)
{
  key_unit_connect(ACTIVITY_IRRIGATE);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_patrol(GtkAction *action, gpointer user_data)
{
  key_unit_patrol();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_goto(GtkAction *action, gpointer user_data)
{
  key_unit_goto();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_goto_city(GtkAction *action, gpointer user_data)
{
  if(get_unit_in_focus()) {
    popup_goto_dialog();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_return(GtkAction *action, gpointer user_data)
{
  key_unit_return();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_disband(GtkAction *action, gpointer user_data)
{
  key_unit_disband();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_diplomat_dlg(GtkAction *action, gpointer user_data)
{
  key_unit_diplomat_actions();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_nuke(GtkAction *action, gpointer user_data)
{
  key_unit_nuke();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_wait(GtkAction *action, gpointer user_data)
{
  key_unit_wait();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_order_done(GtkAction *action, gpointer user_data)
{
  key_unit_done();
}

/****************************************************************
  ...
*****************************************************************/
static const char *load_menu_order(void)
{
  static char buf[2048];

  GtkActionEntry entries_order[] = {
    {"ORDER", NULL, N_("_Orders"), NULL, NULL, NULL},
    {"ORDER_BUILD_CITY", NULL, N_("_Build City"),
     "b", N_("_Build City"), G_CALLBACK(callback_order_build_city)},
    {"ORDER_ROAD", NULL, N_("Build _Road"),
     "r", N_("Build _Road"), G_CALLBACK(callback_order_road)},
    {"ORDER_IRRIGATE", NULL, N_("Build _Irrigation"),
     "i", N_("Build _Irrigation"), G_CALLBACK(callback_order_irrigate)},
    {"ORDER_MINE", NULL, N_("Build _Mine"),
     "m", N_("Build _Mine"), G_CALLBACK(callback_order_mine)},
    {"ORDER_TRANSFORM", NULL, N_("Transf_orm Terrain"),
     "o", N_("Transf_orm Terrain"), G_CALLBACK(callback_order_transform)},
    {"ORDER_FORTRESS", NULL, N_("Build _Fortress"),
     "f", N_("Build _Fortress"), G_CALLBACK(callback_order_fortress)},
    {"ORDER_AIRBASE", NULL, N_("Build Airbas_e"),
     "e", N_("Build Airbas_e"), G_CALLBACK(callback_order_airbase)},
    {"ORDER_POLLUTION", NULL, N_("Clean _Pollution"),
     "p", N_("Clean _Pollution"), G_CALLBACK(callback_order_pollution)},
    {"ORDER_FALLOUT", NULL, N_("Clean _Nuclear Fallout"),
     "n", N_("Clean _Nuclear Fallout"), G_CALLBACK(callback_order_fallout)},
    {"ORDER_SENTRY", NULL, N_("_Sentry"),
     "s", N_("_Sentry"), G_CALLBACK(callback_order_sentry)},
    {"ORDER_SLEEP", NULL, N_("Sleep"),
     "k", N_("Sleep"), G_CALLBACK(callback_order_sleep)},
    {"ORDER_PILLAGE", NULL, N_("Pi_llage"),
     "<Shift>p", N_("Pi_llage"), G_CALLBACK(callback_order_pillage)},
    {"ORDER_HOMECITY", NULL, N_("Make _Homecity"),
     "h", N_("Make _Homecity"), G_CALLBACK(callback_order_homecity)},
    {"ORDER_LOAD", NULL, N_("_Load"),
     "l", N_("_Load"), G_CALLBACK(callback_order_load)},
    {"ORDER_UNLOAD_TRANSPORTER", NULL, N_("_Unload Transporter"),
     "<Shift>u", N_("_Unload Transporter"), G_CALLBACK(callback_order_unload_transporter)},
    {"ORDER_UNLOAD", NULL, N_("_Unload"),
     "u", N_("_Unload"), G_CALLBACK(callback_order_unload)},
    {"ORDER_WAKEUP_OTHERS", NULL, N_("Wake up o_thers"),
     "<Shift>w", N_("Wake up o_thers"), G_CALLBACK(callback_order_wakeup_others)},
    {"ORDER_AUTO_SETTLER", NULL, N_("_Auto Settler"),
     "a", N_("_Auto Settler"), G_CALLBACK(callback_order_auto_settler)},
    {"ORDER_AUTO_EXPLORE", NULL, N_("Auto E_xplore"),
     "x", N_("Auto E_xplore"), G_CALLBACK(callback_order_auto_explore)},
    {"ORDER_CONNECT", NULL, N_("_Connect"), NULL, NULL, NULL},
    {"ORDER_CONNECT_ROAD", NULL, N_("Road"),
     "<Control>r", N_("Road"), G_CALLBACK(callback_order_connect_road)},
    {"ORDER_CONNECT_RAIL", NULL, N_("Rail"),
     "<Control>l", N_("Rail"), G_CALLBACK(callback_order_connect_rail)},
    {"ORDER_CONNECT_IRRIGATE", NULL, N_("Irrigate"),
     "<Control>i", N_("Irrigate"), G_CALLBACK(callback_order_connect_irrigate)},
    {"ORDER_PATROL", NULL, N_("Patrol (_Q)"),
     "q", N_("Patrol (_Q)"), G_CALLBACK(callback_order_patrol)},
    {"ORDER_GOTO", NULL, N_("_Go to"),
     "g", N_("_Go to"), G_CALLBACK(callback_order_goto)},
    {"ORDER_GOTO_CITY", NULL, N_("Go\\/_Airlift to City"),
     "<shift>l", N_("Go\\/_Airlift to City"), G_CALLBACK(callback_order_goto_city)},
    {"ORDER_RETURN", NULL, N_("Ret_urn to nearest city"),
     "<shift>g", N_("Ret_urn to nearest city"), G_CALLBACK(callback_order_return)},
    {"ORDER_DISBAND", NULL, N_("Disb_and Unit"),
     "<shift>d", N_("Disb_and Unit"), G_CALLBACK(callback_order_disband)},
    {"ORDER_DIPLOMAT_DLG", NULL, N_("D_iplomat\\/Spy Actions"),
     "d", N_("D_iplomat\\/Spy Actions"), G_CALLBACK(callback_order_diplomat_dlg)},
    {"ORDER_NUKE", NULL, N_("Explode _Nuclear"),
     "<shift>n", N_("Explode _Nuclear"), G_CALLBACK(callback_order_nuke)},
    {"ORDER_WAIT", NULL, N_("_Wait"),
     "w", N_("_Wait"), G_CALLBACK(callback_order_wait)},
    {"ORDER_DONE", NULL, N_("Don_e"),
     "space", N_("Don_e"), G_CALLBACK(callback_order_done)}
  };

  action_group_order = gtk_action_group_new("GroupOrder");
  gtk_action_group_set_translation_domain(action_group_order,
                                          textdomain(PACKAGE));

  gtk_action_group_add_actions(action_group_order, entries_order,
                               G_N_ELEMENTS(entries_order),
                               NULL);
  gtk_ui_manager_insert_action_group(main_uimanager, action_group_order, 0);

  my_snprintf(buf, sizeof(buf), "%s",
              "<menu action=\"ORDER\">\n"
              "<menuitem action=\"ORDER_BUILD_CITY\" />\n"
              "<menuitem action=\"ORDER_ROAD\" />\n"
              "<menuitem action=\"ORDER_IRRIGATE\" />\n"
              "<menuitem action=\"ORDER_MINE\" />\n"
              "<menuitem action=\"ORDER_TRANSFORM\" />\n"
              "<menuitem action=\"ORDER_FORTRESS\" />\n"
              "<menuitem action=\"ORDER_AIRBASE\" />\n"
              "<menuitem action=\"ORDER_POLLUTION\" />\n"
              "<menuitem action=\"ORDER_FALLOUT\" />\n"
              "<separator/>\n"
              "<menu action=\"ORDER_CONNECT\">\n"
              "<menuitem action=\"ORDER_CONNECT_ROAD\" />\n"
              "<menuitem action=\"ORDER_CONNECT_RAIL\" />\n"
              "<menuitem action=\"ORDER_CONNECT_IRRIGATE\" />\n"
              "</menu>\n"
              "<separator/>\n"
              "<menuitem action=\"ORDER_SENTRY\" />\n"
              "<menuitem action=\"ORDER_SLEEP\" />\n"
              "<menuitem action=\"ORDER_PILLAGE\" />\n"
              "<separator/>\n"
              "<menuitem action=\"ORDER_HOMECITY\" />\n"
              "<menuitem action=\"ORDER_LOAD\" />\n"
              "<menuitem action=\"ORDER_UNLOAD_TRANSPORTER\" />\n"
              "<menuitem action=\"ORDER_UNLOAD\" />\n"
              "<menuitem action=\"ORDER_WAKEUP_OTHERS\" />\n"
              "<separator/>\n"
              "<menuitem action=\"ORDER_AUTO_SETTLER\" />\n"
              "<menuitem action=\"ORDER_AUTO_EXPLORE\" />\n"
              "<separator/>\n"
              "<menuitem action=\"ORDER_PATROL\" />\n"
              "<menuitem action=\"ORDER_GOTO\" />\n"
              "<menuitem action=\"ORDER_GOTO_CITY\" />\n"
              "<menuitem action=\"ORDER_RETURN\" />\n"
              "<separator/>\n"
              "<menuitem action=\"ORDER_DISBAND\" />\n"
              "<menuitem action=\"ORDER_DIPLOMAT_DLG\" />\n"
              "<menuitem action=\"ORDER_NUKE\" />\n"
              "<separator/>\n"
              "<menuitem action=\"ORDER_WAIT\" />\n"
              "<menuitem action=\"ORDER_DONE\" />\n"
              "</menu>\n");

  return buf;
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_delayed_goto(GtkAction *action,
                                               gpointer user_data)
{
  key_unit_delayed_goto(0);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_delayed_paradrop_or_nuke(GtkAction *action,
                                                           gpointer user_data)
{
  key_unit_delayed_goto(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_delayed_airlift(GtkAction *action,
                                                  gpointer user_data)
{
  key_unit_delayed_airlift();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_add_pause(GtkAction *action,
                                            gpointer user_data)
{
  key_add_pause_delayed_goto();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_execute_delayed_goto(GtkAction *action,
                                                       gpointer user_data)
{
  key_unit_execute_delayed_goto();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_clear_delayed_orders(GtkAction *action,
                                                       gpointer user_data)
{
  key_unit_clear_delayed_orders();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg1_select(GtkAction *action,
                                             gpointer user_data)
{
  delayed_goto_copy(0, 1);
  update_delayed_goto_menu(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg1_add(GtkAction *action,
                                          gpointer user_data)
{
  delayed_goto_cat(0, 1);
  update_delayed_goto_menu(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg1_record(GtkAction *action,
                                             gpointer user_data)
{
  delayed_goto_move(1 ,0);
  update_delayed_goto_menu(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg1_clear(GtkAction *action,
                                            gpointer user_data)
{
  delayed_goto_clear(1);
  update_delayed_goto_menu(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg1_execute(GtkAction *action,
                                              gpointer user_data)
{
  request_unit_execute_delayed_goto(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg2_select(GtkAction *action,
                                             gpointer user_data)
{
  delayed_goto_copy(0, 2);
  update_delayed_goto_menu(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg2_add(GtkAction *action,
                                          gpointer user_data)
{
  delayed_goto_cat(0, 2);
  update_delayed_goto_menu(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg2_record(GtkAction *action,
                                             gpointer user_data)
{
  delayed_goto_move(2 ,0);
  update_delayed_goto_menu(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg2_clear(GtkAction *action,
                                            gpointer user_data)
{
  delayed_goto_clear(2);
  update_delayed_goto_menu(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg2_execute(GtkAction *action,
                                              gpointer user_data)
{
  request_unit_execute_delayed_goto(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg3_select(GtkAction *action,
                                             gpointer user_data)
{
  delayed_goto_move(0, 3);
  update_delayed_goto_menu(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg3_add(GtkAction *action,
                                          gpointer user_data)
{
  delayed_goto_cat(0, 3);
  update_delayed_goto_menu(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg3_record(GtkAction *action,
                                             gpointer user_data)
{
  delayed_goto_move(3 ,0);
  update_delayed_goto_menu(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg3_clear(GtkAction *action,
                                            gpointer user_data)
{
  delayed_goto_clear(3);
  update_delayed_goto_menu(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_dg3_execute(GtkAction *action,
                                              gpointer user_data)
{
  request_unit_execute_delayed_goto(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_inclusive_all(GtkToggleAction *action,
                                                gpointer user_data)
{
  if (!!(delayed_goto_inclusive_filter & FILTER_ALL) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_inclusive_filter, FILTER_ALL);
    update_delayed_goto_inclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_inclusive_new(GtkToggleAction *action,
                                                gpointer user_data)
{
  if (!!(delayed_goto_inclusive_filter & FILTER_NEW) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_inclusive_filter, FILTER_NEW);
    update_delayed_goto_inclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_inclusive_fortified(GtkToggleAction *action,
                                                      gpointer user_data)
{
  if (!!(delayed_goto_inclusive_filter & FILTER_FORTIFIED) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_inclusive_filter, FILTER_FORTIFIED);
    update_delayed_goto_inclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_inclusive_sentried(GtkToggleAction *action,
                                                     gpointer user_data)
{
  if (!!(delayed_goto_inclusive_filter & FILTER_SENTRIED) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_inclusive_filter, FILTER_SENTRIED);
    update_delayed_goto_inclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_inclusive_veteran(GtkToggleAction *action,
                                                    gpointer user_data)
{
  if (!!(delayed_goto_inclusive_filter & FILTER_VETERAN) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_inclusive_filter, FILTER_VETERAN);
    update_delayed_goto_inclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_inclusive_auto(GtkToggleAction *action,
                                                 gpointer user_data)
{
  if (!!(delayed_goto_inclusive_filter & FILTER_AUTO) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_inclusive_filter, FILTER_AUTO);
    update_delayed_goto_inclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_inclusive_idle(GtkToggleAction *action,
                                                 gpointer user_data)
{
  if (!!(delayed_goto_inclusive_filter & FILTER_IDLE) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_inclusive_filter, FILTER_IDLE);
    update_delayed_goto_inclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_inclusive_able_to_move(GtkToggleAction *action,
                                                         gpointer user_data)
{
  if (!!(delayed_goto_inclusive_filter & FILTER_ABLE_TO_MOVE) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_inclusive_filter, FILTER_ABLE_TO_MOVE);
    update_delayed_goto_inclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_inclusive_military(GtkToggleAction *action,
                                                     gpointer user_data)
{
  if (!!(delayed_goto_inclusive_filter & FILTER_MILITARY) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_inclusive_filter, FILTER_MILITARY);
    update_delayed_goto_inclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_inclusive_off(GtkToggleAction *action,
                                                gpointer user_data)
{
  if (!!(delayed_goto_inclusive_filter & FILTER_OFF) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_inclusive_filter, FILTER_OFF);
    update_delayed_goto_inclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_exclusive_all(GtkToggleAction *action,
                                                gpointer user_data)
{
  if (!!(delayed_goto_exclusive_filter & FILTER_ALL) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_exclusive_filter, FILTER_ALL);
    update_delayed_goto_exclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_exclusive_new(GtkToggleAction *action,
                                                gpointer user_data)
{
  if (!!(delayed_goto_exclusive_filter & FILTER_NEW) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_exclusive_filter, FILTER_NEW);
    update_delayed_goto_exclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_exclusive_fortified(GtkToggleAction *action,
                                                      gpointer user_data)
{
  if (!!(delayed_goto_exclusive_filter & FILTER_FORTIFIED) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_exclusive_filter, FILTER_FORTIFIED);
    update_delayed_goto_exclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_exclusive_sentried(GtkToggleAction *action,
                                                     gpointer user_data)
{
  if (!!(delayed_goto_exclusive_filter & FILTER_SENTRIED) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_exclusive_filter, FILTER_SENTRIED);
    update_delayed_goto_exclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_exclusive_veteran(GtkToggleAction *action,
                                                    gpointer user_data)
{
  if (!!(delayed_goto_exclusive_filter & FILTER_VETERAN) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_exclusive_filter, FILTER_VETERAN);
    update_delayed_goto_exclusive_filter_menu();
  }

}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_exclusive_auto(GtkToggleAction *action,
                                                 gpointer user_data)
{
  if (!!(delayed_goto_exclusive_filter & FILTER_AUTO) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_exclusive_filter, FILTER_AUTO);
    update_delayed_goto_exclusive_filter_menu();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_exclusive_idle(GtkToggleAction *action,
                                                 gpointer user_data)
{
  if (!!(delayed_goto_exclusive_filter & FILTER_IDLE) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_exclusive_filter, FILTER_IDLE);
    update_delayed_goto_exclusive_filter_menu();
  }

}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_exclusive_able_to_move(GtkToggleAction *action,
                                                         gpointer user_data)
{
  if (!!(delayed_goto_exclusive_filter & FILTER_ABLE_TO_MOVE) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_exclusive_filter, FILTER_ABLE_TO_MOVE);
    update_delayed_goto_exclusive_filter_menu();
  }

}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_exclusive_military(GtkToggleAction *action,
                                                     gpointer user_data)
{
  if (!!(delayed_goto_exclusive_filter & FILTER_MILITARY) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_exclusive_filter, FILTER_MILITARY);
    update_delayed_goto_exclusive_filter_menu();
  }

}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_exclusive_off(GtkToggleAction *action,
                                                gpointer user_data)
{
  if (!!(delayed_goto_exclusive_filter & FILTER_OFF) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&delayed_goto_exclusive_filter, FILTER_OFF);
    update_delayed_goto_exclusive_filter_menu();
  }

}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_place(GtkAction *unusedaction,
                                        GtkRadioAction *action,
                                        gpointer user_data)
{
  delayed_goto_place = gtk_radio_action_get_current_value(action);
  if (block_delayed_goto_utype_place) {
    menu_radio_set_active(radio_action_group_delayed_goto_utype_place,
                          "DELAYED_GOTO_FAST_UNSET");
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_utype(GtkAction *unusedaction,
                                        GtkRadioAction *action,
                                        gpointer user_data)
{
  delayed_goto_utype = gtk_radio_action_get_current_value(action);
  if (block_delayed_goto_utype_place) {
    menu_radio_set_active(radio_action_group_delayed_goto_utype_place,
                          "DELAYED_GOTO_FAST_UNSET");
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_delayed_goto_utype_place(GtkAction *unusedaction,
                                              GtkRadioAction *action,
                                              gpointer user_data)
{
  int value = gtk_radio_action_get_current_value(action);

  block_delayed_goto_utype_place = FALSE;

  switch (value) {
  case -1:
    break;
  case 0:
    menu_radio_set_active(radio_action_group_delayed_goto_place,
                          "DELAYED_GOTO_GOTO_SINGLE_UNIT");
    break;
  case 1:
    menu_radio_set_active(radio_action_group_delayed_goto_place,
                          "DELAYED_GOTO_GOTO_ON_TILE");
    menu_radio_set_active(radio_action_group_delayed_goto_utype,
                          "DELAYED_GOTO_GOTO_SAME_TYPE");
    break;
  case 2:
    menu_radio_set_active(radio_action_group_delayed_goto_place,
                          "DELAYED_GOTO_GOTO_ON_CONTINENT");
    menu_radio_set_active(radio_action_group_delayed_goto_utype,
                          "DELAYED_GOTO_GOTO_SAME_TYPE");
    break;
  case 3:
    menu_radio_set_active(radio_action_group_delayed_goto_place,
                          "DELAYED_GOTO_GOTO_ON_TILE");
    menu_radio_set_active(radio_action_group_delayed_goto_utype,
                          "DELAYED_GOTO_GOTO_ALL");
    break;
  default:
    assert(0);
  }

  block_delayed_goto_utype_place = TRUE;
}

/****************************************************************
  ... for automatic processus
*****************************************************************/
static void update_automatic_processus_filter_menu(automatic_processus *pap,
                                                   int j)
{
  int i;
  char buf[256];

  for (i = 0; i < AUTO_VALUES_NUM; i++) {
    my_snprintf(buf, sizeof(buf), "%s_%d",
                pap->menu, i);
    if (is_auto_value_allowed(pap, i)) {
      menu_toggle_set_active(toggle_action_group_delayed_goto_automatic[j],
                             buf, pap->auto_filter & AV_TO_FV(i));
    }
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_menu_delayed_goto_automatic(GtkToggleAction *action,
                                                 gpointer user_data)
{
  int j = GPOINTER_TO_INT(user_data);
  int i;
  char buf[256];

  automatic_processus_iterate(pap)
  {
    if(pap->menu[0]=='\0') {
      continue;
    }

    for(i=0;i<AUTO_VALUES_NUM;i++){
      my_snprintf(buf, sizeof(buf), "%s_%s",
                  pap->menu, ap_event_name(i));

      if (is_auto_value_allowed(pap, i) &&
          strcmp(buf, gtk_action_get_name(GTK_ACTION(action))) == 0) {

        if (gtk_toggle_action_get_active(action) ^
            (bool)(pap->auto_filter&AV_TO_FV(i)))
          {
            auto_filter_change(pap,i);
            update_automatic_processus_filter_menu(pap, j);
          }
        return;
      }
    }
  } automatic_processus_iterate_end;
}

/****************************************************************
  ...
*****************************************************************/
static const char *load_menu_delayed_goto_automatic(const char *actionname)
{
  int i;
  int j = 0;
  int k = 0;
  char buf[256];
  GtkToggleActionEntry toggle_entries_delayed_goto_automatic[AUTO_VALUES_NUM];
  char name[AUTO_VALUES_NUM][256];
  char label[AUTO_VALUES_NUM][256];
  static char retbuf[1024];

  retbuf[0] = '\0';

  automatic_processus_iterate(pap)
  {
    j++;
    if(pap->menu[0]=='\0' || strcmp(pap->menu, actionname) != 0) {
      continue;
    }
    for (i=0;i<AUTO_VALUES_NUM;i++)
      {
        if(!is_auto_value_allowed(pap,i))
          continue;
        my_snprintf(buf, sizeof(buf), "%s_%d",
                    pap->menu, i);
        cat_snprintf(retbuf, sizeof(retbuf),
                     "<menuitem action=\"%s\" />\n", buf);

        sz_strlcpy(name[k], buf);
        sz_strlcpy(label[k], ap_event_name(i));
        toggle_entries_delayed_goto_automatic[k].name = name[k];
        toggle_entries_delayed_goto_automatic[k].stock_id = NULL;
        toggle_entries_delayed_goto_automatic[k].label = label[k];
        toggle_entries_delayed_goto_automatic[k].accelerator = NULL;
        toggle_entries_delayed_goto_automatic[k].tooltip = label[k];
        toggle_entries_delayed_goto_automatic[k].callback =
          G_CALLBACK(callback_menu_delayed_goto_automatic);
        toggle_entries_delayed_goto_automatic[k].is_active = TRUE;
        sz_strlcpy(pap->namemenu[k], name[k]);
        k++;
      }
    break;
  } automatic_processus_iterate_end;

  my_snprintf(buf, sizeof(buf), "ToggleGroupDelayedGotoAutomatic%d", j-1);
  toggle_action_group_delayed_goto_automatic[j-1] = gtk_action_group_new(buf);
  gtk_action_group_set_translation_domain(toggle_action_group_delayed_goto_automatic[j-1],
                                          textdomain(PACKAGE));


  gtk_action_group_add_toggle_actions(toggle_action_group_delayed_goto_automatic[j-1],
                                      toggle_entries_delayed_goto_automatic,
                                      k,
                                      GINT_TO_POINTER(j-1));
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     toggle_action_group_delayed_goto_automatic[j-1],
                                     0);

  freelog(LOG_MENU, "Strlen of buf = %d; "
          "Size of load_menu_delayed_goto_automatic %d",
          strlen(retbuf), sizeof(retbuf));

  return retbuf;
}

/****************************************************************
  ...
*****************************************************************/
static const char *load_menu_delayed_goto(void)
{
  static char buf[8192];

  GtkActionEntry entries_delayed_goto[] = {
    {"DELAYED_GOTO", NULL, N_("_Delayed Goto"), NULL, NULL, NULL},
    {"DELAYED_GOTO_DELAYED_GOTO", NULL, N_("Delayed go_to"),
     "z", N_("Delayed go_to"), G_CALLBACK(callback_delayed_goto_delayed_goto)},
    {"DELAYED_GOTO_DELAYED_PARADROP_OR_NUKE", NULL,
     N_("Delayed pa_radrop or nuke"),
     "<Control>z", N_("Delayed pa_radrop or nuke"),
     G_CALLBACK(callback_delayed_goto_delayed_paradrop_or_nuke)},
    {"DELAYED_GOTO_DELAYED_AIRLIFT", NULL, N_("Delayed _airlift"),
     "<Control>y", N_("Delayed _airlift"),
     G_CALLBACK(callback_delayed_goto_delayed_airlift)},
    {"DELAYED_GOTO_ADD_PAUSE", NULL, N_("_Add Pause"),
     "<Control>b", N_("_Add Pause"),
     G_CALLBACK(callback_delayed_goto_add_pause)},
    {"DELAYED_GOTO_EXECUTE_DELAYED_GOTO", NULL, N_("_Execute delayed goto"),
     "y", N_("_Execute delayed goto"),
     G_CALLBACK(callback_delayed_goto_execute_delayed_goto)},
    {"DELAYED_GOTO_CLEAR_DELAYED_ORDERS", NULL, N_("_Clear delayed orders"),
     "u", N_("_Clear delayed orders"),
     G_CALLBACK(callback_delayed_goto_clear_delayed_orders)},
    {"DELAYED_GOTO_MODE", NULL, N_("Delayed goto _mode"), NULL, NULL, NULL},
    {"DELAYED_GOTO_AUTOMATIC", NULL, N_("_Automatic execution"), 
     NULL, NULL, NULL},
    {"DELAYED_GOTO_INCLUSIVE", NULL, N_("_Inclusive filter"), NULL, NULL, NULL},
    {"DELAYED_GOTO_EXCLUSIVE", NULL, N_("Exclusive filter"), NULL, NULL, NULL},
    {"DELAYED_GOTO_DG1", NULL, N_("Delayed goto selection") " _1",
     NULL, NULL, NULL},
    {"DELAYED_GOTO_DG1_SELECT", NULL, N_("_Select"),
     "<Alt>1", N_("_Select"), G_CALLBACK(callback_delayed_goto_dg1_select)},
    {"DELAYED_GOTO_DG1_ADD", NULL, N_("_Add to current selection"),
     "<Shift><Alt>1", N_("_Add to current selection"),
     G_CALLBACK(callback_delayed_goto_dg1_add)},
    {"DELAYED_GOTO_DG1_RECORD", NULL, N_("_Record"),
     "<Control><Alt>1", N_("_Record"),
     G_CALLBACK(callback_delayed_goto_dg1_record)},
    {"DELAYED_GOTO_DG1_CLEAR", NULL, N_("_Clear"),
     NULL, N_("_Clear"), G_CALLBACK(callback_delayed_goto_dg1_clear)},
    {"DELAYED_GOTO_DG1_EXECUTE", NULL, N_("_Execute"),
     "<Control><Shift><Alt>1", N_("_Clear"),
     G_CALLBACK(callback_delayed_goto_dg1_execute)},
    {"DELAYED_GOTO_DG1_AUTOMATIC", NULL, N_("_Automatic execution"),
     NULL, NULL, NULL},
    {"DELAYED_GOTO_DG2", NULL, N_("Delayed goto selection") " _2",
     NULL, NULL, NULL},
    {"DELAYED_GOTO_DG2_SELECT", NULL, N_("_Select"),
     "<Alt>2", N_("_Select"), G_CALLBACK(callback_delayed_goto_dg2_select)},
    {"DELAYED_GOTO_DG2_ADD", NULL, N_("_Add to current selection"),
     "<Shift><Alt>2", N_("_Add to current selection"),
     G_CALLBACK(callback_delayed_goto_dg2_add)},
    {"DELAYED_GOTO_DG2_RECORD", NULL, N_("_Record"),
     "<Control><Alt>2", N_("_Record"),
     G_CALLBACK(callback_delayed_goto_dg2_record)},
    {"DELAYED_GOTO_DG2_CLEAR", NULL, N_("_Clear"),
     NULL, N_("_Clear"), G_CALLBACK(callback_delayed_goto_dg2_clear)},
    {"DELAYED_GOTO_DG2_EXECUTE", NULL, N_("_Execute"),
     "<Control><Shift><Alt>2", N_("_Clear"),
     G_CALLBACK(callback_delayed_goto_dg2_execute)},
    {"DELAYED_GOTO_DG2_AUTOMATIC", NULL, N_("_Automatic execution"),
     NULL, NULL, NULL},
    {"DELAYED_GOTO_DG3", NULL, N_("Delayed goto selection") " _3",
     NULL, NULL, NULL},
    {"DELAYED_GOTO_DG3_SELECT", NULL, N_("_Select"),
     "<Alt>3", N_("_Select"), G_CALLBACK(callback_delayed_goto_dg3_select)},
    {"DELAYED_GOTO_DG3_ADD", NULL, N_("_Add to current selection"),
     "<Shift><Alt>3", N_("_Add to current selection"),
     G_CALLBACK(callback_delayed_goto_dg3_add)},
    {"DELAYED_GOTO_DG3_RECORD", NULL, N_("_Record"),
     "<Control><Alt>3", N_("_Record"),
     G_CALLBACK(callback_delayed_goto_dg3_record)},
    {"DELAYED_GOTO_DG3_CLEAR", NULL, N_("_Clear"),
     NULL, N_("_Clear"), G_CALLBACK(callback_delayed_goto_dg3_clear)},
    {"DELAYED_GOTO_DG3_EXECUTE", NULL, N_("_Execute"),
     "<Control><Shift><Alt>3", N_("_Clear"),
     G_CALLBACK(callback_delayed_goto_dg3_execute)},
    {"DELAYED_GOTO_DG3_AUTOMATIC", NULL, N_("_Automatic execution"),
     NULL, NULL, NULL}
  };

  GtkToggleActionEntry toggle_entries_delayed_goto_inclusive[] = {
    {"DELAYED_GOTO_INCLUSIVE_ALL", NULL, N_("_All units"),
     NULL, N_("_All units"), G_CALLBACK(callback_delayed_goto_inclusive_all),
     TRUE},
    {"DELAYED_GOTO_INCLUSIVE_NEW", NULL, N_("_New units"),
     NULL, N_("_New units"), G_CALLBACK(callback_delayed_goto_inclusive_new),
     TRUE},
    {"DELAYED_GOTO_INCLUSIVE_FORTIFIED", NULL, N_("_Fortified units"),
     NULL, N_("_Fortified units"),
     G_CALLBACK(callback_delayed_goto_inclusive_fortified), TRUE},
    {"DELAYED_GOTO_INCLUSIVE_SENTRIED", NULL, N_("_Sentried units"),
     NULL, N_("_Sentried units"),
     G_CALLBACK(callback_delayed_goto_inclusive_sentried), TRUE},
    {"DELAYED_GOTO_INCLUSIVE_VETERAN", NULL, N_("_Veteran units"),
     NULL, N_("_Veteran units"),
     G_CALLBACK(callback_delayed_goto_inclusive_veteran), TRUE},
    {"DELAYED_GOTO_INCLUSIVE_AUTO", NULL, N_("_Auto units"),
     NULL, N_("_Auto units"),
     G_CALLBACK(callback_delayed_goto_inclusive_auto), TRUE},
    {"DELAYED_GOTO_INCLUSIVE_IDLE", NULL, N_("_Idle units"),
     NULL, N_("_Idle units"),
     G_CALLBACK(callback_delayed_goto_inclusive_idle), TRUE},
    {"DELAYED_GOTO_INCLUSIVE_ABLE_TO_MOVE", NULL, N_("Units abl_e to move"),
     NULL, N_("Units abl_e to move"),
     G_CALLBACK(callback_delayed_goto_inclusive_able_to_move), TRUE},
    {"DELAYED_GOTO_INCLUSIVE_MILITARY", NULL, N_("_Military units"),
     NULL, N_("_Military units"),
     G_CALLBACK(callback_delayed_goto_inclusive_military), TRUE},
    {"DELAYED_GOTO_INCLUSIVE_OFF", NULL, N_("_Off"),
     NULL, N_("_Off"), G_CALLBACK(callback_delayed_goto_inclusive_off), TRUE}
  };

  GtkToggleActionEntry toggle_entries_delayed_goto_exclusive[] = {
    {"DELAYED_GOTO_EXCLUSIVE_ALL", NULL, N_("_All units"),
     NULL, N_("_All units"), G_CALLBACK(callback_delayed_goto_exclusive_all),
     TRUE},
    {"DELAYED_GOTO_EXCLUSIVE_NEW", NULL, N_("_New units"),
     NULL, N_("_New units"), G_CALLBACK(callback_delayed_goto_exclusive_new),
     TRUE},
    {"DELAYED_GOTO_EXCLUSIVE_FORTIFIED", NULL, N_("_Fortified units"),
     NULL, N_("_Fortified units"),
     G_CALLBACK(callback_delayed_goto_exclusive_fortified), TRUE},
    {"DELAYED_GOTO_EXCLUSIVE_SENTRIED", NULL, N_("_Sentried units"),
     NULL, N_("_Sentried units"),
     G_CALLBACK(callback_delayed_goto_exclusive_sentried), TRUE},
    {"DELAYED_GOTO_EXCLUSIVE_VETERAN", NULL, N_("_Veteran units"),
     NULL, N_("_Veteran units"),
     G_CALLBACK(callback_delayed_goto_exclusive_veteran), TRUE},
    {"DELAYED_GOTO_EXCLUSIVE_AUTO", NULL, N_("_Auto units"),
     NULL, N_("_Auto units"),
     G_CALLBACK(callback_delayed_goto_exclusive_auto), TRUE},
    {"DELAYED_GOTO_EXCLUSIVE_IDLE", NULL, N_("_Idle units"),
     NULL, N_("_Idle units"),
     G_CALLBACK(callback_delayed_goto_exclusive_idle), TRUE},
    {"DELAYED_GOTO_EXCLUSIVE_ABLE_TO_MOVE", NULL, N_("Units abl_e to move"),
     NULL, N_("Units abl_e to move"),
     G_CALLBACK(callback_delayed_goto_exclusive_able_to_move), TRUE},
    {"DELAYED_GOTO_EXCLUSIVE_MILITARY", NULL, N_("_Military units"),
     NULL, N_("_Military units"),
     G_CALLBACK(callback_delayed_goto_exclusive_military), TRUE},
    {"DELAYED_GOTO_EXCLUSIVE_OFF", NULL, N_("_Off"),
     NULL, N_("_Off"), G_CALLBACK(callback_delayed_goto_exclusive_off), TRUE}
  };

  GtkRadioActionEntry radio_entries_delayed_goto_place[] = {
    {"DELAYED_GOTO_GOTO_SINGLE_UNIT", NULL, N_("_Single unit"),
     "<Alt><Shift>F1", N_("_Single unit"), PLACE_SINGLE_UNIT},
    {"DELAYED_GOTO_GOTO_IN_TRANSPORTER", NULL,
     N_("All _units in the same transporter"),
     "<Alt><Shift>F2", N_("All _units in the same transporter"),
     PLACE_IN_TRANSPORTER},
    {"DELAYED_GOTO_GOTO_ON_TILE", NULL, N_("All units on the _tile"),
     "<Alt><Shift>F3", N_("All units on the _tile"), PLACE_ON_TILE},
    {"DELAYED_GOTO_GOTO_ON_CONTINENT", NULL, N_("All units on the _continent"),
     "<Alt><Shift>F4", N_("All units on the _continent"), PLACE_ON_CONTINENT},
    {"DELAYED_GOTO_GOTO_EVERY_WHERE", NULL, N_("_All units"),
     "<Alt><Shift>F5", N_("_All units"), PLACE_EVERY_WHERE}
  };

  GtkRadioActionEntry radio_entries_delayed_goto_utype[] = {
    {"DELAYED_GOTO_GOTO_SAME_TYPE", NULL, N_("Only units with the _same type"),
     "<Alt><Shift>F6", N_("Only units with the _same type"), UTYPE_SAME_TYPE},
    {"DELAYED_GOTO_GOTO_SAME_MOVE_TYPE", NULL,
     N_("Only units with the same _move type"),
     "<Alt><Shift>F7", N_("Only units with the same _move type"),
     UTYPE_SAME_MOVE_TYPE},
    {"DELAYED_GOTO_GOTO_ALL", NULL, N_("_All unit types"),
     "<Alt><Shift>F8", N_("_All unit types"), UTYPE_ALL}
  };

  GtkRadioActionEntry radio_entries_delayed_goto_utype_place[] = {
    {"DELAYED_GOTO_FAST_UNSET", NULL, N_("Unsed"),
     NULL, N_("Unsed"), -1},
    {"DELAYED_GOTO_FAST_SINGLE", NULL, N_("Single unit (fast)"),
     "F9", N_("Single unit (fast)"), 0},
    {"DELAYED_GOTO_FAST_TILE", NULL,
     N_("All units of the same type on the tile (fast)"),
     "F10", N_("All units of the same type on the tile (fast)"), 1},
    {"DELAYED_GOTO_FAST_TILE_ALL_TYPES", NULL,
     N_("All units on the tile (fast)"),
     "F11", N_("All units on the tile (fast)"), 2},
    {"DELAYED_GOTO_FAST_CONTINENT", NULL,
     N_("All units of the same type on the continent (fast)"),
     "F12", N_("All units of the same type on the continent (fast)"), 3}
  };

  action_group_delayed_goto = gtk_action_group_new("GroupDelayedGoto");
  gtk_action_group_set_translation_domain(action_group_delayed_goto,
                                          textdomain(PACKAGE));
  gtk_action_group_set_translation_domain(action_group_delayed_goto,
                                          textdomain(PACKAGE));

  gtk_action_group_add_actions(action_group_delayed_goto, entries_delayed_goto,
                               G_N_ELEMENTS(entries_delayed_goto),
                               NULL);
  gtk_ui_manager_insert_action_group(main_uimanager, action_group_delayed_goto, 0);

  toggle_action_group_delayed_goto_inclusive =
    gtk_action_group_new("ToggleGroupDelayedGotoInclusive");
  gtk_action_group_set_translation_domain(toggle_action_group_delayed_goto_inclusive,
                                          textdomain(PACKAGE));
  gtk_action_group_add_toggle_actions(toggle_action_group_delayed_goto_inclusive,
                                      toggle_entries_delayed_goto_inclusive,
                                      G_N_ELEMENTS(toggle_entries_delayed_goto_inclusive),
                                      NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     toggle_action_group_delayed_goto_inclusive, 0);

  toggle_action_group_delayed_goto_exclusive =
    gtk_action_group_new("ToggleGroupDelayedGotoExclusive");
  gtk_action_group_set_translation_domain(toggle_action_group_delayed_goto_exclusive,
                                          textdomain(PACKAGE));
  gtk_action_group_add_toggle_actions(toggle_action_group_delayed_goto_exclusive,
                                      toggle_entries_delayed_goto_exclusive,
                                      G_N_ELEMENTS(toggle_entries_delayed_goto_exclusive),
                                      NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     toggle_action_group_delayed_goto_exclusive, 0);

  radio_action_group_delayed_goto_utype =
    gtk_action_group_new("RadioGroupDelayedGotoUtype");
  gtk_action_group_set_translation_domain(radio_action_group_delayed_goto_utype,
                                          textdomain(PACKAGE));
  gtk_action_group_add_radio_actions(radio_action_group_delayed_goto_utype,
                                     radio_entries_delayed_goto_utype,
                                     G_N_ELEMENTS(radio_entries_delayed_goto_utype),
                                     -1,
                                     G_CALLBACK(callback_delayed_goto_utype),
                                     NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     radio_action_group_delayed_goto_utype, 0);

  radio_action_group_delayed_goto_place =
    gtk_action_group_new("RadioGroupDelayedGotoPlace");
  gtk_action_group_set_translation_domain(radio_action_group_delayed_goto_place,
                                          textdomain(PACKAGE));
  gtk_action_group_add_radio_actions(radio_action_group_delayed_goto_place,
                                     radio_entries_delayed_goto_place,
                                     G_N_ELEMENTS(radio_entries_delayed_goto_place),
                                     -1,
                                     G_CALLBACK(callback_delayed_goto_place),
                                     NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     radio_action_group_delayed_goto_place, 0);

  radio_action_group_delayed_goto_utype_place =
    gtk_action_group_new("RadioGroupDelayedGotoUtypePlace");
  gtk_action_group_set_translation_domain(radio_action_group_delayed_goto_utype_place,
                                          textdomain(PACKAGE));
  gtk_action_group_add_radio_actions(radio_action_group_delayed_goto_utype_place,
                                     radio_entries_delayed_goto_utype_place,
                                     G_N_ELEMENTS(radio_entries_delayed_goto_utype_place),
                                     -1,
                                     G_CALLBACK(callback_delayed_goto_utype_place),
                                     NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     radio_action_group_delayed_goto_utype_place, 0);

  my_snprintf(buf, sizeof(buf),
              "<menu action=\"DELAYED_GOTO\">\n"
              "<menuitem action=\"DELAYED_GOTO_DELAYED_GOTO\" />\n"
              "<menuitem action=\"DELAYED_GOTO_DELAYED_PARADROP_OR_NUKE\" />\n"
              "<menuitem action=\"DELAYED_GOTO_DELAYED_AIRLIFT\" />\n"
              "<menuitem action=\"DELAYED_GOTO_ADD_PAUSE\" />\n"
              "<menuitem action=\"DELAYED_GOTO_EXECUTE_DELAYED_GOTO\" />\n"
              "<menuitem action=\"DELAYED_GOTO_CLEAR_DELAYED_ORDERS\" />\n"
              "<separator/>\n"
              "<menu action=\"DELAYED_GOTO_MODE\">\n"
              "<menuitem action=\"DELAYED_GOTO_GOTO_SINGLE_UNIT\" />\n"
              "<menuitem action=\"DELAYED_GOTO_GOTO_IN_TRANSPORTER\" />\n"
              "<menuitem action=\"DELAYED_GOTO_GOTO_ON_TILE\" />\n"
              "<menuitem action=\"DELAYED_GOTO_GOTO_ON_CONTINENT\" />\n"
              "<menuitem action=\"DELAYED_GOTO_GOTO_EVERY_WHERE\" />\n"
              "<separator/>\n"
              "<menuitem action=\"DELAYED_GOTO_GOTO_SAME_TYPE\" />\n"
              "<menuitem action=\"DELAYED_GOTO_GOTO_SAME_MOVE_TYPE\" />\n"
              "<menuitem action=\"DELAYED_GOTO_GOTO_ALL\" />\n"
              "<separator/>\n"
              "<menuitem action=\"DELAYED_GOTO_FAST_UNSET\" />\n"
              "<menuitem action=\"DELAYED_GOTO_FAST_SINGLE\" />\n"
              "<menuitem action=\"DELAYED_GOTO_FAST_TILE\" />\n"
              "<menuitem action=\"DELAYED_GOTO_FAST_TILE_ALL_TYPES\" />\n"
              "<menuitem action=\"DELAYED_GOTO_FAST_CONTINENT\" />\n"
              "</menu>\n"
              "<menu action=\"DELAYED_GOTO_AUTOMATIC\">\n"
              "%s"
              "</menu>\n"
              "<menu action=\"DELAYED_GOTO_INCLUSIVE\">\n"
              "<menuitem action=\"DELAYED_GOTO_INCLUSIVE_ALL\" />\n"
              "<menuitem action=\"DELAYED_GOTO_INCLUSIVE_NEW\" />\n"
              "<menuitem action=\"DELAYED_GOTO_INCLUSIVE_FORTIFIED\" />\n"
              "<menuitem action=\"DELAYED_GOTO_INCLUSIVE_SENTRIED\" />\n"
              "<menuitem action=\"DELAYED_GOTO_INCLUSIVE_VETERAN\" />\n"
              "<menuitem action=\"DELAYED_GOTO_INCLUSIVE_AUTO\" />\n"
              "<menuitem action=\"DELAYED_GOTO_INCLUSIVE_IDLE\" />\n"
              "<menuitem action=\"DELAYED_GOTO_INCLUSIVE_ABLE_TO_MOVE\" />\n"
              "<menuitem action=\"DELAYED_GOTO_INCLUSIVE_MILITARY\" />\n"
              "<menuitem action=\"DELAYED_GOTO_INCLUSIVE_OFF\" />\n"
              "</menu>\n"
              "<menu action=\"DELAYED_GOTO_EXCLUSIVE\">\n"
              "<menuitem action=\"DELAYED_GOTO_EXCLUSIVE_ALL\" />\n"
              "<menuitem action=\"DELAYED_GOTO_EXCLUSIVE_NEW\" />\n"
              "<menuitem action=\"DELAYED_GOTO_EXCLUSIVE_FORTIFIED\" />\n"
              "<menuitem action=\"DELAYED_GOTO_EXCLUSIVE_SENTRIED\" />\n"
              "<menuitem action=\"DELAYED_GOTO_EXCLUSIVE_VETERAN\" />\n"
              "<menuitem action=\"DELAYED_GOTO_EXCLUSIVE_AUTO\" />\n"
              "<menuitem action=\"DELAYED_GOTO_EXCLUSIVE_IDLE\" />\n"
              "<menuitem action=\"DELAYED_GOTO_EXCLUSIVE_ABLE_TO_MOVE\" />\n"
              "<menuitem action=\"DELAYED_GOTO_EXCLUSIVE_MILITARY\" />\n"
              "<menuitem action=\"DELAYED_GOTO_EXCLUSIVE_OFF\" />\n"
              "</menu>\n",
              load_menu_delayed_goto_automatic("DELAYED_GOTO_AUTOMATIC"));

  cat_snprintf(buf, sizeof(buf),
               "<separator/>\n"
               "<menu action=\"DELAYED_GOTO_DG1\">\n"
               "<menuitem action=\"DELAYED_GOTO_DG1_SELECT\" />\n"
               "<menuitem action=\"DELAYED_GOTO_DG1_ADD\" />\n"
               "<menuitem action=\"DELAYED_GOTO_DG1_RECORD\" />\n"
               "<menuitem action=\"DELAYED_GOTO_DG1_CLEAR\" />\n"
               "<menuitem action=\"DELAYED_GOTO_DG1_EXECUTE\" />\n"
               "<menu action=\"DELAYED_GOTO_DG1_AUTOMATIC\">\n"
               "%s"
               "</menu>\n"
               "</menu>\n",
               load_menu_delayed_goto_automatic("DELAYED_GOTO_DG1_AUTOMATIC"));

  cat_snprintf(buf, sizeof(buf),
               "<menu action=\"DELAYED_GOTO_DG2\">\n"
               "<menuitem action=\"DELAYED_GOTO_DG2_SELECT\" />\n"
               "<menuitem action=\"DELAYED_GOTO_DG2_ADD\" />\n"
               "<menuitem action=\"DELAYED_GOTO_DG2_RECORD\" />\n"
               "<menuitem action=\"DELAYED_GOTO_DG2_CLEAR\" />\n"
               "<menuitem action=\"DELAYED_GOTO_DG2_EXECUTE\" />\n"
               "<menu action=\"DELAYED_GOTO_DG2_AUTOMATIC\">\n"
               "%s"
               "</menu>\n"
               "</menu>\n",
               load_menu_delayed_goto_automatic("DELAYED_GOTO_DG2_AUTOMATIC"));

  cat_snprintf(buf, sizeof(buf),
               "<menu action=\"DELAYED_GOTO_DG3\">\n"
               "<menuitem action=\"DELAYED_GOTO_DG3_SELECT\" />\n"
               "<menuitem action=\"DELAYED_GOTO_DG3_ADD\" />\n"
               "<menuitem action=\"DELAYED_GOTO_DG3_RECORD\" />\n"
               "<menuitem action=\"DELAYED_GOTO_DG3_CLEAR\" />\n"
               "<menuitem action=\"DELAYED_GOTO_DG3_EXECUTE\" />\n"
               "<menu action=\"DELAYED_GOTO_DG3_AUTOMATIC\">\n"
               "%s"
               "</menu>\n"
               "</menu>\n"
               "</menu>\n",
               load_menu_delayed_goto_automatic("DELAYED_GOTO_DG3_AUTOMATIC"));

  freelog(LOG_MENU, "Strlen of buf = %d; Size of load_menu_delayed_goto %d",
          strlen(buf), sizeof(buf));

  return buf;
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_set_airlift_dest(GtkAction *action,
                                              gpointer user_data)
{
  request_auto_airlift_destination_selection();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_set_airlift_src(GtkAction *action,
                                             gpointer user_data)
{
  request_auto_airlift_source_selection();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_airlift_select_airport_cities(GtkAction *action,
                                                           gpointer user_data)
{
  request_auto_airlift_source_selection_with_airport();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_clear_airlift_queue(GtkAction *action,
                                                 gpointer user_data)
{
  struct unit *punit = get_unit_in_focus();

  if (punit && get_transporter_occupancy(punit) > 0) {
    key_unit_unload_all();
  } else {
    airlift_queue_clear(0);
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_show_cities_in_airlift_queue(GtkAction *action,
                                                          gpointer user_data)
{
  airlift_queue_show(0);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq1_select(GtkAction *action, gpointer user_data)
{
  airlift_queue_copy(0, 1);
  update_airlift_menu(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq1_add(GtkAction *action, gpointer user_data)
{
  airlift_queue_cat(0, 1);
  update_airlift_menu(1);

}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq1_record(GtkAction *action, gpointer user_data)
{
  airlift_queue_move(1 ,0);
  update_airlift_menu(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq1_clear(GtkAction *action, gpointer user_data)
{
  airlift_queue_clear(1);
  update_airlift_menu(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq1_execute(GtkAction *action, gpointer user_data)
{
  do_airlift_for(1,NULL);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq1_show(GtkAction *action, gpointer user_data)
{
  airlift_queue_show(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq2_select(GtkAction *action, gpointer user_data)
{
  airlift_queue_copy(0, 2);
  update_airlift_menu(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq2_add(GtkAction *action, gpointer user_data)
{
  airlift_queue_cat(0, 2);
  update_airlift_menu(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq2_record(GtkAction *action, gpointer user_data)
{
  airlift_queue_move(2 ,0);
  update_airlift_menu(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq2_clear(GtkAction *action, gpointer user_data)
{
  airlift_queue_clear(2);
  update_airlift_menu(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq2_execute(GtkAction *action, gpointer user_data)
{
  do_airlift_for(2,NULL);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq2_show(GtkAction *action, gpointer user_data)
{
  airlift_queue_show(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq3_select(GtkAction *action, gpointer user_data)
{
  airlift_queue_copy(0, 3);
  update_airlift_menu(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq3_add(GtkAction *action, gpointer user_data)
{
  airlift_queue_cat(0, 3);
  update_airlift_menu(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq3_record(GtkAction *action, gpointer user_data)
{
  airlift_queue_move(3 ,0);
  update_airlift_menu(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq3_clear(GtkAction *action, gpointer user_data)
{
  airlift_queue_clear(3);
  update_airlift_menu(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq3_execute(GtkAction *action, gpointer user_data)
{
  do_airlift_for(3,NULL);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq3_show(GtkAction *action, gpointer user_data)
{
  airlift_queue_show(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq4_select(GtkAction *action, gpointer user_data)
{
  airlift_queue_copy(0, 4);
  update_airlift_menu(4);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq4_add(GtkAction *action, gpointer user_data)
{
  airlift_queue_cat(0, 4);
  update_airlift_menu(4);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq4_record(GtkAction *action, gpointer user_data)
{
  airlift_queue_move(4 ,0);
  update_airlift_menu(4);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq4_clear(GtkAction *action, gpointer user_data)
{
  airlift_queue_clear(4);
  update_airlift_menu(4);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq4_execute(GtkAction *action, gpointer user_data)
{
  do_airlift_for(4,NULL);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq4_show(GtkAction *action, gpointer user_data)
{
  airlift_queue_show(4);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq5_select(GtkAction *action, gpointer user_data)
{
  airlift_queue_copy(0, 5);
  update_airlift_menu(5);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq5_add(GtkAction *action, gpointer user_data)
{
  airlift_queue_cat(0, 5);
  update_airlift_menu(5);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq5_record(GtkAction *action, gpointer user_data)
{
  airlift_queue_move(5 ,0);
  update_airlift_menu(5);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq5_clear(GtkAction *action, gpointer user_data)
{
  airlift_queue_clear(5);
  update_airlift_menu(5);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq5_execute(GtkAction *action, gpointer user_data)
{
  do_airlift_for(5,NULL);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq5_show(GtkAction *action, gpointer user_data)
{
  airlift_queue_show(5);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq6_select(GtkAction *action, gpointer user_data)
{
  airlift_queue_copy(0, 6);
  update_airlift_menu(6);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq6_add(GtkAction *action, gpointer user_data)
{
  airlift_queue_cat(0, 6);
  update_airlift_menu(6);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq6_record(GtkAction *action, gpointer user_data)
{
  airlift_queue_move(6 ,0);
  update_airlift_menu(6);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq6_clear(GtkAction *action, gpointer user_data)
{
  airlift_queue_clear(6);
  update_airlift_menu(6);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq6_execute(GtkAction *action, gpointer user_data)
{
  do_airlift_for(6,NULL);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_airlift_aq6_show(GtkAction *action, gpointer user_data)
{
  airlift_queue_show(6);
}

/****************************************************************
  ...
*****************************************************************/
static const char *get_accelerator(const char *uname, int aq)
{
  if (aq != 0) {
    return NULL;
  }

  if (!strcmp(uname,"Workers")) {
    return "<Shift><Control>w";
  } else if (!strcmp(uname,"Engineers")) {
    return "<Shift><Control>e";
  } else if (!strcmp(uname,"Alpine Troops")) {
    return "<Shift><Control>l";
  } else if (!strcmp(uname,"Riflemen")) {
    return "<Shift><Control>r";
  } else if (!strcmp(uname,"Marines")) {
    return "<Shift><Control>m";
  } else if (!strcmp(uname,"Paratroopers")) {
    return "<Shift><Control>p";
  } else if  (!strcmp(uname,"Mech. Inf.")) {
    return "<Shift><Control>i";
  } else if (!strcmp(uname,"Cavalry")) {
    return "<Shift><Control>c";
  } else if (!strcmp(uname,"Armor")) {
    return "<Shift><Control>t";
  } else if (!strcmp(uname,"Artillery")) {
    return "<Shift><Control>a";
  } else if (!strcmp(uname,"Howitzer")) {
    return "<Shift><Control>h";
  } else if (!strcmp(uname,"Diplomat")) {
    return "<Shift><Control>d";
  } else if (!strcmp(uname,"Spy")) {
    return "<Shift><Control>s";
  } else if (!strcmp(uname,"Fanatics")) {
    return "<Shift><Control>f";
  }

  return NULL;
}

/****************************************************************
...
*****************************************************************/
static void callback_airlift_unit(GtkAction *unusedaction,
                                  GtkRadioAction *action,
                                  gpointer user_data)
{
  char buf[256];
  char actionname[256];
  GtkAction *mainaction;
  int aq = GPOINTER_TO_INT(user_data);
  Unit_Type_id utype = gtk_radio_action_get_current_value(action);

  airlift_queue_set_unit_type(aq, utype);

  my_snprintf(actionname, sizeof(actionname), "AIRLIFT_AQ%d_UNIT_TYPE", aq);
  mainaction = gtk_action_group_get_action(action_group_airlift,
                                           actionname);
  if (utype != U_LAST){
    my_snprintf(buf, sizeof(buf), "STOCK_UNIT_%d", utype);
    g_object_set(G_OBJECT(mainaction), "stock-id", buf, NULL);
  } else {
    g_object_set(G_OBJECT(mainaction), "stock-id", NULL , NULL);
  }
}

/****************************************************************
  Create the menu airlift for unit type
  Sort the unit type in 4 classes:
  * civil units
  * attack units
  * defense units
  * fast units
  *****************************************************************/
#define CLASS_NUM 4
static const char *load_menu_airlift_unit(int aq, GtkAction *paction)
{
  GtkRadioActionEntry radio_entries_airlift_unit[U_LAST];
  Unit_Type_id classes[CLASS_NUM][U_LAST];
  int index[CLASS_NUM] = {0, 0, 0, 0};
  int i, j, k = 0;
  char name[U_LAST][256];
  char accel[U_LAST][256];
  char buf[256];
  static char retbuf[2048];
  const char *bufaccel;
  GdkPixbuf *pixbuf;
  GdkPixbuf *pixbuf1;
  GdkPixbuf *pixbuf2;  
  GdkColormap *colormap;
  GdkScreen *screen;
  GtkIconSet* icon;
  GtkIconFactory *icon_factory;

  screen = gdk_screen_get_default();
  colormap = gdk_screen_get_default_colormap(screen);

  unit_type_iterate(ut) {
    struct unit_type *utype = get_unit_type(ut);

    if (utype->move_type != LAND_MOVING || unit_type_flag(ut, F_NOBUILD)) {
      /* Land unit and buildable units only */
      continue;
    }

    if (utype->attack_strength == 0) {
      i = 0;
    } else if (unit_has_role(ut, L_DEFEND_GOOD)) {
      i = 2;
    } else if (unit_has_role(ut, L_ATTACK_FAST)) {
      i = 3;
    } else if (unit_has_role(ut, L_ATTACK_STRONG)) {
      i = 1;
    } else if (utype->attack_strength > utype->defense_strength) {
      if (utype->move_rate > SINGLE_MOVE
          && utype->attack_strength < 4 * utype->defense_strength) {
        i = 3;
      } else {
        i = 1;
      }
    } else {
      i = 2;
    }

    classes[i][index[i]++] = ut;
    k++;
  } unit_type_iterate_end;

  my_snprintf(buf, sizeof(buf), "%s_NONE", gtk_action_get_name(paction));

  my_snprintf(retbuf, sizeof(retbuf),
              "<menuitem action=\"%s\" />\n", buf);

  sz_strlcpy(name[0], buf);
  radio_entries_airlift_unit[0].name = name[0];
  radio_entries_airlift_unit[0].stock_id = NULL;
  radio_entries_airlift_unit[0].label = _("None");
  radio_entries_airlift_unit[0].accelerator = NULL;
  radio_entries_airlift_unit[0].tooltip = _("None");
  radio_entries_airlift_unit[0].value = U_LAST;
  airlift_queue_set_menu_name(aq, U_LAST, name[0]);

  for (k = i = 0; i < CLASS_NUM; i++) {
    if (index[i] == 0) {
      continue; /* No unit type in this class */
    }
    
    cat_snprintf(retbuf, sizeof(retbuf), "<separator/>\n");
    
    for (j = 0; j < index[i]; j++, k++) {
      struct Sprite *gsprite;

      icon_factory = gtk_icon_factory_new();
      if ((gsprite = get_unit_type(classes[i][j])->sprite)) {
        my_snprintf(buf, sizeof(buf), "STOCK_UNIT_%d", classes[i][j]);
        
        pixbuf = NULL;
        pixbuf = gdk_pixbuf_get_from_drawable(pixbuf,
                                              gsprite->pixmap,
                                              colormap,
                                              0, 0, 0, 0,
                                              gsprite->width,
                                              gsprite->height);

        pixbuf1 = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 
                                 gsprite->width, gsprite->width);

        gdk_pixbuf_fill(pixbuf1, 0xffffff00);

        gdk_pixbuf_composite(pixbuf, pixbuf1, 0, 0, 
                             gsprite->width, gsprite->width, -25, -20,
                             1.8,
                             1.8,
                             GDK_INTERP_HYPER, 255);
        
        pixbuf2 = gdk_pixbuf_add_alpha(pixbuf1, TRUE, 255, 255, 255);

        icon = gtk_icon_set_new_from_pixbuf(pixbuf2);
        gtk_icon_factory_add(icon_factory, buf, icon);

        g_object_unref(pixbuf2);
        g_object_unref(pixbuf1);
        g_object_unref(pixbuf);
      }

      gtk_icon_factory_add_default(icon_factory);

      my_snprintf(buf, sizeof(buf), "%s_%d",
                  gtk_action_get_name(paction), classes[i][j]);
      cat_snprintf(retbuf, sizeof(retbuf), "<menuitem action=\"%s\" />\n", buf);

      sz_strlcpy(name[k + 1], buf);
      radio_entries_airlift_unit[k+1].name = name[k + 1];
      radio_entries_airlift_unit[k+1].stock_id = NULL;
      
      radio_entries_airlift_unit[k+1].label = unit_name(classes[i][j]);
      if (( bufaccel = get_accelerator(unit_name_orig(classes[i][j]), aq))) {
        sz_strlcpy(accel[k + 1], bufaccel);
        radio_entries_airlift_unit[k+1].accelerator = accel[k + 1];
      } else {
        radio_entries_airlift_unit[k+1].accelerator = NULL;
      }
      radio_entries_airlift_unit[k+1].tooltip = unit_name(classes[i][j]);
      radio_entries_airlift_unit[k+1].value = classes[i][j];
      airlift_queue_set_menu_name(aq, classes[i][j], name[k + 1]);
    }
  }
 
  my_snprintf(buf, sizeof(buf), "RadioGroupAirlift%d", aq);
  radio_action_group_airlift_unit[aq] = gtk_action_group_new(buf);
  gtk_action_group_set_translation_domain(radio_action_group_airlift_unit[aq],
                                          textdomain(PACKAGE));
  gtk_action_group_add_radio_actions(radio_action_group_airlift_unit[aq],
                                     radio_entries_airlift_unit,
                                     k+1,
                                     -1,
                                     G_CALLBACK(callback_airlift_unit),
                                     GINT_TO_POINTER(aq));
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     radio_action_group_airlift_unit[aq], 0);

  freelog(LOG_MENU, "Strlen of buf = %d; Size of load_menu_airlift_unit %d",
          strlen(retbuf), sizeof(retbuf));

  return retbuf;
}

/****************************************************************
  ...
*****************************************************************/
static const char *load_menu_airlift(void)
{
  static char buf[16384];

  GtkActionEntry entries_airlift[] = {
    {"AIRLIFT", NULL, N_("A_irlift"), NULL, NULL, NULL},
    {"AIRLIFT_AQ0_UNIT_TYPE", NULL, N_("Airlift unit _type"), NULL, NULL, NULL},
    {"AIRLIFT_SET_AIRLIFT_DEST", NULL, N_("Set airlift _destination"),
     "<Shift>y", N_("Set airlift _destination"),
     G_CALLBACK(callback_airlift_set_airlift_dest)},
    {"AIRLIFT_SET_AIRLIFT_SRC", NULL, N_("Set airlift _source"),
     "<Shift>z", N_("Set airlift _source"),
     G_CALLBACK(callback_airlift_set_airlift_src)},
    {"AIRLIFT_AIRLIFT_SELECT_AIRPORT_CITIES", NULL,
     N_("_Add all cities with airport in airlift queue"),
     "<Contorl><Shift>z", N_("_Add all cities with airport in airlift queue"),
     G_CALLBACK(callback_airlift_airlift_select_airport_cities)},
    {"AIRLIFT_CLEAR_AIRLIFT_QUEUE", NULL, N_("_Clear airlift queue"),
     "<Shift>u", N_("_Clear airlift queue"),
     G_CALLBACK(callback_airlift_clear_airlift_queue)},
    {"AIRLIFT_SHOW_CITIES_IN_AIRLIFT_QUEUE", NULL,
     N_("Show cities in airlift _queue"),
     NULL, N_("Show cities in airlift _queue"),
     G_CALLBACK(callback_airlift_show_cities_in_airlift_queue)},
    {"AIRLIFT_AQ1", NULL, N_("Airlift queue") " _4", NULL, NULL, NULL},
    {"AIRLIFT_AQ1_SELECT", NULL, N_("_Select"),
     "<Alt>4", N_("_Select"), G_CALLBACK(callback_airlift_aq1_select)},
    {"AIRLIFT_AQ1_ADD", NULL, N_("_Add to current selection"),
     "<Shift><Alt>4", N_("_Add to current selection"),
     G_CALLBACK(callback_airlift_aq1_add)},
    {"AIRLIFT_AQ1_RECORD", NULL, N_("_Record"),
     "<Control><Alt>4", N_("_Record"), G_CALLBACK(callback_airlift_aq1_record)},
    {"AIRLIFT_AQ1_CLEAR", NULL, N_("_Clear"),
     NULL, N_("_Clear"), G_CALLBACK(callback_airlift_aq1_clear)},
    {"AIRLIFT_AQ1_EXECUTE", NULL, N_("_Execute"),
     "<Control><Shift><Alt>4", N_("_Execute"),
     G_CALLBACK(callback_airlift_aq1_execute)},
    {"AIRLIFT_AQ1_SHOW", NULL, N_("_Show"),
     NULL, N_("_Show"), G_CALLBACK(callback_airlift_aq1_show)},
    {"AIRLIFT_AQ1_UNIT_TYPE", NULL, N_("Airlift unit _type"), NULL, NULL, NULL},
    {"AIRLIFT_AQ2", NULL, N_("Airlift queue") " _5", NULL, NULL, NULL},
    {"AIRLIFT_AQ2_SELECT", NULL, N_("_Select"),
     "<Alt>5", N_("_Select"), G_CALLBACK(callback_airlift_aq2_select)},
    {"AIRLIFT_AQ2_ADD", NULL, N_("_Add to current selection"),
     "<Shift><Alt>5", N_("_Add to current selection"),
     G_CALLBACK(callback_airlift_aq2_add)},
    {"AIRLIFT_AQ2_RECORD", NULL, N_("_Record"),
     "<Control><Alt>5", N_("_Record"), G_CALLBACK(callback_airlift_aq2_record)},
    {"AIRLIFT_AQ2_CLEAR", NULL, N_("_Clear"),
     NULL, N_("_Clear"), G_CALLBACK(callback_airlift_aq2_clear)},
    {"AIRLIFT_AQ2_EXECUTE", NULL, N_("_Execute"),
     "<Control><Shift><Alt>5", N_("_Execute"),
     G_CALLBACK(callback_airlift_aq2_execute)},
    {"AIRLIFT_AQ2_SHOW", NULL, N_("_Show"),
     NULL, N_("_Show"), G_CALLBACK(callback_airlift_aq2_show)},
    {"AIRLIFT_AQ2_UNIT_TYPE", NULL, N_("Airlift unit _type"), NULL, NULL, NULL},
    {"AIRLIFT_AQ3", NULL, N_("Airlift queue") " _6", NULL, NULL, NULL},
    {"AIRLIFT_AQ3_SELECT", NULL, N_("_Select"),
     "<Alt>6", N_("_Select"), G_CALLBACK(callback_airlift_aq3_select)},
    {"AIRLIFT_AQ3_ADD", NULL, N_("_Add to current selection"),
     "<Shift><Alt>6", N_("_Add to current selection"),
     G_CALLBACK(callback_airlift_aq3_add)},
    {"AIRLIFT_AQ3_RECORD", NULL, N_("_Record"),
     "<Control><Alt>6", N_("_Record"), G_CALLBACK(callback_airlift_aq3_record)},
    {"AIRLIFT_AQ3_CLEAR", NULL, N_("_Clear"),
     NULL, N_("_Clear"), G_CALLBACK(callback_airlift_aq3_clear)},
    {"AIRLIFT_AQ3_EXECUTE", NULL, N_("_Execute"),
     "<Control><Shift><Alt>6", N_("_Execute"),
     G_CALLBACK(callback_airlift_aq3_execute)},
    {"AIRLIFT_AQ3_SHOW", NULL, N_("_Show"),
     NULL, N_("_Show"), G_CALLBACK(callback_airlift_aq3_show)},
    {"AIRLIFT_AQ3_UNIT_TYPE", NULL, N_("Airlift unit _type"), NULL, NULL, NULL},
    {"AIRLIFT_AQ4", NULL, N_("Airlift queue") " _7", NULL, NULL, NULL},
    {"AIRLIFT_AQ4_SELECT", NULL, N_("_Select"),
     "<Alt>7", N_("_Select"), G_CALLBACK(callback_airlift_aq4_select)},
    {"AIRLIFT_AQ4_ADD", NULL, N_("_Add to current selection"),
     "<Shift><Alt>7", N_("_Add to current selection"),
     G_CALLBACK(callback_airlift_aq4_add)},
    {"AIRLIFT_AQ4_RECORD", NULL, N_("_Record"),
     "<Control><Alt>7", N_("_Record"), G_CALLBACK(callback_airlift_aq4_record)},
    {"AIRLIFT_AQ4_CLEAR", NULL, N_("_Clear"),
     NULL, N_("_Clear"), G_CALLBACK(callback_airlift_aq4_clear)},
    {"AIRLIFT_AQ4_EXECUTE", NULL, N_("_Execute"),
     "<Control><Shift><Alt>7", N_("_Execute"),
     G_CALLBACK(callback_airlift_aq4_execute)},
    {"AIRLIFT_AQ4_SHOW", NULL, N_("_Show"),
     NULL, N_("_Show"), G_CALLBACK(callback_airlift_aq4_show)},
    {"AIRLIFT_AQ4_UNIT_TYPE", NULL, N_("Airlift unit _type"),
     NULL, NULL, NULL},
    {"AIRLIFT_AQ5", NULL, N_("Airlift queue") " _8",
     NULL, NULL, NULL},
    {"AIRLIFT_AQ5_SELECT", NULL, N_("_Select"),
     "<Alt>8", N_("_Select"), G_CALLBACK(callback_airlift_aq5_select)},
    {"AIRLIFT_AQ5_ADD", NULL, N_("_Add to current selection"),
     "<Shift><Alt>8", N_("_Add to current selection"),
     G_CALLBACK(callback_airlift_aq5_add)},
    {"AIRLIFT_AQ5_RECORD", NULL, N_("_Record"),
     "<Control><Alt>8", N_("_Record"), G_CALLBACK(callback_airlift_aq5_record)},
    {"AIRLIFT_AQ5_CLEAR", NULL, N_("_Clear"),
     NULL, N_("_Clear"), G_CALLBACK(callback_airlift_aq5_clear)},
    {"AIRLIFT_AQ5_EXECUTE", NULL, N_("_Execute"),
     "<Control><Shift><Alt>8", N_("_Execute"),
     G_CALLBACK(callback_airlift_aq5_execute)},
    {"AIRLIFT_AQ5_SHOW", NULL, N_("_Show"),
     NULL, N_("_Show"), G_CALLBACK(callback_airlift_aq5_show)},
    {"AIRLIFT_AQ5_UNIT_TYPE", NULL, N_("Airlift unit _type"), NULL, NULL, NULL},
    {"AIRLIFT_AQ6", NULL, N_("Airlift queue") " _9", NULL, NULL, NULL},
    {"AIRLIFT_AQ6_SELECT", NULL, N_("_Select"),
     "<Alt>9", N_("_Select"), G_CALLBACK(callback_airlift_aq6_select)},
    {"AIRLIFT_AQ6_ADD", NULL, N_("_Add to current selection"),
     "<Shift><Alt>9", N_("_Add to current selection"),
     G_CALLBACK(callback_airlift_aq6_add)},
    {"AIRLIFT_AQ6_RECORD", NULL, N_("_Record"),
     "<Control><Alt>9", N_("_Record"), G_CALLBACK(callback_airlift_aq6_record)},
    {"AIRLIFT_AQ6_CLEAR", NULL, N_("_Clear"),
     NULL, N_("_Clear"), G_CALLBACK(callback_airlift_aq6_clear)},
    {"AIRLIFT_AQ6_EXECUTE", NULL, N_("_Execute"),
     "<Control><Shift><Alt>9", N_("_Execute"),
     G_CALLBACK(callback_airlift_aq6_execute)},
    {"AIRLIFT_AQ6_SHOW", NULL, N_("_Show"),
     NULL, N_("_Show"), G_CALLBACK(callback_airlift_aq6_show)},
    {"AIRLIFT_AQ6_UNIT_TYPE", NULL, N_("Airlift unit _type"), NULL, NULL, NULL}
  };

  action_group_airlift = gtk_action_group_new("GroupAirlift");
  gtk_action_group_set_translation_domain(action_group_airlift,
                                          textdomain(PACKAGE));
  gtk_action_group_add_actions(action_group_airlift, entries_airlift,
                               G_N_ELEMENTS(entries_airlift),
                               NULL);
  gtk_ui_manager_insert_action_group(main_uimanager, action_group_airlift, 0);

  my_snprintf(buf, sizeof(buf),
              "<menu action=\"AIRLIFT\">\n"
              "<menu action=\"AIRLIFT_AQ0_UNIT_TYPE\">\n"
              "%s"
              "</menu>\n"
              "<separator/>\n"
              "<menuitem action=\"AIRLIFT_SET_AIRLIFT_DEST\" />\n"
              "<menuitem action=\"AIRLIFT_SET_AIRLIFT_SRC\" />\n"
              "<menuitem action=\"AIRLIFT_AIRLIFT_SELECT_AIRPORT_CITIES\" />\n"
              "<menuitem action=\"AIRLIFT_CLEAR_AIRLIFT_QUEUE\" />\n"
              "<menuitem action=\"AIRLIFT_SHOW_CITIES_IN_AIRLIFT_QUEUE\" />\n"
              "<separator/>\n",
              load_menu_airlift_unit(0,
                                     gtk_action_group_get_action(action_group_airlift,
                                                                 "AIRLIFT_AQ0_UNIT_TYPE")));

  cat_snprintf(buf, sizeof(buf),
               "<menu action=\"AIRLIFT_AQ1\">\n"
               "<menuitem action=\"AIRLIFT_AQ1_SELECT\" />\n"
               "<menuitem action=\"AIRLIFT_AQ1_ADD\" />\n"
               "<menuitem action=\"AIRLIFT_AQ1_RECORD\" />\n"
               "<menuitem action=\"AIRLIFT_AQ1_CLEAR\" />\n"
               "<menuitem action=\"AIRLIFT_AQ1_EXECUTE\" />\n"
               "<menuitem action=\"AIRLIFT_AQ1_SHOW\" />\n"
               "<menu action=\"AIRLIFT_AQ1_UNIT_TYPE\">\n"
               "%s"
               "</menu>\n"
               "</menu>\n",
               load_menu_airlift_unit(1,
                                      gtk_action_group_get_action(action_group_airlift,
                                                                  "AIRLIFT_AQ1_UNIT_TYPE")));

  cat_snprintf(buf, sizeof(buf),
               "<menu action=\"AIRLIFT_AQ2\">\n"
               "<menuitem action=\"AIRLIFT_AQ2_SELECT\" />\n"
               "<menuitem action=\"AIRLIFT_AQ2_ADD\" />\n"
               "<menuitem action=\"AIRLIFT_AQ2_RECORD\" />\n"
               "<menuitem action=\"AIRLIFT_AQ2_CLEAR\" />\n"
               "<menuitem action=\"AIRLIFT_AQ2_EXECUTE\" />\n"
               "<menuitem action=\"AIRLIFT_AQ2_SHOW\" />\n"
               "<menu action=\"AIRLIFT_AQ2_UNIT_TYPE\">\n"
               "%s"
               "</menu>\n"
               "</menu>\n",
               load_menu_airlift_unit(2,
                                      gtk_action_group_get_action(action_group_airlift,
                                                                  "AIRLIFT_AQ2_UNIT_TYPE")));

  cat_snprintf(buf, sizeof(buf),
               "<menu action=\"AIRLIFT_AQ3\">\n"
               "<menuitem action=\"AIRLIFT_AQ3_SELECT\" />\n"
               "<menuitem action=\"AIRLIFT_AQ3_ADD\" />\n"
               "<menuitem action=\"AIRLIFT_AQ3_RECORD\" />\n"
               "<menuitem action=\"AIRLIFT_AQ3_CLEAR\" />\n"
               "<menuitem action=\"AIRLIFT_AQ3_EXECUTE\" />\n"
               "<menuitem action=\"AIRLIFT_AQ3_SHOW\" />\n"
               "<menu action=\"AIRLIFT_AQ3_UNIT_TYPE\">\n"
               "%s"
               "</menu>\n"
               "</menu>\n",
               load_menu_airlift_unit(3,
                                      gtk_action_group_get_action(action_group_airlift,
                                                                  "AIRLIFT_AQ3_UNIT_TYPE")));

  cat_snprintf(buf, sizeof(buf),
               "<menu action=\"AIRLIFT_AQ4\">\n"
               "<menuitem action=\"AIRLIFT_AQ4_SELECT\" />\n"
               "<menuitem action=\"AIRLIFT_AQ4_ADD\" />\n"
               "<menuitem action=\"AIRLIFT_AQ4_RECORD\" />\n"
               "<menuitem action=\"AIRLIFT_AQ4_CLEAR\" />\n"
               "<menuitem action=\"AIRLIFT_AQ4_EXECUTE\" />\n"
               "<menuitem action=\"AIRLIFT_AQ4_SHOW\" />\n"
               "<menu action=\"AIRLIFT_AQ4_UNIT_TYPE\">\n"
               "%s"
               "</menu>\n"
               "</menu>\n",
               load_menu_airlift_unit(4,
                                      gtk_action_group_get_action(action_group_airlift,
                                                                  "AIRLIFT_AQ4_UNIT_TYPE")));

  cat_snprintf(buf, sizeof(buf),
               "<menu action=\"AIRLIFT_AQ5\">\n"
               "<menuitem action=\"AIRLIFT_AQ5_SELECT\" />\n"
               "<menuitem action=\"AIRLIFT_AQ5_ADD\" />\n"
               "<menuitem action=\"AIRLIFT_AQ5_RECORD\" />\n"
               "<menuitem action=\"AIRLIFT_AQ5_CLEAR\" />\n"
               "<menuitem action=\"AIRLIFT_AQ5_EXECUTE\" />\n"
               "<menuitem action=\"AIRLIFT_AQ5_SHOW\" />\n"
               "<menu action=\"AIRLIFT_AQ5_UNIT_TYPE\">\n"
               "%s"
               "</menu>\n"
               "</menu>\n",
               load_menu_airlift_unit(5,
                                      gtk_action_group_get_action(action_group_airlift,
                                                                  "AIRLIFT_AQ5_UNIT_TYPE")));
  cat_snprintf(buf, sizeof(buf),
               "<menu action=\"AIRLIFT_AQ6\">\n"
               "<menuitem action=\"AIRLIFT_AQ6_SELECT\" />\n"
               "<menuitem action=\"AIRLIFT_AQ6_ADD\" />\n"
               "<menuitem action=\"AIRLIFT_AQ6_RECORD\" />\n"
               "<menuitem action=\"AIRLIFT_AQ6_CLEAR\" />\n"
               "<menuitem action=\"AIRLIFT_AQ6_EXECUTE\" />\n"
               "<menuitem action=\"AIRLIFT_AQ6_SHOW\" />\n"
               "<menu action=\"AIRLIFT_AQ6_UNIT_TYPE\">\n"
               "%s"
               "</menu>\n"
               "</menu>\n"
               "</menu>\n",
               load_menu_airlift_unit(6,
                                      gtk_action_group_get_action(action_group_airlift,
                                                                  "AIRLIFT_AQ6_UNIT_TYPE")));

  freelog(LOG_MENU, "Strlen of buf = %d; Size of load_menu_airlift %d",
          strlen(buf), sizeof(buf));

  return buf;
}

/****************************************************************
  ...
*****************************************************************/
static void callback_auto_caravan_my_ai_trade_city(GtkAction *action,
                                                   gpointer user_data)
{
  key_my_ai_trade_city();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_auto_caravan_my_ai_clear_trade_city(GtkAction *action,
                                                         gpointer user_data)
{
  clear_my_ai_trade_cities();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_auto_caravan_my_ai_trade_recalculate(GtkAction *action,
                                                          gpointer user_data)
{
  recalculate_trade_plan();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_auto_caravan_my_ai_estimate_trade(GtkAction *action,
                                                       gpointer user_data)
{
  calculate_trade_estimation();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_auto_caravan_my_ai_show_free_slots(GtkAction *action,
                                                        gpointer user_data)
{
  show_free_slots_in_trade_plan();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_auto_caravan_my_ai_show_trade_cities(GtkAction *action,
                                                          gpointer user_data)
{
  show_cities_in_trade_plan();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_auto_caravan_my_ai_trade_with(GtkAction *action,
                                                   gpointer user_data)
{
  key_my_ai_trade();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_auto_caravan_my_ai_caravan(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_iterate(TRUE,punit)
    {
      my_ai_caravan(punit);
    } multi_select_iterate_end;
}

/****************************************************************
  ...
*****************************************************************/
static void callback_auto_caravan_my_ai_trade_execute(GtkAction *action,
                                                      gpointer user_data)
{
  my_ai_trade_route_execute_all();
}

/****************************************************************
  ...
*****************************************************************/
static const char *load_menu_auto_caravan(void)
{
  static char buf[1024];

  GtkActionEntry entries_auto_caravan[] = {
    {"AUTO_CARAVAN", NULL, N_("Auto _Caravan"), NULL, NULL, NULL},
    {"AUTO_CARAVAN_MY_AI_TRADE_CITY", NULL, N_("_Add a city in trade plan"),
     "<Shift>a", N_("_Add a city in trade plan"),
     G_CALLBACK(callback_auto_caravan_my_ai_trade_city)},
    {"AUTO_CARAVAN_MY_AI_CLEAR_TRADE_CITY", NULL,
     N_("_Clear city list for trade plan"),
     NULL, N_("_Clear city list for trade plan"),
     G_CALLBACK(callback_auto_caravan_my_ai_clear_trade_city)},
    {"AUTO_CARAVAN_MY_AI_TRADE_RECALCULATE", NULL,
     N_("_Recalculate trade plan"),
     NULL, N_("_Recalculate trade plan"),
     G_CALLBACK(callback_auto_caravan_my_ai_trade_recalculate)},
    {"AUTO_CARAVAN_MY_AI_ESTIMATE_TRADE", NULL, N_("Show trade _estimation"),
     NULL, N_("Show trade _estimation"),
     G_CALLBACK(callback_auto_caravan_my_ai_estimate_trade)},
    {"AUTO_CARAVAN_MY_AI_SHOW_FREE_SLOTS", NULL,
     N_("Show the trade route _free slots"),
     NULL, N_("Show the trade route _free slots"),
     G_CALLBACK(callback_auto_caravan_my_ai_show_free_slots)},
    {"AUTO_CARAVAN_MY_AI_SHOW_TRADE_CITIES", NULL,
     N_("Show cities in _trade plan"),
     NULL, N_("Show cities in _trade plan"),
     G_CALLBACK(callback_auto_caravan_my_ai_show_trade_cities)},
    {"AUTO_CARAVAN_MY_AI_TRADE_WITH", NULL, N_("_Set caravan destination"),
     "j", N_("_Set caravan destination"), 
     G_CALLBACK(callback_auto_caravan_my_ai_trade_with)},
    {"AUTO_CARAVAN_MY_AI_CARAVAN", NULL, N_("Auto_matic caravan orders"),
     "<Control>j", N_("Auto_matic caravan orders"),
     G_CALLBACK(callback_auto_caravan_my_ai_caravan)},
    {"AUTO_CARAVAN_MY_AI_TRADE_EXECUTE", NULL,
     N_("_Execute all trade route orders"),
     "<Shift>j", N_("_Execute all trade route orders"),
     G_CALLBACK(callback_auto_caravan_my_ai_trade_execute)}
  };

  action_group_auto_caravan = gtk_action_group_new("GroupAutoCaravan");
  gtk_action_group_set_translation_domain(action_group_auto_caravan,
                                          textdomain(PACKAGE));
  gtk_action_group_add_actions(action_group_auto_caravan, entries_auto_caravan,
                               G_N_ELEMENTS(entries_auto_caravan),
                               NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     action_group_auto_caravan, 0);

  my_snprintf(buf, sizeof(buf), "%s",
              "<menu action=\"AUTO_CARAVAN\">\n"
              "<menuitem action=\"AUTO_CARAVAN_MY_AI_TRADE_CITY\" />\n"
              "<menuitem action=\"AUTO_CARAVAN_MY_AI_CLEAR_TRADE_CITY\" />\n"
              "<menuitem action=\"AUTO_CARAVAN_MY_AI_TRADE_RECALCULATE\" />\n"
              "<separator/>\n"
              "<menuitem action=\"AUTO_CARAVAN_MY_AI_ESTIMATE_TRADE\" />\n"
              "<menuitem action=\"AUTO_CARAVAN_MY_AI_SHOW_FREE_SLOTS\" />\n"
              "<menuitem action=\"AUTO_CARAVAN_MY_AI_SHOW_TRADE_CITIES\" />\n"
              "<separator/>\n"
              "<menuitem action=\"AUTO_CARAVAN_MY_AI_TRADE_WITH\" />\n"
              "<menuitem action=\"AUTO_CARAVAN_MY_AI_CARAVAN\" />\n"
              "<menuitem action=\"AUTO_CARAVAN_MY_AI_TRADE_EXECUTE\" />\n"
              "</menu>\n");

  freelog(LOG_MENU, "Strlen of buf = %d; Size of load_menu_auto_caravan %d",
          strlen(buf), sizeof(buf));

  return buf;
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms_select(GtkAction *action,
                                               gpointer user_data)
{
  multi_select_select();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms_active_all(GtkAction *action,
                                                   gpointer user_data)
{
  multi_select_active_all(0);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms_clear(GtkAction *action,
                                              gpointer user_data)
{
  multi_select_clear(0);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_my_ai_spread(GtkAction *action,
                                                  gpointer user_data)
{
  if (tiles_hilited_cities) {
    key_select_rally_point();
  } else {
    my_ai_spread_execute();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms1_select(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(0, 1);
  update_multi_selection_menu(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms1_add(GtkAction *action,
                                             gpointer user_data)
{
  multi_select_cat(0, 1);
  update_multi_selection_menu(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms1_record(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(1, 0);
  update_multi_selection_menu(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms1_clear(GtkAction *action,
                                               gpointer user_data)
{
  multi_select_clear(1);
  update_multi_selection_menu(1);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms2_select(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(0, 2);
  update_multi_selection_menu(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms2_add(GtkAction *action,
                                             gpointer user_data)
{
  multi_select_cat(0, 2);
  update_multi_selection_menu(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms2_record(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(2, 0);
  update_multi_selection_menu(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms2_clear(GtkAction *action,
                                               gpointer user_data)
{
  multi_select_clear(2);
  update_multi_selection_menu(2);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms3_select(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(0, 3);
  update_multi_selection_menu(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms3_add(GtkAction *action,
                                             gpointer user_data)
{
  multi_select_cat(0, 3);
  update_multi_selection_menu(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms3_record(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(3, 0);
  update_multi_selection_menu(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms3_clear(GtkAction *action,
                                               gpointer user_data)
{
  multi_select_clear(3);
  update_multi_selection_menu(3);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms4_select(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(0, 4);
  update_multi_selection_menu(4);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms4_add(GtkAction *action,
                                             gpointer user_data)
{
  multi_select_cat(0, 4);
  update_multi_selection_menu(4);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms4_record(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(4, 0);
  update_multi_selection_menu(4);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms4_clear(GtkAction *action,
                                               gpointer user_data)
{
  multi_select_clear(4);
  update_multi_selection_menu(4);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms5_select(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(0, 5);
  update_multi_selection_menu(5);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms5_add(GtkAction *action,
                                             gpointer user_data)
{
  multi_select_cat(0, 5);
  update_multi_selection_menu(5);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms5_record(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(5, 0);
  update_multi_selection_menu(5);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms5_clear(GtkAction *action,
                                               gpointer user_data)
{
  multi_select_clear(5);
  update_multi_selection_menu(5);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms6_select(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(0, 6);
  update_multi_selection_menu(6);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms6_add(GtkAction *action,
                                             gpointer user_data)
{
  multi_select_cat(0, 6);
  update_multi_selection_menu(6);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms6_record(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(6, 0);
  update_multi_selection_menu(6);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms6_clear(GtkAction *action,
                                               gpointer user_data)
{
  multi_select_clear(6);
  update_multi_selection_menu(6);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms7_select(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(0, 7);
  update_multi_selection_menu(7);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms7_add(GtkAction *action,
                                             gpointer user_data)
{
  multi_select_cat(0, 7);
  update_multi_selection_menu(7);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms7_record(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(7, 0);
  update_multi_selection_menu(7);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms7_clear(GtkAction *action,
                                               gpointer user_data)
{
  multi_select_clear(7);
  update_multi_selection_menu(7);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms8_select(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(0, 8);
  update_multi_selection_menu(8);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms8_add(GtkAction *action,
                                             gpointer user_data)
{
  multi_select_cat(0, 8);
  update_multi_selection_menu(8);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms8_record(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(8, 0);
  update_multi_selection_menu(8);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms8_clear(GtkAction *action,
                                               gpointer user_data)
{
  multi_select_clear(8);
  update_multi_selection_menu(8);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms9_select(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(0, 9);
  update_multi_selection_menu(9);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms9_add(GtkAction *action,
                                             gpointer user_data)
{
  multi_select_cat(0, 9);
  update_multi_selection_menu(9);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms9_record(GtkAction *action,
                                                gpointer user_data)
{
  multi_select_copy(9, 0);
  update_multi_selection_menu(9);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_ms9_clear(GtkAction *action,
                                               gpointer user_data)
{
  multi_select_clear(9);
  update_multi_selection_menu(9);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_my_ai_spread_airport(GtkToggleAction *action,
                                                          gpointer user_data)
{
  if (spread_airport_cities ^
      gtk_toggle_action_get_active(action)) {
    key_toggle_spread_airport();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_my_ai_spread_ally(GtkToggleAction *action,
                                                       gpointer user_data)
{
  if (spread_allied_cities ^ gtk_toggle_action_get_active(action)) {
    key_toggle_spread_ally();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_inclusive_all(GtkToggleAction *action,
                                                   gpointer user_data)
{
  if (!!(multi_select_inclusive_filter & FILTER_ALL) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_inclusive_filter, FILTER_ALL);
    update_multi_selection_inclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_inclusive_new(GtkToggleAction *action,
                                                   gpointer user_data)
{
  if (!!(multi_select_inclusive_filter & FILTER_NEW) ^
      gtk_toggle_action_get_active(action))
    {
      filter_change(&multi_select_inclusive_filter, FILTER_NEW);
      update_multi_selection_inclusive_filter_menu();
      update_unit_info_label(get_unit_in_focus());
    }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_inclusive_fortified(GtkToggleAction *action,
                                                         gpointer user_data)
{
  if (!!(multi_select_inclusive_filter & FILTER_FORTIFIED) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_inclusive_filter, FILTER_FORTIFIED);
    update_multi_selection_inclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_inclusive_sentried(GtkToggleAction *action,
                                                        gpointer user_data)
{
  if (!!(multi_select_inclusive_filter & FILTER_SENTRIED) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_inclusive_filter, FILTER_SENTRIED);
    update_multi_selection_inclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_inclusive_veteran(GtkToggleAction *action,
                                                       gpointer user_data)
{
  if (!!(multi_select_inclusive_filter & FILTER_VETERAN) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_inclusive_filter, FILTER_VETERAN);
    update_multi_selection_inclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_inclusive_auto(GtkToggleAction *action,
                                                    gpointer user_data)
{
  if (!!(multi_select_inclusive_filter & FILTER_AUTO) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_inclusive_filter, FILTER_AUTO);
    update_multi_selection_inclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_inclusive_idle(GtkToggleAction *action,
                                                    gpointer user_data)
{
  if (!!(multi_select_inclusive_filter & FILTER_IDLE) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_inclusive_filter, FILTER_IDLE);
    update_multi_selection_inclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_inclusive_able_to_move(GtkToggleAction *action,
                                                            gpointer user_data)
{
  if (!!(multi_select_inclusive_filter & FILTER_ABLE_TO_MOVE) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_inclusive_filter, FILTER_ABLE_TO_MOVE);
    update_multi_selection_inclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_inclusive_military(GtkToggleAction *action,
                                                        gpointer user_data)
{
  if (!!(multi_select_inclusive_filter & FILTER_MILITARY) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_inclusive_filter, FILTER_MILITARY);
    update_multi_selection_inclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_inclusive_off(GtkToggleAction *action,
                                                   gpointer user_data)
{
  if (!!(multi_select_inclusive_filter & FILTER_OFF) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_inclusive_filter, FILTER_OFF);
    update_multi_selection_inclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_exclusive_all(GtkToggleAction *action,
                                                   gpointer user_data)
{
  if (!!(multi_select_exclusive_filter & FILTER_ALL) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_exclusive_filter, FILTER_ALL);
    update_multi_selection_exclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_exclusive_new(GtkToggleAction *action,
                                                   gpointer user_data)
{
  if (!!(multi_select_exclusive_filter & FILTER_NEW) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_exclusive_filter, FILTER_NEW);
    update_multi_selection_exclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_exclusive_fortified(GtkToggleAction *action,
                                                         gpointer user_data)
{
  if (!!(multi_select_exclusive_filter & FILTER_FORTIFIED) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_exclusive_filter, FILTER_FORTIFIED);
    update_multi_selection_exclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_exclusive_sentried(GtkToggleAction *action,
                                                        gpointer user_data)
{
  if (!!(multi_select_exclusive_filter & FILTER_SENTRIED) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_exclusive_filter, FILTER_SENTRIED);
    update_multi_selection_exclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_exclusive_veteran(GtkToggleAction *action,
                                                       gpointer user_data)
{
  if (!!(multi_select_exclusive_filter & FILTER_VETERAN) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_exclusive_filter, FILTER_VETERAN);
    update_multi_selection_exclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_exclusive_auto(GtkToggleAction *action,
                                                    gpointer user_data)
{
  if (!!(multi_select_exclusive_filter & FILTER_AUTO) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_exclusive_filter, FILTER_AUTO);
    update_multi_selection_exclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_exclusive_idle(GtkToggleAction *action,
                                                    gpointer user_data)
{
  if (!!(multi_select_exclusive_filter & FILTER_IDLE) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_exclusive_filter, FILTER_IDLE);
    update_multi_selection_exclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_exclusive_able_to_move(GtkToggleAction *action,
                                                            gpointer user_data)
{
  if (!!(multi_select_exclusive_filter & FILTER_ABLE_TO_MOVE) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_exclusive_filter, FILTER_ABLE_TO_MOVE);
    update_multi_selection_exclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_exclusive_military(GtkToggleAction *action,
                                                        gpointer user_data)
{
  if (!!(multi_select_exclusive_filter & FILTER_MILITARY) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_exclusive_filter, FILTER_MILITARY);
    update_multi_selection_exclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_exclusive_off(GtkToggleAction *action,
                                                   gpointer user_data)
{
  if (!!(multi_select_exclusive_filter & FILTER_OFF) ^
      gtk_toggle_action_get_active(action)) {
    filter_change(&multi_select_exclusive_filter, FILTER_OFF);
    update_multi_selection_exclusive_filter_menu();
    update_unit_info_label(get_unit_in_focus());
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_utype(GtkAction *unusedaction,
                                           GtkRadioAction *action,
                                           gpointer user_data)
{
  multi_select_utype = gtk_radio_action_get_current_value(action);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_multi_selection_place(GtkAction *unusedaction,
                                           GtkRadioAction *action,
                                           gpointer user_data)
{
  multi_select_place = gtk_radio_action_get_current_value(action);
}

/****************************************************************
  ...
*****************************************************************/
static const char *load_menu_multi_selection(void)
{
  static char buf[8192];

  GtkActionEntry entries_multi_selection[] = {
    {"MULTI_SELECTION", NULL, N_("_Multi-Selection"), NULL, NULL, NULL},
    {"MULTI_SELECTION_MS_SELECT", NULL, N_("Multi-selection _select"),
     "<Shift>c", N_("Multi-selection _select"),
     G_CALLBACK(callback_multi_selection_ms_select)},
    {"MULTI_SELECTION_MS_ACTIVE_ALL", NULL,
     N_("Multi-selection _active all units"),
     "<Shift>b", N_("Multi-selection _active all units"),
     G_CALLBACK(callback_multi_selection_ms_active_all)},
    {"MULTI_SELECTION_MS_CLEAR", NULL, N_("Multi-selection _clear"),
     "<Shift>v", N_("Multi-selection _clear"),
     G_CALLBACK(callback_multi_selection_ms_clear)},
    {"MULTI_SELECTION_MODE", NULL, N_("_Multi-Selection _mode"),
     NULL, NULL, NULL},
    {"MULTI_SELECTION_INCLUSIVE", NULL, N_("Inclusive filter"), 
     NULL, NULL, NULL},
    {"MULTI_SELECTION_EXCLUSIVE", NULL, N_("Exclusive filter"),
     NULL, NULL, NULL},
    {"MULTI_SELECTION_MY_AI_SPREAD", NULL, N_("Multi-selection sprea_d"),
     "<shift>s", N_("Multi-selection sprea_d"),
     G_CALLBACK(callback_multi_selection_my_ai_spread)},
    {"MULTI_SELECTION_MS1", NULL, N_("Multi-selection") " _1",
     NULL, NULL, NULL},
    {"MULTI_SELECTION_MS1_SELECT", NULL, N_("_Select"),
     "1", N_("_Select"), G_CALLBACK(callback_multi_selection_ms1_select)},
    {"MULTI_SELECTION_MS1_ADD", NULL, N_("_Add to current selection"),
     "<Shift>1", N_("_Add to current selection"),
     G_CALLBACK(callback_multi_selection_ms1_add)},
    {"MULTI_SELECTION_MS1_RECORD", NULL, N_("_Record"),
     "<Control>1", N_("_Record"),
     G_CALLBACK(callback_multi_selection_ms1_record)},
    {"MULTI_SELECTION_MS1_CLEAR", NULL, N_("_Clear"),
     "<Control><Shift>1", N_("_Clear"),
     G_CALLBACK(callback_multi_selection_ms1_clear)},
    {"MULTI_SELECTION_MS2", NULL, N_("Multi-selection") " _2",
     NULL, NULL, NULL},
    {"MULTI_SELECTION_MS2_SELECT", NULL, N_("_Select"),
     "2", N_("_Select"), G_CALLBACK(callback_multi_selection_ms2_select)},
    {"MULTI_SELECTION_MS2_ADD", NULL, N_("_Add to current selection"),
     "<Shift>2", N_("_Add to current selection"),
     G_CALLBACK(callback_multi_selection_ms2_add)},
    {"MULTI_SELECTION_MS2_RECORD", NULL, N_("_Record"),
     "<Control>2", N_("_Record"), 
     G_CALLBACK(callback_multi_selection_ms2_record)},
    {"MULTI_SELECTION_MS2_CLEAR", NULL, N_("_Clear"),
     "<Control><Shift>2", N_("_Clear"),
     G_CALLBACK(callback_multi_selection_ms2_clear)},
    {"MULTI_SELECTION_MS3", NULL, N_("Multi-selection") " _3",
     NULL, NULL, NULL},
    {"MULTI_SELECTION_MS3_SELECT", NULL, N_("_Select"),
     "3", N_("_Select"), G_CALLBACK(callback_multi_selection_ms3_select)},
    {"MULTI_SELECTION_MS3_ADD", NULL, N_("_Add to current selection"),
     "<Shift>3", N_("_Add to current selection"),
     G_CALLBACK(callback_multi_selection_ms3_add)},
    {"MULTI_SELECTION_MS3_RECORD", NULL, N_("_Record"),
     "<Control>3", N_("_Record"),
     G_CALLBACK(callback_multi_selection_ms3_record)},
    {"MULTI_SELECTION_MS3_CLEAR", NULL, N_("_Clear"),
     "<Control><Shift>3", N_("_Clear"),
     G_CALLBACK(callback_multi_selection_ms3_clear)},
    {"MULTI_SELECTION_MS4", NULL, N_("Multi-selection") " _4",
     NULL, NULL, NULL},
    {"MULTI_SELECTION_MS4_SELECT", NULL, N_("_Select"),
     "4", N_("_Select"), G_CALLBACK(callback_multi_selection_ms4_select)},
    {"MULTI_SELECTION_MS4_ADD", NULL, N_("_Add to current selection"),
     "<Shift>4", N_("_Add to current selection"),
     G_CALLBACK(callback_multi_selection_ms4_add)},
    {"MULTI_SELECTION_MS4_RECORD", NULL, N_("_Record"),
     "<Control>4", N_("_Record"),
     G_CALLBACK(callback_multi_selection_ms4_record)},
    {"MULTI_SELECTION_MS4_CLEAR", NULL, N_("_Clear"),
     "<Control><Shift>4", N_("_Clear"),
     G_CALLBACK(callback_multi_selection_ms4_clear)},
    {"MULTI_SELECTION_MS5", NULL, N_("Multi-selection") " _5",
     NULL, NULL, NULL},
    {"MULTI_SELECTION_MS5_SELECT", NULL, N_("_Select"),
     "5", N_("_Select"), G_CALLBACK(callback_multi_selection_ms5_select)},
    {"MULTI_SELECTION_MS5_ADD", NULL, N_("_Add to current selection"),
     "<Shift>5", N_("_Add to current selection"),
     G_CALLBACK(callback_multi_selection_ms5_add)},
    {"MULTI_SELECTION_MS5_RECORD", NULL, N_("_Record"),
     "<Control>5", N_("_Record"), 
     G_CALLBACK(callback_multi_selection_ms5_record)},
    {"MULTI_SELECTION_MS5_CLEAR", NULL, N_("_Clear"),
     "<Control><Shift>5", N_("_Clear"),
     G_CALLBACK(callback_multi_selection_ms5_clear)},
    {"MULTI_SELECTION_MS6", NULL, N_("Multi-selection") " _6",
     NULL, NULL, NULL},
    {"MULTI_SELECTION_MS6_SELECT", NULL, N_("_Select"),
     "6", N_("_Select"), G_CALLBACK(callback_multi_selection_ms6_select)},
    {"MULTI_SELECTION_MS6_ADD", NULL, N_("_Add to current selection"),
     "<Shift>6", N_("_Add to current selection"),
     G_CALLBACK(callback_multi_selection_ms6_add)},
    {"MULTI_SELECTION_MS6_RECORD", NULL, N_("_Record"),
     "<Control>6", N_("_Record"),
     G_CALLBACK(callback_multi_selection_ms6_record)},
    {"MULTI_SELECTION_MS6_CLEAR", NULL, N_("_Clear"),
     "<Control><Shift>6", N_("_Clear"),
     G_CALLBACK(callback_multi_selection_ms6_clear)},
    {"MULTI_SELECTION_MS7", NULL, N_("Multi-selection") " _7",
     NULL, NULL, NULL},
    {"MULTI_SELECTION_MS7_SELECT", NULL, N_("_Select"),
     "7", N_("_Select"), G_CALLBACK(callback_multi_selection_ms7_select)},
    {"MULTI_SELECTION_MS7_ADD", NULL, N_("_Add to current selection"),
     "<Shift>7", N_("_Add to current selection"),
     G_CALLBACK(callback_multi_selection_ms7_add)},
    {"MULTI_SELECTION_MS7_RECORD", NULL, N_("_Record"),
     "<Control>7", N_("_Record"),
     G_CALLBACK(callback_multi_selection_ms7_record)},
    {"MULTI_SELECTION_MS7_CLEAR", NULL, N_("_Clear"),
     "<Control><Shift>7", N_("_Clear"),
     G_CALLBACK(callback_multi_selection_ms7_clear)},
    {"MULTI_SELECTION_MS8", NULL, N_("Multi-selection") " _8",
     NULL, NULL, NULL},
    {"MULTI_SELECTION_MS8_SELECT", NULL, N_("_Select"),
     "8", N_("_Select"), G_CALLBACK(callback_multi_selection_ms8_select)},
    {"MULTI_SELECTION_MS8_ADD", NULL, N_("_Add to current selection"),
     "<Shift>8", N_("_Add to current selection"),
     G_CALLBACK(callback_multi_selection_ms8_add)},
    {"MULTI_SELECTION_MS8_RECORD", NULL, N_("_Record"),
     "<Control>8", N_("_Record"),
     G_CALLBACK(callback_multi_selection_ms8_record)},
    {"MULTI_SELECTION_MS8_CLEAR", NULL, N_("_Clear"),
     "<Control><Shift>8", N_("_Clear"),
     G_CALLBACK(callback_multi_selection_ms8_clear)},
    {"MULTI_SELECTION_MS9", NULL, N_("Multi-selection") " _9",
     NULL, NULL, NULL},
    {"MULTI_SELECTION_MS9_SELECT", NULL, N_("_Select"),
     "9", N_("_Select"), G_CALLBACK(callback_multi_selection_ms9_select)},
    {"MULTI_SELECTION_MS9_ADD", NULL, N_("_Add to current selection"),
     "<Shift>9", N_("_Add to current selection"),
     G_CALLBACK(callback_multi_selection_ms9_add)},
    {"MULTI_SELECTION_MS9_RECORD", NULL, N_("_Record"),
     "<Control>9", N_("_Record"), 
     G_CALLBACK(callback_multi_selection_ms9_record)},
    {"MULTI_SELECTION_MS9_CLEAR", NULL, N_("_Clear"),
     "<Control><Shift>9", N_("_Clear"),
     G_CALLBACK(callback_multi_selection_ms9_clear)}
  };

  GtkToggleActionEntry toggle_entries_multi_selection[] = {
    {"MULTI_SELECTION_MY_AI_SPREAD_AIRPORT", NULL,
     N_("Spread only in cities _with airport"),
     NULL, N_("Spread only in cities _with airport"),
     G_CALLBACK(callback_multi_selection_my_ai_spread_airport), TRUE},
    {"MULTI_SELECTION_MY_AI_SPREAD_ALLY", NULL,
     N_("Allow spreadi_ng into allied cities"),
     NULL, N_("Allow spreadi_ng into allied cities"),
     G_CALLBACK(callback_multi_selection_my_ai_spread_ally), TRUE}
  };

  GtkToggleActionEntry toggle_entries_multi_selection_inclusive[] = {
    {"MULTI_SELECTION_INCLUSIVE_ALL", NULL, N_("_All units"),
     NULL, N_("_All units"),
     G_CALLBACK(callback_multi_selection_inclusive_all), TRUE},
    {"MULTI_SELECTION_INCLUSIVE_NEW", NULL, N_("_New units"),
     NULL, N_("_New units"),
     G_CALLBACK(callback_multi_selection_inclusive_new), TRUE},
    {"MULTI_SELECTION_INCLUSIVE_FORTIFIED", NULL, N_("_Fortified units"),
     NULL, N_("_Fortified units"),
     G_CALLBACK(callback_multi_selection_inclusive_fortified), TRUE},
    {"MULTI_SELECTION_INCLUSIVE_SENTRIED", NULL, N_("_Sentried units"),
     NULL, N_("_Sentried units"),
     G_CALLBACK(callback_multi_selection_inclusive_sentried), TRUE},
    {"MULTI_SELECTION_INCLUSIVE_VETERAN", NULL, N_("_Veteran units"),
     NULL, N_("_Veteran units"),
     G_CALLBACK(callback_multi_selection_inclusive_veteran), TRUE},
    {"MULTI_SELECTION_INCLUSIVE_AUTO", NULL, N_("Aut_o units"),
     NULL, N_("Aut_o units"),
     G_CALLBACK(callback_multi_selection_inclusive_auto), TRUE},
    {"MULTI_SELECTION_INCLUSIVE_IDLE", NULL, N_("_Idle units"),
     NULL, N_("_Idle units"),
     G_CALLBACK(callback_multi_selection_inclusive_idle), TRUE},
    {"MULTI_SELECTION_INCLUSIVE_ABLE_TO_MOVE", NULL, N_("Units abl_e to move"),
     NULL, N_("Units abl_e to move"),
     G_CALLBACK(callback_multi_selection_inclusive_able_to_move), TRUE},
    {"MULTI_SELECTION_INCLUSIVE_MILITARY", NULL, N_("_Military units"),
     NULL, N_("_Military units"),
     G_CALLBACK(callback_multi_selection_inclusive_military), TRUE},
    {"MULTI_SELECTION_INCLUSIVE_OFF", NULL, N_("_Off"),
     NULL, N_("_Off"), G_CALLBACK(callback_multi_selection_inclusive_off), TRUE}

  };

  GtkToggleActionEntry toggle_entries_multi_selection_exclusive[] = {
    {"MULTI_SELECTION_EXCLUSIVE_ALL", NULL, N_("_All units"),
     NULL, N_("_All units"),
     G_CALLBACK(callback_multi_selection_exclusive_all), TRUE},
    {"MULTI_SELECTION_EXCLUSIVE_NEW", NULL, N_("_New units"),
     NULL, N_("_New units"),
     G_CALLBACK(callback_multi_selection_exclusive_new), TRUE},
    {"MULTI_SELECTION_EXCLUSIVE_FORTIFIED", NULL, N_("_Fortified units"),
     NULL, N_("_Fortified units"),
     G_CALLBACK(callback_multi_selection_exclusive_fortified), TRUE},
    {"MULTI_SELECTION_EXCLUSIVE_SENTRIED", NULL, N_("_Sentried units"),
     NULL, N_("_Sentried units"),
     G_CALLBACK(callback_multi_selection_exclusive_sentried), TRUE},
    {"MULTI_SELECTION_EXCLUSIVE_VETERAN", NULL, N_("_Veteran units"),
     NULL, N_("_Veteran units"),
     G_CALLBACK(callback_multi_selection_exclusive_veteran), TRUE},
    {"MULTI_SELECTION_EXCLUSIVE_AUTO", NULL, N_("Aut_o units"),
     NULL, N_("Aut_o units"),
     G_CALLBACK(callback_multi_selection_exclusive_auto), TRUE},
    {"MULTI_SELECTION_EXCLUSIVE_IDLE", NULL, N_("_Idle units"),
     NULL, N_("_Idle units"),
     G_CALLBACK(callback_multi_selection_exclusive_idle), TRUE},
    {"MULTI_SELECTION_EXCLUSIVE_ABLE_TO_MOVE", NULL, N_("Units able to mov_e"),
     NULL, N_("Units able to mov_e"),
     G_CALLBACK(callback_multi_selection_exclusive_able_to_move), TRUE},
    {"MULTI_SELECTION_EXCLUSIVE_MILITARY", NULL, N_("_Military units"),
     NULL, N_("_Military units"),
     G_CALLBACK(callback_multi_selection_exclusive_military), TRUE},
    {"MULTI_SELECTION_EXCLUSIVE_OFF", NULL, N_("_Off"),
     NULL, N_("_Off"), G_CALLBACK(callback_multi_selection_exclusive_off), TRUE}
  };

  GtkRadioActionEntry radio_entries_multi_selection_utype[] = {
    {"MULTI_SELECTION_MODE_SAME_TYPE", NULL,
     N_("Only units with the _same type"),
     "<Shift>F6", N_("Only units with the _same type"), UTYPE_SAME_TYPE},
    {"MULTI_SELECTION_MODE_SAME_MOVE_TYPE", NULL,
     N_("Only units with the same _move type"),
     "<Shift>F7", N_("Only units with the same _move type"),
     UTYPE_SAME_MOVE_TYPE},
    {"MULTI_SELECTION_MODE_ALL", NULL, N_("_All unit types"),
     "<Shift>F8", N_("_All unit types"), UTYPE_ALL}
  };

  GtkRadioActionEntry radio_entries_multi_selection_place[] = {
    {"MULTI_SELECTION_MODE_SINGLE_UNIT", NULL, N_("_Single unit"),
     "<Shift>F1", N_("_Single unit"), PLACE_SINGLE_UNIT},
    {"MULTI_SELECTION_MODE_IN_TRANSPORTER", NULL,
     N_("All _units in the same transporter"),
     "<Shift>F2", N_("All _units in the same transporter"),
     PLACE_IN_TRANSPORTER},
    {"MULTI_SELECTION_MODE_ON_TILE", NULL, N_("All units on the _tile"),
     "<Shift>F3", N_("All units on the _tile"), PLACE_ON_TILE},
    {"MULTI_SELECTION_MODE_ON_CONTINENT", NULL,
     N_("All units on the _continent"),
     "<Shift>F4", N_("All units on the _continent"), PLACE_ON_CONTINENT},
    {"MULTI_SELECTION_MODE_EVERY_WHERE", NULL, N_("_All units"),
     "<Shift>F5", N_("_All units"), PLACE_EVERY_WHERE}
  };

  action_group_multi_selection = gtk_action_group_new("GroupMultiSelection");
  gtk_action_group_set_translation_domain(action_group_multi_selection,
                                          textdomain(PACKAGE));
  gtk_action_group_add_actions(action_group_multi_selection, 
                               entries_multi_selection,
                               G_N_ELEMENTS(entries_multi_selection),
                               NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     action_group_multi_selection, 0);

  toggle_action_group_multi_selection =
    gtk_action_group_new("ToggleGroupMultiSelection");
  gtk_action_group_set_translation_domain(toggle_action_group_multi_selection,
                                          textdomain(PACKAGE));
  gtk_action_group_add_toggle_actions(toggle_action_group_multi_selection,
                                      toggle_entries_multi_selection,
                                      G_N_ELEMENTS(toggle_entries_multi_selection),
                                      NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     toggle_action_group_multi_selection, 0);

  toggle_action_group_multi_selection_inclusive =
    gtk_action_group_new("ToggleGroupMultiSelectionInclusive");
  gtk_action_group_set_translation_domain(toggle_action_group_multi_selection_inclusive,
                                          textdomain(PACKAGE));
  gtk_action_group_add_toggle_actions(toggle_action_group_multi_selection_inclusive,
                                      toggle_entries_multi_selection_inclusive,
                                      G_N_ELEMENTS(toggle_entries_multi_selection_inclusive),
                                      NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     toggle_action_group_multi_selection_inclusive, 0);

  toggle_action_group_multi_selection_exclusive =
    gtk_action_group_new("ToggleGroupMultiSelectionExclusive");
  gtk_action_group_set_translation_domain(toggle_action_group_multi_selection_exclusive,
                                          textdomain(PACKAGE));
  gtk_action_group_add_toggle_actions(toggle_action_group_multi_selection_exclusive,
                                      toggle_entries_multi_selection_exclusive,
                                      G_N_ELEMENTS(toggle_entries_multi_selection_exclusive),
                                      NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     toggle_action_group_multi_selection_exclusive, 0);


  radio_action_group_multi_selection_utype =
    gtk_action_group_new("RadioGroupMultiSelectionUtype");
  gtk_action_group_set_translation_domain(radio_action_group_multi_selection_utype,
                                          textdomain(PACKAGE));

  gtk_action_group_add_radio_actions(radio_action_group_multi_selection_utype,
                                     radio_entries_multi_selection_utype,
                                     G_N_ELEMENTS(radio_entries_multi_selection_utype),
                                     -1,
                                     G_CALLBACK(callback_multi_selection_utype),
                                     NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     radio_action_group_multi_selection_utype, 0);

  radio_action_group_multi_selection_place =
    gtk_action_group_new("RadioGroupMultiSelectionPlace");
  gtk_action_group_set_translation_domain(radio_action_group_multi_selection_place,
                                          textdomain(PACKAGE));
  gtk_action_group_add_radio_actions(radio_action_group_multi_selection_place,
                                     radio_entries_multi_selection_place,
                                     G_N_ELEMENTS(radio_entries_multi_selection_place),
                                     -1,
                                     G_CALLBACK(callback_multi_selection_place),
                                     NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     radio_action_group_multi_selection_place, 0);

  my_snprintf(buf, sizeof(buf), "%s",
              "<menu action=\"MULTI_SELECTION\">\n"
              "<menuitem action=\"MULTI_SELECTION_MS_SELECT\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS_ACTIVE_ALL\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS_CLEAR\" />\n"
              "<separator/>\n"
              "<menu action=\"MULTI_SELECTION\">\n"
              "<menuitem action=\"MULTI_SELECTION_MODE_SINGLE_UNIT\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MODE_IN_TRANSPORTER\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MODE_ON_TILE\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MODE_ON_CONTINENT\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MODE_EVERY_WHERE\" />\n"
              "<separator/>\n"
              "<menuitem action=\"MULTI_SELECTION_MODE_SAME_TYPE\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MODE_SAME_MOVE_TYPE\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MODE_ALL\" />\n"
              "</menu>\n"
              "<menu action=\"MULTI_SELECTION_INCLUSIVE\">\n"
              "<menuitem action=\"MULTI_SELECTION_INCLUSIVE_ALL\" />\n"
              "<menuitem action=\"MULTI_SELECTION_INCLUSIVE_NEW\" />\n"
              "<menuitem action=\"MULTI_SELECTION_INCLUSIVE_FORTIFIED\" />\n"
              "<menuitem action=\"MULTI_SELECTION_INCLUSIVE_SENTRIED\" />\n"
              "<menuitem action=\"MULTI_SELECTION_INCLUSIVE_VETERAN\" />\n"
              "<menuitem action=\"MULTI_SELECTION_INCLUSIVE_AUTO\" />\n"
              "<menuitem action=\"MULTI_SELECTION_INCLUSIVE_IDLE\" />\n"
              "<menuitem action=\"MULTI_SELECTION_INCLUSIVE_ABLE_TO_MOVE\" />\n"
              "<menuitem action=\"MULTI_SELECTION_INCLUSIVE_MILITARY\" />\n"
              "<menuitem action=\"MULTI_SELECTION_INCLUSIVE_OFF\" />\n"
              "</menu>\n"
              "<menu action=\"MULTI_SELECTION_EXCLUSIVE\">\n"
              "<menuitem action=\"MULTI_SELECTION_EXCLUSIVE_ALL\" />\n"
              "<menuitem action=\"MULTI_SELECTION_EXCLUSIVE_NEW\" />\n"
              "<menuitem action=\"MULTI_SELECTION_EXCLUSIVE_FORTIFIED\" />\n"
              "<menuitem action=\"MULTI_SELECTION_EXCLUSIVE_SENTRIED\" />\n"
              "<menuitem action=\"MULTI_SELECTION_EXCLUSIVE_VETERAN\" />\n"
              "<menuitem action=\"MULTI_SELECTION_EXCLUSIVE_AUTO\" />\n"
              "<menuitem action=\"MULTI_SELECTION_EXCLUSIVE_IDLE\" />\n"
              "<menuitem action=\"MULTI_SELECTION_EXCLUSIVE_ABLE_TO_MOVE\" />\n"
              "<menuitem action=\"MULTI_SELECTION_EXCLUSIVE_MILITARY\" />\n"
              "<menuitem action=\"MULTI_SELECTION_EXCLUSIVE_OFF\" />\n"
              "</menu>\n"
              "<separator/>\n"
              "<menuitem action=\"MULTI_SELECTION_MY_AI_SPREAD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MY_AI_SPREAD_AIRPORT\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MY_AI_SPREAD_ALLY\" />\n"
              "<menu action=\"MULTI_SELECTION_MS1\">\n"
              "<menuitem action=\"MULTI_SELECTION_MS1_SELECT\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS1_ADD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS1_RECORD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS1_CLEAR\" />\n"
              "</menu>\n"
              "<menu action=\"MULTI_SELECTION_MS2\">\n"
              "<menuitem action=\"MULTI_SELECTION_MS2_SELECT\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS2_ADD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS2_RECORD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS2_CLEAR\" />\n"
              "</menu>\n"
              "<menu action=\"MULTI_SELECTION_MS3\">\n"
              "<menuitem action=\"MULTI_SELECTION_MS3_SELECT\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS3_ADD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS3_RECORD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS3_CLEAR\" />\n"
              "</menu>\n"
              "<menu action=\"MULTI_SELECTION_MS4\">\n"
              "<menuitem action=\"MULTI_SELECTION_MS4_SELECT\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS4_ADD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS4_RECORD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS4_CLEAR\" />\n"
              "</menu>\n"
              "<menu action=\"MULTI_SELECTION_MS5\">\n"
              "<menuitem action=\"MULTI_SELECTION_MS5_SELECT\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS5_ADD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS5_RECORD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS5_CLEAR\" />\n"
              "</menu>\n"
              "<menu action=\"MULTI_SELECTION_MS6\">\n"
              "<menuitem action=\"MULTI_SELECTION_MS6_SELECT\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS6_ADD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS6_RECORD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS6_CLEAR\" />\n"
              "</menu>\n"
              "<menu action=\"MULTI_SELECTION_MS7\">\n"
              "<menuitem action=\"MULTI_SELECTION_MS7_SELECT\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS7_ADD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS7_RECORD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS7_CLEAR\" />\n"
              "</menu>\n"
              "<menu action=\"MULTI_SELECTION_MS8\">\n"
              "<menuitem action=\"MULTI_SELECTION_MS8_SELECT\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS8_ADD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS8_RECORD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS8_CLEAR\" />\n"
              "</menu>\n"
              "<menu action=\"MULTI_SELECTION_MS9\">\n"
              "<menuitem action=\"MULTI_SELECTION_MS9_SELECT\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS9_ADD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS9_RECORD\" />\n"
              "<menuitem action=\"MULTI_SELECTION_MS9_CLEAR\" />\n"
              "</menu>\n"
              "</menu>\n");

  freelog(LOG_MENU, "Strlen of buf = %d; Size of load_menu_multi_selection %d",
          strlen(buf), sizeof(buf));

  return buf;
}


/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_air_patrol(GtkAction *action,
                                              gpointer user_data)
{
  if (get_unit_in_focus()) {
    if (can_unit_do_activity(get_unit_in_focus(), ACTIVITY_AIRBASE))
      key_unit_airbase();
    else {
      key_unit_air_patrol();
    }
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_air_patrol_dest(GtkAction *action,
                                                   gpointer user_data)
{
  key_airplane_patrol();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_patrol_execute(GtkAction *action,
                                                  gpointer user_data)
{
  my_ai_patrol_execute_all();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_set_rallies(GtkAction *action,
                                               gpointer user_data)
{
  if (tiles_hilited_cities) {
    key_select_rally_point();
  } else {
    my_ai_spread_execute();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_clear_rallies(GtkAction *action,
                                                 gpointer user_data)
{
  key_clear_rally_point_for_selected_cities();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_my_ai_execute(GtkAction *action,
                                                 gpointer user_data)
{
  multi_select_iterate(TRUE,punit)
    {
      my_ai_unit_execute(punit);
    } multi_select_iterate_end;
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_my_ai_free(GtkAction *action,
                                              gpointer user_data)
{
  unit_list_iterate(multi_select_get_units_focus(), punit) {
    my_ai_orders_free(punit);
  } unit_list_iterate_end;
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_my_ai_execute_all(GtkAction *action,
                                                     gpointer user_data)
{
  my_ai_execute();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_diplomat_ignore_allies(GtkToggleAction *action,
                                                          gpointer user_data)
{
  default_diplomat_ignore_allies = gtk_toggle_action_get_active(action);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_unit_lock(GtkToggleAction *action,
                                             gpointer user_data)
{
  default_action_locked = gtk_toggle_action_get_active(action);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_unit_military(GtkToggleAction *action,
                                                 gpointer user_data)
{
  default_action_military_only = gtk_toggle_action_get_active(action);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_toggle_wakeup(GtkToggleAction *action,
                                                 gpointer user_data)
{
  if (autowakeup_state ^ gtk_toggle_action_get_active(action)) {
    key_toggle_autowakeup();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_toggle_moveandattack(GtkToggleAction *action,
                                                        gpointer user_data)
{
  if (moveandattack_state ^ gtk_toggle_action_get_active(action)) {
    key_toggle_moveandattack();
  }
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_diplomat(GtkAction *unusedaction,
                                            GtkRadioAction *action,
                                            gpointer user_data)
{
  default_diplomat_action = gtk_radio_action_get_current_value(action);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_caravan(GtkAction *unusedaction,
                                           GtkRadioAction *action,
                                           gpointer user_data)
{
  default_caravan_action = gtk_radio_action_get_current_value(action);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_miscellaneous_unit(GtkAction *unusedaction,
                                        GtkRadioAction *action,
                                        gpointer user_data)
{
  default_action_type = gtk_radio_action_get_current_value(action);
}


/****************************************************************
  ...
*****************************************************************/
static const char *load_menu_miscellaneous(void)
{
  static char buf[2048];

  GtkActionEntry entries_miscellaneous[] = {
    {"MISCELLANEOUS", NULL, N_("Misce_llaneous"), NULL, NULL, NULL},
    {"MISCELLANEOUS_AIR_PATROL", NULL, N_("Airplane _patrol"),
     "e", N_("Airplane _patrol"), 
     G_CALLBACK(callback_miscellaneous_air_patrol)},
    {"MISCELLANEOUS_AIR_PATROL_DEST", NULL, N_("Airplane patrol _destination"),
     "<Control>e", N_("Airplane patrol _destination"),
     G_CALLBACK(callback_miscellaneous_air_patrol_dest)},
    {"MISCELLANEOUS_PATROL_EXECUTE", NULL, N_("_Execute all patrol orders"),
     "<shift>e", N_("_Execute all patrol orders"),
     G_CALLBACK(callback_miscellaneous_patrol_execute)},
    {"MISCELLANEOUS_CARAVAN", NULL, N_("_Caravan action upon arrival"),
     NULL, NULL, NULL},
    {"MISCELLANEOUS_DIPLOMAT", NULL, N_("Diplomat action upon arrival"),
     NULL, NULL, NULL},
    {"MISCELLANEOUS_UNIT", NULL, N_("New unit default action"),
     NULL, NULL, NULL},
    {"MISCELLANEOUS_SET_RALLIES", NULL, N_("_Set rallies for selected cities"),
     "<Shift>s", N_("_Set rallies for selected cities"),
     G_CALLBACK(callback_miscellaneous_set_rallies)},
    {"MISCELLANEOUS_CLEAR_RALLIES", NULL,
     N_("_Clear rallies in selected cities"),
     "<shift>r", N_("_Clear rallies in selected cities"),
     G_CALLBACK(callback_miscellaneous_clear_rallies)},
    {"MISCELLANEOUS_MY_AI_EXECUTE", NULL, N_("E_xecute automatic orders"),
     "<control>m", N_("E_xecute automatic orders"),
     G_CALLBACK(callback_miscellaneous_my_ai_execute)},
    {"MISCELLANEOUS_MY_AI_FREE", NULL, N_("_Free automatic orders"),
     "<control>f", N_("_Free automatic orders") ,
     G_CALLBACK(callback_miscellaneous_my_ai_free)},
    {"MISCELLANEOUS_MY_AI_EXECUTE_ALL", NULL,
     N_("Execute all automatic _orders"),
     "<shift>m", N_("Execute all automatic _orders"),
     G_CALLBACK(callback_miscellaneous_my_ai_execute_all)},
  };

  GtkToggleActionEntry toggle_entries_miscellaneous[] = {
    {"MISCELLANEOUS_DIPLOMAT_IGNORE_ALLIES", NULL, N_("Ignore allies"),
     NULL, N_("Ignore allies"),
     G_CALLBACK(callback_miscellaneous_diplomat_ignore_allies), TRUE},
    {"MISCELLANEOUS_UNIT_LOCK", NULL, N_("Lock new unit default action"),
     NULL, N_("Lock new unit default action"),
     G_CALLBACK(callback_miscellaneous_unit_lock), TRUE},
    {"MISCELLANEOUS_UNIT_MILITARY", NULL, N_("Military units only"),
     NULL, N_("Military units only"),
     G_CALLBACK(callback_miscellaneous_unit_military), TRUE},
    {"MISCELLANEOUS_TOGGLE_WAKEUP", NULL, N_("Autowakeup _sentried units"),
     NULL, N_("Autowakeup _sentried units"),
     G_CALLBACK(callback_miscellaneous_toggle_wakeup), TRUE},
    {"MISCELLANEOUS_TOGGLE_MOVEANDATTACK", NULL, N_("Move and _attack mode"),
     NULL, N_("Move and _attack mode"),
     G_CALLBACK(callback_miscellaneous_toggle_moveandattack), TRUE}
  };

  GtkRadioActionEntry radio_entries_miscellaneous_diplomat[] = {
    {"MISCELLANEOUS_DIPLOMAT_POPUP", NULL, N_("_Popup dialog"),
     NULL, N_("_Popup dialog"), 0},
    {"MISCELLANEOUS_DIPLOMAT_BRIBE", NULL, N_("_Bribe unit"),
     NULL, N_("_Bribe unit"), 1},
    {"MISCELLANEOUS_DIPLOMAT_SABOTAGE_UNIT", NULL, N_("_Sabotage unit (spy)"),
     NULL, N_("_Sabotage unit (spy)"), 2},
    {"MISCELLANEOUS_DIPLOMAT_EMBASSY", NULL, N_("_Establish embassy"),
     NULL, N_("_Establish embassy"), 3},
    {"MISCELLANEOUS_DIPLOMAT_INVESTIGATE", NULL, N_("_Investigate city"),
     NULL, N_("_Investigate city"), 4},
    {"MISCELLANEOUS_DIPLOMAT_SABOTAGE_CITY", NULL, N_("_Sabotage city"),
     NULL, N_("_Sabotage city"), 5},
    {"MISCELLANEOUS_DIPLOMAT_STEAL", NULL, N_("_Steal technology"),
     NULL, N_("_Steal technology"), 6},
    {"MISCELLANEOUS_DIPLOMAT_INCITE", NULL, N_("_Incite revolt"),
     NULL, N_("_Incite revolt"), 7},
    {"MISCELLANEOUS_DIPLOMAT_POISON", NULL, N_("_Poison city (spy)"),
     NULL, N_("_Poison city (spy)"), 8},
    {"MISCELLANEOUS_DIPLOMAT_NOTHING", NULL, N_("_Keep going"),
     NULL, N_("_Keep going"), 9}
  };

  GtkRadioActionEntry radio_entries_miscellaneous_caravan[] = {
    {"MISCELLANEOUS_CARAVAN_POPUP", NULL, N_("_Popup dialog"),
     NULL, N_("_Popup dialog"), 0},
    {"MISCELLANEOUS_CARAVAN_TRADE", NULL, N_("_Establish trade route"),
     NULL, N_("_Establish trade route"), 1},
    {"MISCELLANEOUS_CARAVAN_BUILD", NULL, N_("_Help building wonder"),
     NULL, N_("_Help building wonder"), 2},
    {"MISCELLANEOUS_CARAVAN_NOTHING", NULL, N_("_Keep going"),
     NULL, N_("_Keep going"), 3}
  };

  GtkRadioActionEntry radio_entries_miscellaneous_unit[] = {
    {"MISCELLANEOUS_UNIT_IDLE", NULL, N_("Idle"),
     NULL, N_("Idle"), ACTION_IDLE},
    {"MISCELLANEOUS_UNIT_SENTRY", NULL, N_("Sentry"),
     NULL, N_("Sentry"), ACTION_SENTRY},
    {"MISCELLANEOUS_UNIT_FORTIFY", NULL, N_("Fortify"),
     NULL, N_("Fortify"), ACTION_FORTIFY},
    {"MISCELLANEOUS_UNIT_SLEEP", NULL, N_("Sleep"),
     NULL, N_("Sleep"), ACTION_SLEEP},
    {"MISCELLANEOUS_UNIT_FORTIFY_OR_SLEEP", NULL, N_("Fortify or Sleep"),
     NULL, N_("Fortify or Sleep"), ACTION_FORTIFY_OR_SLEEP}
  };

  action_group_miscellaneous = gtk_action_group_new("GroupMiscellaneous");
  gtk_action_group_set_translation_domain(action_group_miscellaneous,
                                          textdomain(PACKAGE));
  gtk_action_group_add_actions(action_group_miscellaneous,
                               entries_miscellaneous,
                               G_N_ELEMENTS(entries_miscellaneous),
                               NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     action_group_miscellaneous, 0);

  toggle_action_group_miscellaneous =
    gtk_action_group_new("ToggleGroupMiscellaneous");
  gtk_action_group_set_translation_domain(toggle_action_group_miscellaneous,
                                          textdomain(PACKAGE));
  gtk_action_group_add_toggle_actions(toggle_action_group_miscellaneous,
                                      toggle_entries_miscellaneous,
                                      G_N_ELEMENTS(toggle_entries_miscellaneous),
                                      NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     toggle_action_group_miscellaneous, 0);


  radio_action_group_miscellaneous_diplomat =
    gtk_action_group_new("RadioGroupMiscellaneouDiplomat");
  gtk_action_group_set_translation_domain(radio_action_group_miscellaneous_diplomat,
                                          textdomain(PACKAGE));
  gtk_action_group_add_radio_actions(radio_action_group_miscellaneous_diplomat,
                                     radio_entries_miscellaneous_diplomat,
                                     G_N_ELEMENTS(radio_entries_miscellaneous_diplomat),
                                     -1,
                                     G_CALLBACK(callback_miscellaneous_diplomat),
                                     NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     radio_action_group_miscellaneous_diplomat, 0);

  radio_action_group_miscellaneous_caravan =
    gtk_action_group_new("RadioGroupMiscellaneouCaravan");
  gtk_action_group_set_translation_domain(radio_action_group_miscellaneous_caravan,
                                          textdomain(PACKAGE));
  gtk_action_group_add_radio_actions(radio_action_group_miscellaneous_caravan,
                                     radio_entries_miscellaneous_caravan,
                                     G_N_ELEMENTS(radio_entries_miscellaneous_caravan),
                                     -1,
                                     G_CALLBACK(callback_miscellaneous_caravan),
                                     NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     radio_action_group_miscellaneous_caravan, 0);

  radio_action_group_miscellaneous_unit =
    gtk_action_group_new("RadioGroupMiscellaneouUnit");
  gtk_action_group_set_translation_domain(radio_action_group_miscellaneous_unit,
                                          textdomain(PACKAGE));
  gtk_action_group_add_radio_actions(radio_action_group_miscellaneous_unit,
                                     radio_entries_miscellaneous_unit,
                                     G_N_ELEMENTS(radio_entries_miscellaneous_unit),
                                     -1,
                                     G_CALLBACK(callback_miscellaneous_unit),
                                     NULL);
  gtk_ui_manager_insert_action_group(main_uimanager,
                                     radio_action_group_miscellaneous_unit, 0);

  my_snprintf(buf, sizeof(buf), "%s",
              "<menu action=\"MISCELLANEOUS\">\n"
              "<menuitem action=\"MISCELLANEOUS_AIR_PATROL\" />\n"
              "<menuitem action=\"MISCELLANEOUS_AIR_PATROL_DEST\" />\n"
              "<menuitem action=\"MISCELLANEOUS_PATROL_EXECUTE\" />\n"
              "<separator/>\n"
              "<menu action=\"MISCELLANEOUS_CARAVAN\">\n"
              "<menuitem action=\"MISCELLANEOUS_CARAVAN_POPUP\" />\n"
              "<menuitem action=\"MISCELLANEOUS_CARAVAN_TRADE\" />\n"
              "<menuitem action=\"MISCELLANEOUS_CARAVAN_BUILD\" />\n"
              "<menuitem action=\"MISCELLANEOUS_CARAVAN_NOTHING\" />\n"
              "</menu>\n"
              "<menu action=\"MISCELLANEOUS_DIPLOMAT\">\n"
              "<menuitem action=\"MISCELLANEOUS_DIPLOMAT_POPUP\" />\n"
              "<menuitem action=\"MISCELLANEOUS_DIPLOMAT_BRIBE\" />\n"
              "<menuitem action=\"MISCELLANEOUS_DIPLOMAT_SABOTAGE_UNIT\" />\n"
              "<menuitem action=\"MISCELLANEOUS_DIPLOMAT_EMBASSY\" />\n"
              "<menuitem action=\"MISCELLANEOUS_DIPLOMAT_INVESTIGATE\" />\n"
              "<menuitem action=\"MISCELLANEOUS_DIPLOMAT_SABOTAGE_CITY\" />\n"
              "<menuitem action=\"MISCELLANEOUS_DIPLOMAT_STEAL\" />\n"
              "<menuitem action=\"MISCELLANEOUS_DIPLOMAT_INCITE\" />\n"
              "<menuitem action=\"MISCELLANEOUS_DIPLOMAT_POISON\" />\n"
              "<menuitem action=\"MISCELLANEOUS_DIPLOMAT_NOTHING\" />\n"
              "<separator/>\n"
              "<menuitem action=\"MISCELLANEOUS_DIPLOMAT_IGNORE_ALLIES\" />\n"
              "</menu>\n"
              "<separator/>\n"
              "<menu action=\"MISCELLANEOUS_UNIT\">\n"
              "<menuitem action=\"MISCELLANEOUS_UNIT_IDLE\" />\n"
              "<menuitem action=\"MISCELLANEOUS_UNIT_SENTRY\" />\n"
              "<menuitem action=\"MISCELLANEOUS_UNIT_FORTIFY\" />\n"
              "<menuitem action=\"MISCELLANEOUS_UNIT_SLEEP\" />\n"
              "<menuitem action=\"MISCELLANEOUS_UNIT_FORTIFY_OR_SLEEP\" />\n"
              "</menu>\n"
              "<menuitem action=\"MISCELLANEOUS_UNIT_LOCK\" />\n"
              "<menuitem action=\"MISCELLANEOUS_UNIT_MILITARY\" />\n"
              "<separator/>\n"
              "<menuitem action=\"MISCELLANEOUS_TOGGLE_WAKEUP\" />\n"
              "<menuitem action=\"MISCELLANEOUS_TOGGLE_MOVEANDATTACK\" />\n"
              "<separator/>\n"
              "<menuitem action=\"MISCELLANEOUS_SET_RALLIES\" />\n"
              "<menuitem action=\"MISCELLANEOUS_CLEAR_RALLIES\" />\n"
              "<separator/>\n"
              "<menuitem action=\"MISCELLANEOUS_MY_AI_EXECUTE\" />\n"
              "<menuitem action=\"MISCELLANEOUS_MY_AI_FREE\" />\n"
              "<menuitem action=\"MISCELLANEOUS_MY_AI_EXECUTE_ALL\" />\n"
              "</menu>\n");

  freelog(LOG_MENU, "Strlen of buf = %d; Size of load_menu_miscellaneous %d",
          strlen(buf), sizeof(buf));

  return buf;
}

/****************************************************************
  ...
*****************************************************************/
static void callback_report_cities(GtkAction *action, gpointer user_data)
{
  raise_city_report_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_report_units(GtkAction *action, gpointer user_data)
{
  raise_activeunits_report_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_report_players(GtkAction *action, gpointer user_data)
{
  raise_players_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_report_economy(GtkAction *action, gpointer user_data)
{
  raise_economy_report_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_report_science(GtkAction *action, gpointer user_data)
{
  raise_science_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_report_wow(GtkAction *action, gpointer user_data)
{
  send_report_request(REPORT_WONDERS_OF_THE_WORLD);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_report_top_cities(GtkAction *action, gpointer user_data)
{
  send_report_request(REPORT_TOP_5_CITIES);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_report_messages(GtkAction *action, gpointer user_data)
{
  raise_meswin_dialog();
}

/****************************************************************
  ...
*****************************************************************/
static void callback_report_demographic(GtkAction *action, gpointer user_data)
{
  send_report_request(REPORT_DEMOGRAPHIC);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_report_spaceship(GtkAction *action, gpointer user_data)
{
  popup_spaceship_dialog(get_player_ptr());
}

/****************************************************************
  ...
*****************************************************************/
static const char *load_menu_report(void)
{
  static char buf[1024];

  GtkActionEntry entries_report[] = {
    {"REPORT", NULL, N_("_Reports"), NULL, NULL, NULL},
    {"REPORT_CITIES", NULL, N_("_Cities"),
     "F1", N_("_Cities"), G_CALLBACK(callback_report_cities)},
    {"REPORT_UNITS", NULL, N_("_Units"),
     "F2", N_("_Units"), G_CALLBACK(callback_report_units)},
    {"REPORT_PLAYERS", NULL, N_("_Players"),
     "F3", N_("_Players"), G_CALLBACK(callback_report_players)},
    {"REPORT_ECONOMY", NULL, N_("_Economy"),
     "F5", N_("_Economy"), G_CALLBACK(callback_report_economy)},
    {"REPORT_SCIENCE", NULL, N_("_Science"),
     "F6", N_("_Science"), G_CALLBACK(callback_report_science)},
    {"REPORT_WOW", NULL, N_("_Wonders of the World"),
     "F7", N_("_Wonders of the World"), G_CALLBACK(callback_report_wow)},
    {"REPORT_TOP_CITIES", NULL, N_("_Top Five Cities"),
     "F8", N_("_Top Five Cities"), G_CALLBACK(callback_report_top_cities)},
    {"REPORT_MESSAGES", NULL, N_("_Messages"),
     "<Control>F9", N_("_Messages"), G_CALLBACK(callback_report_messages)},
    {"REPORT_DEMOGRAPHIC", NULL, N_("_Demographics"),
     "<Control>F11", N_("_Demographics"),
     G_CALLBACK(callback_report_demographic)},
    {"REPORT_SPACESHIP", NULL, N_("S_paceship"),
     "<Control>F12", N_("S_paceship"), G_CALLBACK(callback_report_spaceship)}
  };

  action_group_report = gtk_action_group_new("GroupReport");
  gtk_action_group_set_translation_domain(action_group_report,
                                          textdomain(PACKAGE));
  gtk_action_group_add_actions(action_group_report, entries_report,
                               G_N_ELEMENTS(entries_report),
                               NULL);
  gtk_ui_manager_insert_action_group(main_uimanager, action_group_report, 0);

  my_snprintf(buf, sizeof(buf), "%s",
              "<menu action=\"REPORT\">\n"
              "<menuitem action=\"REPORT_CITIES\" />\n"
              "<menuitem action=\"REPORT_UNITS\" />\n"
              "<menuitem action=\"REPORT_PLAYERS\" />\n"
              "<menuitem action=\"REPORT_ECONOMY\" />\n"
              "<menuitem action=\"REPORT_SCIENCE\" />\n"
              "<separator/>\n"
              "<menuitem action=\"REPORT_WOW\" />\n"
              "<menuitem action=\"REPORT_TOP_CITIES\" />\n"
              "<menuitem action=\"REPORT_MESSAGES\" />\n"
              "<menuitem action=\"REPORT_DEMOGRAPHIC\" />\n"
              "<menuitem action=\"REPORT_SPACESHIP\" />\n"
              "</menu>\n");

  freelog(LOG_MENU, "Strlen of buf = %d; Size of load_menu_report %d",
          strlen(buf), sizeof(buf));

  return buf;
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_languages(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_LANGUAGES_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_connecting(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_CONNECTING_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_controls(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_CONTROLS_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_chatline(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_CHATLINE_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_worklist_editor(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_WORKLIST_EDITOR_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_cma(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_CMA_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_playing(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_PLAYING_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_warclient(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_WARCLIENT_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_improvements(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_IMPROVEMENTS_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_units(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_UNITS_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_combat(GtkAction *action, gpointer user_data)
{

  popup_help_dialog_string(HELP_COMBAT_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_zoc(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_ZOC_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_tech(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_TECHS_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_terrain(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_TERRAIN_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_wonders(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_WONDERS_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_government(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_GOVERNMENT_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_happiness(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_HAPPINESS_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_space_race(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_SPACE_RACE_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_copying(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_COPYING_ITEM);
}

/****************************************************************
  ...
*****************************************************************/
static void callback_help_about(GtkAction *action, gpointer user_data)
{
  popup_help_dialog_string(HELP_ABOUT_ITEM);
}


/****************************************************************
  ...
*****************************************************************/
static const char *load_menu_help(void)
{
  static char buf[1024];

  GtkActionEntry entries_help[] = {
    {"HELP", NULL, N_("_Help"), NULL, NULL, NULL},
    {"HELP_LANGUAGES", NULL, N_("Language_s"),
     NULL, N_("Language_s"), G_CALLBACK(callback_help_languages)},
    {"HELP_CONNECTING", NULL, N_("Co_nnecting"),
     NULL, N_("Co_nnecting"), G_CALLBACK(callback_help_connecting)},
    {"HELP_CONTROLS", NULL, N_("C_ontrols"),
     NULL, N_("C_ontrols"), G_CALLBACK(callback_help_controls)},
    {"HELP_CHATLINE", NULL, N_("C_hatline"),
     NULL, N_("C_hatline"), G_CALLBACK(callback_help_chatline)},
    {"HELP_WORKLIST_EDITOR", NULL, N_("_Worklist Editor"),
     NULL, N_("_Worklist Editor"), G_CALLBACK(callback_help_worklist_editor)},
    {"HELP_CMA", NULL, N_("Citizen _Management"),
     NULL, N_("Citizen _Management"), G_CALLBACK(callback_help_cma)},
    {"HELP_PLAYING", NULL, N_("_Playing"),
     NULL, N_("_Playing"), G_CALLBACK(callback_help_playing)},
    {"HELP_WARCLIENT", NULL, N_("_Warclient - PepClient"),
     NULL, N_("_Warclient - PepClient"), G_CALLBACK(callback_help_warclient)},
    {"HELP_IMPROVEMENTS", NULL, N_("City _Improvements"),
     NULL, N_("City _Improvements"), G_CALLBACK(callback_help_improvements)},
    {"HELP_UNITS", NULL, N_("_Units"),
     NULL, N_("_Units"), G_CALLBACK(callback_help_units)},
    {"HELP_COMBAT", NULL, N_("Com_bat"),
     NULL, N_("Com_bat"), G_CALLBACK(callback_help_combat)},
    {"HELP_ZOC", NULL, N_("_ZOC"),
     NULL, N_("_ZOC"), G_CALLBACK(callback_help_zoc)},
    {"HELP_TECH", NULL, N_("Techno_logy"),
     NULL, N_("Techno_logy"), G_CALLBACK(callback_help_tech)},
    {"HELP_TERRAIN", NULL, N_("_Terrain"),
     NULL, N_("_Terrain"), G_CALLBACK(callback_help_terrain)},
    {"HELP_WONDERS", NULL, N_("Won_ders"),
     NULL, N_("Won_ders"), G_CALLBACK(callback_help_wonders)},
    {"HELP_GOVERNMENT", NULL, N_("_Government"),
     NULL, N_("_Government"), G_CALLBACK(callback_help_government)},
    {"HELP_HAPPINESS", NULL, N_("Happin_ess"),
     NULL, N_("Happin_ess"), G_CALLBACK(callback_help_happiness)},
    {"HELP_SPACE_RACE", NULL, N_("Space _Race"),
     NULL, N_("Space _Race"), G_CALLBACK(callback_help_space_race)},
    {"HELP_COPYING", NULL, N_("_Copying"),
     NULL, N_("_Copying"), G_CALLBACK(callback_help_copying)},
    {"HELP_ABOUT", NULL, N_("_About"),
     NULL, N_("_About"), G_CALLBACK(callback_help_about)}
  };

  action_group_help = gtk_action_group_new("GroupHelp");
  gtk_action_group_set_translation_domain(action_group_help,
                                          textdomain(PACKAGE));
  gtk_action_group_add_actions(action_group_help, entries_help,
                               G_N_ELEMENTS(entries_help),
                               NULL);
  gtk_ui_manager_insert_action_group(main_uimanager, action_group_help, 0);

  my_snprintf(buf, sizeof(buf), "%s",
              "<menu action=\"HELP\">\n"
              "<menuitem action=\"HELP_LANGUAGES\" />\n"
              "<menuitem action=\"HELP_CONNECTING\" />\n"
              "<menuitem action=\"HELP_CONTROLS\" />\n"
              "<menuitem action=\"HELP_CHATLINE\" />\n"
              "<menuitem action=\"HELP_WORKLIST_EDITOR\" />\n"
              "<menuitem action=\"HELP_CMA\" />\n"
              "<menuitem action=\"HELP_PLAYING\" />\n"
              "<menuitem action=\"HELP_WARCLIENT\" />\n"
              "<separator/>\n"
              "<menuitem action=\"HELP_IMPROVEMENTS\" />\n"
              "<menuitem action=\"HELP_UNITS\" />\n"
              "<menuitem action=\"HELP_COMBAT\" />\n"
              "<menuitem action=\"HELP_ZOC\" />\n"
              "<menuitem action=\"HELP_TECH\" />\n"
              "<menuitem action=\"HELP_TERRAIN\" />\n"
              "<menuitem action=\"HELP_WONDERS\" />\n"
              "<menuitem action=\"HELP_GOVERNMENT\" />\n"
              "<menuitem action=\"HELP_HAPPINESS\" />\n"
              "<menuitem action=\"HELP_SPACE_RACE\" />\n"
              "<separator/>\n"
              "<menuitem action=\"HELP_COPYING\" />\n"
              "<menuitem action=\"HELP_ABOUT\" />\n"
              "</menu>\n");

  freelog(LOG_MENU, "Strlen of buf = %d; Size of load_menu_help %d",
          strlen(buf), sizeof(buf));

  return buf;
}

/****************************************************************
...
*****************************************************************/
static void callback_addwidget(GtkUIManager *main_uimanager,
                               GtkWidget *p_widget,
                               GtkContainer *p_box)
{
  gtk_box_pack_start(GTK_BOX(p_box), p_widget, TRUE, TRUE, 0);
  gtk_widget_show(p_widget);
}

/****************************************************************
  http://library.gnome.org/devel/gtk/stable/GtkActionGroup.html
*****************************************************************/
void setup_menus(GtkWidget *window, GtkWidget **menubar)
{
  char xmlmenu[32768];
  GError **error = NULL;
  GtkWidget *tabulation;

  /*   widget for menu */
  main_menubar = gtk_hbox_new(FALSE, 0);
  main_uimanager = gtk_ui_manager_new();

  /*   add tearoff in menus */
  gtk_ui_manager_set_add_tearoffs(main_uimanager, TRUE);


  my_snprintf(xmlmenu, sizeof(xmlmenu),
              "<ui>\n"
              "<menubar>\n");

  /*   load group action for each items */
  cat_snprintf(xmlmenu, sizeof(xmlmenu),"%s", load_menu_game());
  cat_snprintf(xmlmenu, sizeof(xmlmenu),"%s", load_menu_government());
  cat_snprintf(xmlmenu, sizeof(xmlmenu),"%s", load_menu_view());
  cat_snprintf(xmlmenu, sizeof(xmlmenu),"%s", load_menu_order());
  cat_snprintf(xmlmenu, sizeof(xmlmenu),"%s", load_menu_delayed_goto());
  cat_snprintf(xmlmenu, sizeof(xmlmenu),"%s", load_menu_airlift());
  cat_snprintf(xmlmenu, sizeof(xmlmenu),"%s", load_menu_auto_caravan());
  cat_snprintf(xmlmenu, sizeof(xmlmenu),"%s", load_menu_multi_selection());
  cat_snprintf(xmlmenu, sizeof(xmlmenu),"%s", load_menu_miscellaneous());
  cat_snprintf(xmlmenu, sizeof(xmlmenu),"%s", load_menu_report());
  cat_snprintf(xmlmenu, sizeof(xmlmenu),"%s", load_menu_help());

  cat_snprintf(xmlmenu, sizeof(xmlmenu),
               "</menubar>\n"
               "</ui>\n");

  freelog(LOG_MENU, "Strlen of buf = %d; Size of xmlmenu %d",
          strlen(xmlmenu), sizeof(xmlmenu));

  /*   read menu structure from file */
  gtk_ui_manager_add_ui_from_string(main_uimanager,
                                    (const gchar*) g_locale_to_utf8(xmlmenu,
                                                                    -1,
                                                                    NULL,
                                                                    NULL,
                                                                    NULL),
                                    strlen(xmlmenu) -1,
                                    error);

  /*   enable shortcuts */
  gtk_window_add_accel_group(GTK_WINDOW(window),
                             gtk_ui_manager_get_accel_group(main_uimanager));

  /*   draw menu */
  g_signal_connect(main_uimanager, "add_widget",
                   G_CALLBACK(callback_addwidget), main_menubar);

  tabulation = gtk_ui_manager_get_widget(main_uimanager,
                                         "/ui/menubar/HELP");
  gtk_menu_item_set_right_justified(GTK_MENU_ITEM(tabulation), TRUE);

  if (menubar) {
    *menubar = main_menubar;
  }

  update_menus();
}

/****************************************************************************
  Return the text for the tile, changed by the activity.

  Should only be called for irrigation, mining, or transformation, and
  only when the activity changes the base terrain type.
****************************************************************************/
static const char *get_tile_change_menu_text(struct tile *ptile,
                                             enum unit_activity activity)
{
  Terrain_type_id old_terrain = ptile->terrain;
  enum tile_special_type old_special = ptile->special;
  struct tile_type *ptype = get_tile_type(ptile->terrain);
  const char *text;

  /* Change the terrain manually to avoid any side effects. */
  switch (activity) {
  case ACTIVITY_IRRIGATE:
    if (ptype->irrigation_result == ptile->terrain
        || ptype->irrigation_result == T_NONE) {
      assert(0);
    }
    map_irrigate_tile(ptile);
    break;

  case ACTIVITY_MINE:
    if (ptype->mining_result == ptile->terrain
        || ptype->mining_result == T_NONE) {
      assert(0);
    }
    map_mine_tile(ptile);
    break;

  case ACTIVITY_TRANSFORM:
    if (ptype->transform_result == ptile->terrain
        || ptype->transform_result == T_NONE) {
      assert(0);
    }
    map_transform_tile(ptile);
    break;

  default:
    assert(0);
    return "-";
  }

  text = map_get_tile_info_text(ptile);

  /* Restore the original state of the tile. */
  ptile->terrain = old_terrain;
  ptile->special = old_special;
  reset_move_costs(ptile);

  return text;
}

/****************************************************************
...
*****************************************************************/
static void menu_rename(GtkActionGroup *paction_group,
                        const char *actionname,
                        const char *s)
{
  GtkAction *action;

  action = gtk_action_group_get_action(paction_group, actionname);

  if (!action) {
    freelog(LOG_ERROR, "Can't rename non-existent menu %s.",
            actionname);
    return;
  }

  g_object_set(G_OBJECT(action), "label", s, NULL);
}

/****************************************************************
...
*****************************************************************/
static void menu_set_visible(GtkActionGroup *paction_group,
                             const char *actionname,
                             bool visible)
{
  GtkAction *action;

  action = gtk_action_group_get_action(paction_group, actionname);

  if (!action) {
    freelog(LOG_ERROR, "Can't set visibility non-existent menu %s.",
            actionname);
    return;
  }

  gtk_action_set_visible(action, visible);
}

/****************************************************************
...
*****************************************************************/
void update_menus(void)
{
  char buf[256];

  if (!main_menubar || !can_client_change_view()) {
    return;
  }

  if (can_client_change_view()) {
    int i;
    struct unit *punit;
    bool cond;

    /* Update governements available. */
    government_iterate(g) {
      if (g->index != game.ruleset_control.government_when_anarchy) {
         my_snprintf(buf, sizeof(buf), "GOVERNMENT_TYPE_%d", g->index);
         menu_set_sensitive(action_group_government_type, buf, 
                            can_change_to_government(get_player_ptr(), 
                                                     g->index));
      } government_iterate_end;
    }

    menu_set_sensitive(action_group_report,
                       "REPORT_SPACESHIP",
                       (get_player_ptr()->spaceship.state!=SSHIP_NONE));

    menu_toggle_set_sensitive(toggle_action_group_view,
                              "VIEW_SHOW_CITY_GROWTH_TURNS",
                              draw_city_names);
    menu_toggle_set_sensitive(toggle_action_group_view,
                              "VIEW_SHOW_COASTLINE",
                              !draw_terrain);
    menu_toggle_set_sensitive(toggle_action_group_view,
                              "VIEW_SHOW_FOCUS_UNIT",
                              !draw_units);

    for (i = 1; i < MULTI_SELECT_NUM; i++) {
      update_multi_selection_menu(i);
    }

    /*      This should be done in a future version for all warclient menu items */
    /*     menu_set_sensitive(action_group_miscellaneous,  */
    /*          "MISCELLANEOUS_SET_RALLIES",  */
    /*          tiles_hilited_cities); */
    /*     Remaining part of this function: Update Orders menu */

    if (!can_client_issue_orders()) {
      return;
    }

    if ((punit = get_unit_in_focus())) {
      const char *irrfmt = _("Change to %s (_I)");
      const char *minfmt = _("Change to %s (_M)");
      const char *transfmt = _("Transf_orm to %s");
      char irrtext[128], mintext[128], transtext[128];
      const char *roadtext;
      Terrain_type_id  ttype;
      struct tile_type *      tinfo;

      sz_strlcpy(irrtext, _("Build _Irrigation"));
      sz_strlcpy(mintext, _("Build _Mine"));
      sz_strlcpy(transtext, _("Transf_orm Terrain"));

      /* Enable the button for adding to a city in all cases, so we
         get an eventual error message from the server if we try. */
      menu_set_sensitive(action_group_order,
                         "ORDER_BUILD_CITY",
                         can_unit_add_or_build_city(punit) ||
                         unit_can_help_build_wonder_here(punit));
      menu_set_sensitive(action_group_order,
                         "ORDER_ROAD",
                         (can_unit_do_activity(punit, ACTIVITY_ROAD) ||
                          can_unit_do_activity(punit, ACTIVITY_RAILROAD) ||
                          unit_can_est_traderoute_here(punit)));
      menu_set_sensitive(action_group_order,
                         "ORDER_IRRIGATE",
                         can_unit_do_activity(punit, ACTIVITY_IRRIGATE));
      menu_set_sensitive(action_group_order,
                         "ORDER_MINE",
                         can_unit_do_activity(punit, ACTIVITY_MINE));
      menu_set_sensitive(action_group_order,
                         "ORDER_TRANSFORM",
                         can_unit_do_activity(punit, ACTIVITY_TRANSFORM));
      menu_set_sensitive(action_group_order,
                         "ORDER_FORTRESS",
                         (can_unit_do_activity(punit, ACTIVITY_FORTRESS) ||
                          can_unit_do_activity(punit, ACTIVITY_FORTIFYING)));
      menu_set_sensitive(action_group_order,
                         "ORDER_AIRBASE",
                         can_unit_do_activity(punit, ACTIVITY_AIRBASE));
      menu_set_sensitive(action_group_order,
                         "ORDER_POLLUTION",
                         (can_unit_do_activity(punit, ACTIVITY_POLLUTION) ||
                          can_unit_paradrop(punit)));
      menu_set_sensitive(action_group_order,
                         "ORDER_FALLOUT",
                         can_unit_do_activity(punit, ACTIVITY_FALLOUT));
      menu_set_sensitive(action_group_order,
                         "ORDER_SENTRY",
                         can_unit_do_activity(punit, ACTIVITY_SENTRY));
      menu_set_sensitive(action_group_order,
                         "ORDER_PILLAGE",
                         can_unit_do_activity(punit, ACTIVITY_PILLAGE));
      menu_set_sensitive(action_group_order,
                         "ORDER_DISBAND",
                         !unit_flag(punit, F_UNDISBANDABLE));
      menu_set_sensitive(action_group_order,
                         "ORDER_HOMECITY",
                         can_unit_change_homecity(punit));
      menu_set_sensitive(action_group_order,
                         "ORDER_UNLOAD_TRANSPORTER",
                         get_transporter_occupancy(punit) > 0);
      menu_set_sensitive(action_group_order,
                         "ORDER_LOAD",
                         can_unit_load(punit,
                                       find_transporter_for_unit(punit,
                                                                 punit->tile)));
      menu_set_sensitive(action_group_order,
                         "ORDER_UNLOAD",
                         (can_unit_unload(punit,
                                          find_unit_by_id(punit->transported_by))
                          && can_unit_exist_at_tile(punit, punit->tile)));
      menu_set_sensitive(action_group_order,
                         "ORDER_WAKEUP_OTHERS",
                         is_unit_activity_on_tile(ACTIVITY_SENTRY,
                                                  punit->tile));
      menu_set_sensitive(action_group_order,
                         "ORDER_AUTO_SETTLER",
                         can_unit_do_auto(punit));
      menu_set_sensitive(action_group_order,
                         "ORDER_AUTO_EXPLORE",
                         can_unit_do_activity(punit, ACTIVITY_EXPLORE));
      menu_set_sensitive(action_group_order,
                         "ORDER_CONNECT_ROAD",
                         can_unit_do_connect(punit, ACTIVITY_ROAD));
      menu_set_sensitive(action_group_order,
                         "ORDER_CONNECT_RAIL",
                         can_unit_do_connect(punit, ACTIVITY_RAILROAD));
      menu_set_sensitive(action_group_order,
                         "ORDER_CONNECT_IRRIGATE",
                         can_unit_do_connect(punit, ACTIVITY_IRRIGATE));
      menu_set_sensitive(action_group_order,
                         "ORDER_RETURN",
                         !(is_air_unit(punit) || is_heli_unit(punit)));
      menu_set_sensitive(action_group_order,
                         "ORDER_DIPLOMAT_DLG",
                         (is_diplomat_unit(punit) &&
                          diplomat_can_do_action(punit,
                                                 DIPLOMAT_ANY_ACTION,
                                                 punit->tile)));
      menu_set_sensitive(action_group_order,
                         "ORDER_NUKE",
                         unit_flag(punit, F_NUCLEAR));
      if (unit_flag(punit, F_HELP_WONDER)) {
        menu_rename(action_group_order,
                    "ORDER_BUILD_CITY",
                    _("Help _Build Wonder"));
      } else if (unit_flag(punit, F_CITIES)) {
        if (map_get_city(punit->tile)) {
          menu_rename(action_group_order,
                      "ORDER_BUILD_CITY", _("Add to City (_B)"));
        } else {
          menu_rename(action_group_order,
                      "ORDER_BUILD_CITY", _("_Build City"));
        }
      } else {
        menu_rename(action_group_order,
                    "ORDER_BUILD_CITY", _("_Build City"));
      }

      if (unit_flag(punit, F_TRADE_ROUTE)) {
        menu_rename(action_group_order,
                    "ORDER_ROAD",
                    _("Make Trade _Route"));
      } else if (unit_flag(punit, F_SETTLERS)) {
        if (map_has_special(punit->tile, S_ROAD)) {
          roadtext = _("Build _Railroad");
          road_activity=ACTIVITY_RAILROAD;
        } else {
          roadtext = _("Build _Road");
          road_activity=ACTIVITY_ROAD;
        }
        menu_rename(action_group_order,
                    "ORDER_ROAD", roadtext);
      } else {
        menu_rename(action_group_order,
                    "ORDER_ROAD", _("Build _Road"));
      }

      ttype = punit->tile->terrain;
      tinfo = get_tile_type(ttype);
      if (tinfo->irrigation_result != T_NONE
          && tinfo->irrigation_result != ttype) {
        my_snprintf(irrtext, sizeof(irrtext), irrfmt,
                    get_tile_change_menu_text(punit->tile,
                                              ACTIVITY_IRRIGATE));
      } else if (map_has_special(punit->tile, S_IRRIGATION)
                 && player_knows_techs_with_flag(get_player_ptr(),
                                                 TF_FARMLAND)) {
        sz_strlcpy(irrtext, _("Bu_ild Farmland"));
      }
      if (tinfo->mining_result != T_NONE
          && tinfo->mining_result != ttype) {
        my_snprintf(mintext, sizeof(mintext), minfmt,
                    get_tile_change_menu_text(punit->tile, ACTIVITY_MINE));
      }
      if (tinfo->transform_result != T_NONE
          && tinfo->transform_result != ttype) {
        my_snprintf(transtext, sizeof(transtext), transfmt,
                    get_tile_change_menu_text(punit->tile,
                                              ACTIVITY_TRANSFORM));
      }
      menu_rename(action_group_order,
                  "ORDER_IRRIGATE", irrtext);
      menu_rename(action_group_order,
                  "ORDER_MINE", mintext);
      menu_rename(action_group_order,
                  "ORDER_TRANSFORM", transtext);

      if (can_unit_do_activity(punit, ACTIVITY_FORTIFYING)) {
        menu_rename(action_group_order,
                    "ORDER_FORTRESS", _("_Fortify"));
        menu_set_sensitive(action_group_order,
                           "ORDER_SLEEP", FALSE);
      } else {
        menu_set_sensitive(action_group_order,
                           "ORDER_SLEEP", TRUE);
        if (can_unit_do_activity(punit, ACTIVITY_FORTRESS)) {
          menu_rename(action_group_order,
                      "ORDER_FORTRESS", _("Build _Fortress"));
        }
      }

      if (unit_flag(punit, F_PARATROOPERS)) {
        menu_rename(action_group_order,
                    "ORDER_POLLUTION", _("_Paradrop"));
      } else {
        menu_rename(action_group_order,
                    "ORDER_POLLUTION", _("Clean _Pollution"));
      }

      if (!unit_flag(punit, F_SETTLERS)) {
        menu_rename(action_group_order,
                    "ORDER_AUTO_SETTLER", _("_Auto Attack"));
      } else {
        menu_rename(action_group_order,
                    "ORDER_AUTO_SETTLER", _("_Auto Settler"));
      }

      menu_set_sensitive(action_group_delayed_goto,
                         "DELAYED_GOTO_DELAYED_GOTO", TRUE);
      menu_set_sensitive(action_group_delayed_goto,
                         "DELAYED_GOTO_DELAYED_PARADROP_OR_NUKE", TRUE);

      cond = (my_ai_enable && unit_type(punit)->fuel > 0);
      menu_set_sensitive(action_group_miscellaneous,
                         "MISCELLANEOUS_AIR_PATROL",
                         cond &&
                         !can_unit_do_activity(punit, ACTIVITY_AIRBASE));
      menu_set_sensitive(action_group_miscellaneous,
                         "MISCELLANEOUS_AIR_PATROL_DEST",
                         cond);
      menu_set_sensitive(action_group_multi_selection,
                         "MULTI_SELECTION_MS_SELECT",
                         TRUE);
      menu_set_sensitive(action_group_multi_selection,
                         "MULTI_SELECTION_MS_ACTIVE_ALL", TRUE);
      menu_set_sensitive(action_group_multi_selection,
                         "MULTI_SELECTION_MS_CLEAR",
                         TRUE);
      menu_set_sensitive(action_group_multi_selection,
                         "MULTI_SELECTION_MY_AI_SPREAD",
                         TRUE);

      cond = (my_ai_trade_level && unit_flag(punit, F_TRADE_ROUTE));
      menu_set_sensitive(action_group_auto_caravan,
                         "AUTO_CARAVAN_MY_AI_TRADE_WITH",
                         my_ai_enable && cond);
      menu_set_sensitive(action_group_auto_caravan,
                         "AUTO_CARAVAN_MY_AI_CARAVAN",
                         my_ai_enable && cond);

      menu_set_sensitive(action_group_miscellaneous,
                         "MISCELLANEOUS_MY_AI_FREE",
                         punit->my_ai.control);
      menu_set_sensitive(action_group_miscellaneous,
                         "MISCELLANEOUS_MY_AI_EXECUTE",
                         my_ai_enable && punit->my_ai.control);

      menu_set_sensitive(action_group_order,
                         "ORDER", TRUE);
    } else {
      menu_set_sensitive(action_group_delayed_goto,
                         "DELAYED_GOTO_DELAYED_GOTO", FALSE);
      menu_set_sensitive(action_group_delayed_goto,
                         "DELAYED_GOTO_DELAYED_PARADROP_OR_NUKE",
                         FALSE);

      menu_set_sensitive(action_group_miscellaneous,
                         "MISCELLANEOUS_AIR_PATROL", FALSE);
      menu_set_sensitive(action_group_miscellaneous,
                         "MISCELLANEOUS_AIR_PATROL_DEST", FALSE);
      menu_set_sensitive(action_group_multi_selection,
                         "MULTI_SELECTION_MS_SELECT",
                         FALSE);
      menu_set_sensitive(action_group_multi_selection,
                         "MULTI_SELECTION_MS_ACTIVE_ALL", FALSE);
      menu_set_sensitive(action_group_multi_selection,
                         "MULTI_SELECTION_MS_CLEAR", FALSE);
      menu_set_sensitive(action_group_multi_selection,
                         "MULTI_SELECTION_MY_AI_SPREAD", FALSE);

      menu_set_sensitive(action_group_auto_caravan,
                         "AUTO_CARAVAN_MY_AI_CARAVAN", FALSE);
      menu_set_sensitive(action_group_auto_caravan,
                         "AUTO_CARAVAN_MY_AI_TRADE_WITH", FALSE);

      menu_set_sensitive(action_group_miscellaneous,
                         "MISCELLANEOUS_MY_AI_FREE", FALSE);
      menu_set_sensitive(action_group_miscellaneous,
                         "MISCELLANEOUS_MY_AI_EXECUTE", FALSE);

      menu_set_sensitive(action_group_order, "ORDER", FALSE);
    }
  }
}

/****************************************************************
  ...
*****************************************************************/
static void menu_airlift_set_active(void)
{
  int i;
  const char *buf;

  for(i = 0; i < AIRLIFT_QUEUE_NUM; i++)
    {
      buf = airlift_queue_get_menu_name(i, airlift_queue_get_unit_type(i));
      menu_radio_set_active(radio_action_group_airlift_unit[i], buf);
    }
}

/****************************************************************
  Update the menus about the delayed goto queues.
*****************************************************************/
void update_delayed_goto_menu(int i)
{
  if (!can_client_issue_orders() || !main_menubar) {
    return;
  }

  assert(i >= 0 && i < DELAYED_GOTO_NUM);

  char buf[256], *m;
  bool cond = delayed_goto_size(0) > 0;

  if (i == 0) {
    menu_set_sensitive(action_group_delayed_goto,
                       "DELAYED_GOTO_EXECUTE_DELAYED_GOTO", cond);
    menu_set_sensitive(action_group_delayed_goto,
                       "DELAYED_GOTO_CLEAR_DELAYED_ORDERS", cond);
    for (i = 1; i < DELAYED_GOTO_NUM; i++) {
      update_delayed_goto_menu(i);
    }
    return;
  } else {
    m = buf + my_snprintf(buf, sizeof(buf),
                          "DELAYED_GOTO_DG%d", i);
    if (delayed_goto_size(i) > 0) {
      menu_set_sensitive(action_group_delayed_goto,
                         buf, TRUE);
      strcpy(m, "_SELECT");
      menu_set_sensitive(action_group_delayed_goto,
                         buf, TRUE);
      strcpy(m, "_ADD");
      menu_set_sensitive(action_group_delayed_goto,
                         buf, TRUE);
      strcpy(m, "_RECORD");
      menu_set_sensitive(action_group_delayed_goto,
                         buf, cond);
      strcpy(m, "_CLEAR");
      menu_set_sensitive(action_group_delayed_goto,
                         buf, TRUE);
      strcpy(m, "_EXECUTE");
      menu_set_sensitive(action_group_delayed_goto,
                         buf, TRUE);
    } else if (cond) {
      menu_set_sensitive(action_group_delayed_goto,
                         buf, TRUE);
      strcpy(m, "_SELECT");
      menu_set_sensitive(action_group_delayed_goto,
                         buf, FALSE);
      strcpy(m, "_ADD");
      menu_set_sensitive(action_group_delayed_goto,
                         buf, FALSE);
      strcpy(m, "_RECORD");
      menu_set_sensitive(action_group_delayed_goto,
                         buf, TRUE);
      strcpy(m, "_CLEAR");
      menu_set_sensitive(action_group_delayed_goto,
                         buf, FALSE);
      strcpy(m, "_EXECUTE");
      menu_set_sensitive(action_group_delayed_goto,
                         buf, FALSE);
    } else {
      menu_set_sensitive(action_group_delayed_goto,
                         buf, FALSE);
    }
  }
}

/****************************************************************
  Update the menus about the trade planning.
*****************************************************************/
void update_auto_caravan_menu(void)
{
  if (!can_client_issue_orders() || !main_menubar) {
    return;
  }

  bool cond = city_list_size(my_ai_get_trade_cities()) > 0;

  menu_set_sensitive(action_group_auto_caravan,
                     "AUTO_CARAVAN_MY_AI_CLEAR_TRADE_CITY",
                     cond);
  menu_set_sensitive(action_group_auto_caravan,
                     "AUTO_CARAVAN_MY_AI_SHOW_TRADE_CITIES", cond);
  menu_set_sensitive(action_group_auto_caravan,
                     "AUTO_CARAVAN_MY_AI_SHOW_FREE_SLOTS",
                     cond);
  menu_set_sensitive(action_group_auto_caravan,
                     "AUTO_CARAVAN_MY_AI_ESTIMATE_TRADE",
                     (my_ai_trade_level
                      && my_ai_count_activity(MY_AI_TRADE_ROUTE) > 0) ||
                     (my_ai_trade_manual_trade_route_enable &&
                      trade_route_list_size(estimate_non_ai_trade_route()) > 0));
  menu_set_sensitive(action_group_auto_caravan,
                     "AUTO_CARAVAN_MY_AI_TRADE_RECALCULATE", cond);
  menu_set_sensitive(action_group_auto_caravan,
                     "AUTO_CARAVAN_MY_AI_TRADE_EXECUTE",
                     my_ai_trade_level &&
                     my_ai_count_activity(MY_AI_TRADE_ROUTE) > 0);
}

/****************************************************************
...
*****************************************************************/
void init_menus(void)
{
  if (!main_menubar) {
    return;
  }

  bool cond;
  int i;

  cond = (can_client_access_hack() && get_client_state() >=
          CLIENT_GAME_RUNNING_STATE);
  menu_set_sensitive(action_group_game, "GAME_SAVE_GAME", cond);
  menu_set_sensitive(action_group_game, "GAME_SAVE_QUICK", cond);
  menu_set_sensitive(action_group_game, "GAME_SERVER_OPTIONS1",
                     aconnection.established);
  menu_set_sensitive(action_group_game, "GAME_SERVER_OPTIONS2",
                     get_client_state() >= CLIENT_GAME_RUNNING_STATE);
  menu_set_sensitive(action_group_game, "GAME_LEAVE",
                     aconnection.established);

  if (!can_client_change_view()) {
    menu_set_visible(action_group_government, "GOVERNMENT", FALSE);
    menu_set_visible(action_group_view, "VIEW", FALSE);
    menu_set_visible(action_group_order, "ORDER", FALSE);
    menu_set_visible(action_group_delayed_goto, "DELAYED GOTO", FALSE);
    menu_set_visible(action_group_airlift, "AIRLIFT", FALSE);
    menu_set_visible(action_group_auto_caravan, "AUTO_CARAVAN", FALSE);
    menu_set_visible(action_group_multi_selection, "MULTI_SELECTION", FALSE);
    menu_set_visible(action_group_miscellaneous, "MISCELLANEOUS", FALSE);
    menu_set_visible(action_group_report, "REPORT", FALSE);
    return;
  }

  cond = (can_client_issue_orders());
  menu_set_visible(action_group_government, "GOVERNMENT", TRUE);
  menu_set_visible(action_group_view, "VIEW", TRUE);
  menu_set_visible(action_group_order, "ORDER", cond);
  menu_set_visible(action_group_delayed_goto, "DELAYED_GOTO", cond);
  menu_set_visible(action_group_airlift, "AIRLIFT", cond);
  menu_set_visible(action_group_auto_caravan, "AUTO_CARAVAN", cond );
  menu_set_visible(action_group_multi_selection, "MULTI_SELECTION", cond);
  menu_set_visible(action_group_miscellaneous, "MISCELLANEOUS", cond );
  menu_set_visible(action_group_report, "REPORT", TRUE);

  menu_set_sensitive(action_group_government, "GOVERNMENT_TAX_RATE",
                     game.ruleset_game.changable_tax && cond);
  menu_set_sensitive(action_group_government, "GOVERNMENT_WORKLISTS", cond);
  menu_set_sensitive(action_group_government, "GOVERNMENT_CHANGE", cond);

  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_MAP_GRID", draw_map_grid);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_NATIONAL_BORDERS", draw_borders);
  menu_toggle_set_sensitive(toggle_action_group_view,
                            "VIEW_SHOW_NATIONAL_BORDERS",
                            game.ruleset_control.borders > 0);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_CITY_NAMES", draw_city_names);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_CITY_GROWTH_TURNS",
                         draw_city_growth);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_CITY_PRODUCTIONS",
                         draw_city_productions);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_CITY_TRADEROUTES",
                         draw_city_traderoutes);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_TERRAIN", draw_terrain);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_COASTLINE", draw_coastline);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_ROADS_RAILS", draw_roads_rails);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_IRRIGATION", draw_irrigation);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_MINES", draw_mines);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_FORTRESS_AIRBASE",
                         draw_fortress_airbase);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_SPECIALS", draw_specials);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_POLLUTION", draw_pollution);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_CITIES", draw_cities);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_UNITS", draw_units);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_FOCUS_UNIT", draw_focus_unit);
  menu_toggle_set_active(toggle_action_group_view,
                         "VIEW_SHOW_FOG_OF_WAR", draw_fog_of_war);

  /* Delayed goto menu */
  switch (delayed_goto_place) {
  case PLACE_SINGLE_UNIT:
    menu_radio_set_active(radio_action_group_delayed_goto_place,
                          "DELAYED_GOTO_GOTO_SINGLE_UNIT");
    break;
  case PLACE_IN_TRANSPORTER:
    menu_radio_set_active(radio_action_group_delayed_goto_place,
                          "DELAYED_GOTO_GOTO_IN_TRANSPORTER");
    break;
  case PLACE_ON_TILE:
    menu_radio_set_active(radio_action_group_delayed_goto_place,
                          "DELAYED_GOTO_GOTO_ON_TILE");
    break;
  case PLACE_ON_CONTINENT:
    menu_radio_set_active(radio_action_group_delayed_goto_place,
                          "DELAYED_GOTO_GOTO_ON_CONTINENT");
    break;
  case PLACE_EVERY_WHERE:
    menu_radio_set_active(radio_action_group_delayed_goto_place,
                          "DELAYED_GOTO_GOTO_EVERY_WHERE");
    break;
  default:
    break;
  }

  switch (delayed_goto_utype) {
  case UTYPE_SAME_TYPE:
    menu_radio_set_active(radio_action_group_delayed_goto_utype,
                          "DELAYED_GOTO_GOTO_SAME_TYPE");
    break;
  case UTYPE_SAME_MOVE_TYPE:
    menu_radio_set_active(radio_action_group_delayed_goto_utype,
                          "DELAYED_GOTO_GOTO_SAME_MOVE_TYPE");
    break;
  case UTYPE_ALL:
    menu_radio_set_active(radio_action_group_delayed_goto_utype,
                          "DELAYED_GOTO_GOTO_ALL");
    break;
  default:
    break;
  }

  update_delayed_goto_inclusive_filter_menu();
  update_delayed_goto_exclusive_filter_menu();

  for (i = 0; i < DELAYED_GOTO_NUM; i++) {
    update_delayed_goto_menu(i);
  }

  /* Airlift menu */
  for (i = 0; i < AIRLIFT_QUEUE_NUM; i++) {
    update_airlift_menu(i);
  }
  menu_airlift_set_active();

  /* Auto Caravan menu */
  update_auto_caravan_menu();

  /* Multi-Selection menu */
  switch (multi_select_place) {
  case PLACE_SINGLE_UNIT:
    menu_radio_set_active(radio_action_group_multi_selection_place,
                          "MULTI_SELECTION_MODE_SINGLE_UNIT");
    break;
  case PLACE_IN_TRANSPORTER:
    menu_radio_set_active(radio_action_group_multi_selection_place,
                          "MULTI_SELECTION_MODE_IN_TRANSPORTER");
    break;
  case PLACE_ON_TILE:
    menu_radio_set_active(radio_action_group_multi_selection_place,
                          "MULTI_SELECTION_MODE_ON_TILE");
    break;
  case PLACE_ON_CONTINENT:
    menu_radio_set_active(radio_action_group_multi_selection_place,
                          "MULTI_SELECTION_MODE_ON_CONTINENT");
    break;
  case PLACE_EVERY_WHERE:
    menu_radio_set_active(radio_action_group_multi_selection_place,
                          "MULTI_SELECTION_MODE_EVERY_WHERE");
    break;
  default:
    break;
  }

  switch (multi_select_utype) {
  case UTYPE_SAME_TYPE:
    menu_radio_set_active(radio_action_group_multi_selection_utype,
                          "MULTI_SELECTION_MODE_SAME_TYPE");
    break;
  case UTYPE_SAME_MOVE_TYPE:
    menu_radio_set_active(radio_action_group_multi_selection_utype,
                          "MULTI_SELECTION_MODE_SAME_MOVE_TYPE");
    break;
  case UTYPE_ALL:
    menu_radio_set_active(radio_action_group_multi_selection_utype,
                          "MULTI_SELECTION_MODE_ALL");
    break;
  default:
    break;
  }

  menu_toggle_set_active(toggle_action_group_multi_selection,
                         "MULTI_SELECTION_MY_AI_SPREAD_AIRPORT",
                         spread_airport_cities);
  menu_toggle_set_active(toggle_action_group_multi_selection,
                         "MULTI_SELECTION_MY_AI_SPREAD_ALLY",
                         spread_allied_cities);

  update_multi_selection_inclusive_filter_menu();
  update_multi_selection_exclusive_filter_menu();

  /* Miscellaneous menu */
  switch (default_caravan_action) {
  case 0:
    menu_radio_set_active(radio_action_group_miscellaneous_caravan,
                          "MISCELLANEOUS_CARAVAN_POPUP");
    break;
  case 1:
    menu_radio_set_active(radio_action_group_miscellaneous_caravan,
                          "MISCELLANEOUS_CARAVAN_TRADE");
    break;
  case 2:
    menu_radio_set_active(radio_action_group_miscellaneous_caravan,
                          "MISCELLANEOUS_CARAVAN_BUILD");
    break;
  case 3:
    menu_radio_set_active(radio_action_group_miscellaneous_caravan,
                          "MISCELLANEOUS_CARAVAN_NOTHING");
    break;
  default:
    break;
  }

  switch (default_diplomat_action) {
  case 0:
    menu_radio_set_active(radio_action_group_miscellaneous_diplomat,
                          "MISCELLANEOUS_DIPLOMAT_POPUP");
    break;
  case 1:
    menu_radio_set_active(radio_action_group_miscellaneous_diplomat,
                          "MISCELLANEOUS_DIPLOMAT_BRIBE");
    break;
  case 2:
    menu_radio_set_active(radio_action_group_miscellaneous_diplomat,
                          "MISCELLANEOUS_DIPLOMAT_SABOTAGE_UNIT");
    break;
  case 3:
    menu_radio_set_active(radio_action_group_miscellaneous_diplomat,
                          "MISCELLANEOUS_DIPLOMAT_EMBASSY");
    break;
  case 4:
    menu_radio_set_active(radio_action_group_miscellaneous_diplomat,
                          "MISCELLANEOUS_DIPLOMAT_INVESTIGATE");
    break;
  case 5:
    menu_radio_set_active(radio_action_group_miscellaneous_diplomat,
                          "MISCELLANEOUS_DIPLOMAT_SABOTAGE_CITY");
    break;
  case 6:
    menu_radio_set_active(radio_action_group_miscellaneous_diplomat,
                          "MISCELLANEOUS_DIPLOMAT_STEAL");
    break;
  case 7:
    menu_radio_set_active(radio_action_group_miscellaneous_diplomat,
                          "MISCELLANEOUS_DIPLOMAT_INCITE");
    break;
  case 8:
    menu_radio_set_active(radio_action_group_miscellaneous_diplomat,
                          "MISCELLANEOUS_DIPLOMAT_POISON");
    break;
  case 9:
    menu_radio_set_active(radio_action_group_miscellaneous_diplomat,
                          "MISCELLANEOUS_DIPLOMAT_NOTHING");
    break;
  default:
    break;
  }

  menu_toggle_set_active(toggle_action_group_miscellaneous,
                         "MISCELLANEOUS_DIPLOMAT_IGNORE_ALLIES",
                         default_diplomat_ignore_allies);
  menu_toggle_set_active(toggle_action_group_miscellaneous,
                         "MISCELLANEOUS_UNIT_LOCK",
                         default_action_locked);
  menu_toggle_set_active(toggle_action_group_miscellaneous,
                         "MISCELLANEOUS_UNIT_MILITARY",
                         default_action_military_only);
  menu_toggle_set_active(toggle_action_group_miscellaneous,
                         "MISCELLANEOUS_TOGGLE_WAKEUP",
                         autowakeup_state);
  menu_toggle_set_active(toggle_action_group_miscellaneous,
                         "MISCELLANEOUS_TOGGLE_MOVEANDATTACK",
                         moveandattack_state);

  update_miscellaneous_menu();

  i = 0;
  automatic_processus_iterate(pap) {
    if(pap->menu[0]=='\0') {
      continue;
    }
    update_automatic_processus_filter_menu(pap, i++);
  } automatic_processus_iterate_end;

  start_turn_menus_udpate();
  update_menus();

}

/****************************************************************
  Update the menus about the airlift queue i.
*****************************************************************/
void update_airlift_menu(int i)
{
  if (!can_client_issue_orders() || !main_menubar) {
    return;
  }

  assert(i >= 0 && i < AIRLIFT_QUEUE_NUM);

  char buf[256], *m;
  bool cond = airlift_queue_size(0) > 0;

  if (i == 0) {
    menu_set_sensitive(action_group_airlift,
                       "AIRLIFT_CLEAR_AIRLIFT_QUEUE", cond);
    menu_set_sensitive(action_group_airlift,
                       "AIRLIFT_SHOW_CITIES_IN_AIRLIFT_QUEUE", cond);
    cond = cond && airlift_queue_get_unit_type(0) != U_LAST;
    menu_set_sensitive(action_group_airlift,
                       "AIRLIFT_SET_AIRLIFT_DEST", cond);
    menu_set_sensitive(action_group_delayed_goto,
                       "DELAYED_GOTO_DELAYED_AIRLIFT", cond);
    for (i = 1; i < AIRLIFT_QUEUE_NUM; i++) {
      update_airlift_menu(i);
    }
    return;
  } else {
    m = buf + my_snprintf(buf, sizeof(buf), "AIRLIFT_AQ%d",
                          i);
    if (airlift_queue_size(i) > 0) {
      menu_set_sensitive(action_group_airlift, buf, TRUE);
      strcpy(m, "_SELECT");
      menu_set_sensitive(action_group_airlift, buf, TRUE);
      strcpy(m, "_ADD");
      menu_set_sensitive(action_group_airlift, buf, TRUE);
      strcpy(m, "_RECORD");
      menu_set_sensitive(action_group_airlift, buf, cond);
      strcpy(m, "_CLEAR");
      menu_set_sensitive(action_group_airlift, buf, TRUE);
      strcpy(m, "_EXECUTE");
      menu_set_sensitive(action_group_airlift, buf,
                         airlift_queue_get_unit_type(i) != U_LAST);
      strcpy(m, "_SHOW");
      menu_set_sensitive(action_group_airlift, buf, TRUE);
      menu_set_sensitive(action_group_airlift,
                         "AIRLIFT_SET_AIRLIFT_DEST", TRUE);
      menu_set_sensitive(action_group_delayed_goto,
                         "DELAYED_GOTO_DELAYED_AIRLIFT", TRUE);
    } else if (cond) {
      menu_set_sensitive(action_group_airlift, buf, TRUE);
      strcpy(m, "_SELECT");
      menu_set_sensitive(action_group_airlift, buf, FALSE);
      strcpy(m, "_ADD");
      menu_set_sensitive(action_group_airlift, buf, FALSE);
      strcpy(m, "_RECORD");
      menu_set_sensitive(action_group_airlift, buf, TRUE);
      strcpy(m, "_CLEAR");
      menu_set_sensitive(action_group_airlift, buf, FALSE);
      strcpy(m, "_EXECUTE");
      menu_set_sensitive(action_group_airlift, buf, FALSE);
      strcpy(m, "_SHOW");
      menu_set_sensitive(action_group_airlift, buf, FALSE);
    } else {
      menu_set_sensitive(action_group_airlift, buf, FALSE);
    }
  }

}

/****************************************************************
  Update the menus about the multi-selection i.
*****************************************************************/
void update_multi_selection_menu(int i)
{

  if (!can_client_issue_orders() || !main_menubar) {
    return;
  }

  assert(i > 0 && i < MULTI_SELECT_NUM);

  char buf[256], *m;
  bool cond = multi_select_size(0) > 0;

  m = buf + my_snprintf(buf, sizeof(buf),
                        "MULTI_SELECTION_MS%d", i);
  if (multi_select_size(i) > 0) {
    menu_set_sensitive(action_group_multi_selection, buf, TRUE);
    strcpy(m, "_SELECT");
    menu_set_sensitive(action_group_multi_selection, buf, TRUE);
    strcpy(m, "_ADD");
    menu_set_sensitive(action_group_multi_selection, buf, TRUE);
    strcpy(m, "_RECORD");
    menu_set_sensitive(action_group_multi_selection, buf, cond);
    strcpy(m, "_CLEAR");
    menu_set_sensitive(action_group_multi_selection, buf, TRUE);
  } else if (cond) {
    menu_set_sensitive(action_group_multi_selection, buf, TRUE);
    strcpy(m, "_SELECT");
    menu_set_sensitive(action_group_multi_selection, buf, FALSE);
    strcpy(m, "_ADD");
    menu_set_sensitive(action_group_multi_selection, buf, FALSE);
    strcpy(m, "_RECORD");
    menu_set_sensitive(action_group_multi_selection, buf, TRUE);
    strcpy(m, "_CLEAR");
    menu_set_sensitive(action_group_multi_selection, buf, FALSE);
  } else {
    menu_set_sensitive(action_group_multi_selection, buf, FALSE);
  }
}

/****************************************************************
  Update the menus about the hilited cities.
*****************************************************************/
void update_miscellaneous_menu(void)
{
  if (!can_client_issue_orders() || !main_menubar) {
    return;
  }

  bool cond = (tiles_hilited_cities);

  menu_set_sensitive(action_group_miscellaneous,
                     "MISCELLANEOUS_SET_RALLIES", cond);
  menu_set_sensitive(action_group_miscellaneous,
                     "MISCELLANEOUS_CLEAR_RALLIES", cond);
  menu_set_sensitive(action_group_miscellaneous,
                     "MISCELLANEOUS_MY_AI_EXECUTE_ALL",
                     my_ai_enable && my_ai_count_activity(MY_AI_LAST));
  menu_set_sensitive(action_group_miscellaneous,
                     "MISCELLANEOUS_AIR_PATROL",
                     my_ai_enable && my_ai_count_activity(MY_AI_PATROL));

}

/****************************************************************
  ...
*****************************************************************/
static bool can_player_unit_type(Unit_Type_id utype)
{
  if (can_player_build_unit(get_player_ptr(),utype))
    return TRUE;

  unit_list_iterate(get_player_ptr()->units,punit) {
    if (punit->type==utype) {
      return TRUE;
    }
  } unit_list_iterate_end;

  return FALSE;
}

/****************************************************************
...
*****************************************************************/
void start_turn_menus_udpate(void)
{
  int i, j;

  /* Airlift */
  for(i = 0; i < U_LAST; i++) {
    if (strcmp(airlift_queue_get_menu_name(0, i), "\0") <= 0 ) {
      continue;
    }
    for(j = 0; j < AIRLIFT_QUEUE_NUM; j++) {
      bool sensitive = can_player_unit_type(i);
      const char *buf =
        airlift_queue_get_menu_name(j, i);
      menu_set_sensitive(radio_action_group_airlift_unit[j], buf, sensitive);
    }
  }

  /* Miscellaneous */
  switch(default_action_type) {
  case ACTION_IDLE:
    menu_radio_set_active(radio_action_group_miscellaneous_unit,
                          "MISCELLANEOUS_UNIT_IDLE");
    break;
  case ACTION_SENTRY:
    menu_radio_set_active(radio_action_group_miscellaneous_unit,
                          "MISCELLANEOUS_UNIT_SENTRY");
    break;
  case ACTION_FORTIFY:
    menu_radio_set_active(radio_action_group_miscellaneous_unit,
                          "MISCELLANEOUS_UNIT_FORTIFY");
    break;
  case ACTION_SLEEP:
    menu_radio_set_active(radio_action_group_miscellaneous_unit,
                          "MISCELLANEOUS_UNIT_SLEEP");
    break;
  case ACTION_FORTIFY_OR_SLEEP:
    menu_radio_set_active(radio_action_group_miscellaneous_unit,
                          "MISCELLANEOUS_UNIT_FORTIFY_OR_SLEEP");
    break;
  default:
    break;
  }

}

