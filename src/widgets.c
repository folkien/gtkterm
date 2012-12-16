/***********************************************************************/
/* widgets.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Functions for the management of the GUI for the main window    */
/*                                                                     */
/*   ChangeLog                                                         */
/*   (All changes by Julien Schmitt except when explicitly written)    */
/*                                                                     */
/*      - 0.99.7 : Changed keyboard shortcuts to <ctrl><shift>         */ 
/*	            (Ken Peek)                                         */
/*      - 0.99.6 : Added scrollbar and copy/paste (Zach Davis)         */
/*                                                                     */
/*      - 0.99.5 : Make package buildable on pure *BSD by changing the */
/*                 include to asm/termios.h by sys/ttycom.h            */
/*                 Print message without converting it into the locale */
/*                 in show_message()                                   */
/*                 Set backspace key binding to backspace so that the  */
/*                 backspace works. It would even be nicer if the      */
/*                 behaviour of this key could be configured !         */
/*      - 0.99.4 : - Sebastien Bacher -                                */
/*                 Added functions for CR LF auto mode                 */
/*                 Fixed put_text() to have \r\n for the VTE Widget    */
/*                 Rewritten put_hexadecimal() function                */
/*                 - Julien -                                          */
/*                 Modified send_serial to return the actual number of */
/*                 bytes written, and also only display exactly what   */
/*                 is written                                          */
/*      - 0.99.3 : Modified to use a VTE terminal                      */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.0 : \b byte now handled correctly by the ascii widget   */
/*                 SUPPR (0x7F) also prints correctly                  */
/*                 adapted for macros                                  */
/*                 modified "about" dialog                             */
/*      - 0.98.6 : fixed possible buffer overrun in hex send           */
/*                 new "Send break" option                             */
/*      - 0.98.5 : icons in the menu                                   */
/*                 bug fixed with local echo and hexadecimal           */
/*                 modified hexadecimal send separator, and bug fixed  */
/*      - 0.98.4 : new hexadecimal display / send                      */
/*      - 0.98.3 : put_text() modified to fit with 0x0D 0x0A           */
/*      - 0.98.2 : added local echo by Julien                          */
/*      - 0.98 : file creation by Julien                               */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#if defined (__linux__)
#  include <asm/termios.h>       /* For control signals */
#endif
#if defined (__FreeBSD__) || defined (__FreeBSD_kernel__) \
     || defined (__NetBSD__) || defined (__NetBSD_kernel__) \
     || defined (__OpenBSD__) || defined (__OpenBSD_kernel__)
#  include <sys/ttycom.h>        /* For control signals */
#endif
#include <vte/vte.h>
#include <stdio.h>
#include <string.h>

#include "term_config.h"
#include "fichier.h"
#include "serie.h"
#include "widgets.h"
#include "buffer.h"
#include "macros.h"
#include "auto_config.h"
#include "logging.h"

#include <config.h>
#include <glib/gi18n.h>

guint id;
gboolean echo_on;
gboolean crlfauto_on;
GtkWidget *StatusBar;
GtkWidget *signals[6];
static GtkWidget *echo_menu = NULL;
static GtkWidget *crlfauto_menu = NULL;
static GtkWidget *ascii_menu = NULL;
static GtkWidget *hex_menu = NULL;
static GtkWidget *hex_len_menu = NULL;
static GtkWidget *hex_chars_menu = NULL;
static GtkWidget *show_index_menu = NULL;
static GtkWidget *Hex_Box;
static GtkWidget *log_pause_resume_menu = NULL;
static GtkWidget *log_start_menu = NULL;
static GtkWidget *log_stop_menu = NULL;
static GtkWidget *log_clear_menu = NULL;
GtkWidget *scrolled_window;
GtkWidget *Fenetre;
GtkAccelGroup *shortcuts;
GtkWidget *display = NULL;

GtkWidget *Text;
GtkTextBuffer *buffer;
GtkTextIter iter;

/* Variables for hexadecimal display */
static gint bytes_per_line = 16;
static gchar blank_data[128];
static guint total_bytes;
static gboolean show_index = FALSE;

/* Local functions prototype */
gint signaux(GtkWidget *, guint);
gint a_propos(GtkWidget *, guint);
gboolean Envoie_car(GtkWidget *, GdkEventKey *, gpointer);
gboolean control_signals_read(void);
gint Toggle_Echo(gpointer *, guint, GtkWidget *);
gint Toggle_Crlfauto(gpointer *, guint, GtkWidget *);
gint view(gpointer *, guint, GtkWidget *);
gint hexadecimal_chars_to_display(gpointer *, guint, GtkWidget *);
gint toggle_index(gpointer *, guint, GtkWidget *);
gint show_hide_hex(gpointer *, guint, GtkWidget *);
void initialize_hexadecimal_display(void);
gboolean Send_Hexadecimal(GtkWidget *, GdkEventKey *, gpointer);
gboolean pop_message(void);
static gchar *translate_menu(const gchar *, gpointer);
static void Got_Input(VteTerminal *, gchar *, guint, gpointer);
gint gui_paste(void);
gint gui_copy(void);
gint gui_copy_all_clipboard(void);


/* Menu */
#define NUMBER_OF_ITEMS 42

static GtkItemFactoryEntry Tableau_Menu[] = {
  {N_("/_File") , NULL, NULL, 0, "<Branch>"},
  {N_("/File/Clear screen") , "<ctrl><shift>L", (GtkItemFactoryCallback)clear_buffer, 0, "<StockItem>", GTK_STOCK_CLEAR},
  {N_("/File/Send _raw file") , "<ctrl><shift>R", (GtkItemFactoryCallback)fichier, 1, "<StockItem>",GTK_STOCK_JUMP_TO},
  {N_("/File/_Save raw file") , NULL, (GtkItemFactoryCallback)fichier, 2, "<StockItem>", GTK_STOCK_SAVE_AS},
  {N_("/File/Separator") , NULL, NULL, 0, "<Separator>"},
  {N_("/File/E_xit") , "<ctrl><shift>Q", gtk_main_quit, 0, "<StockItem>", GTK_STOCK_QUIT},
  {N_("/Edit/_Paste") , "<ctrl><shift>v", (GtkItemFactoryCallback)gui_paste, 0, "<StockItem>", GTK_STOCK_PASTE},
  {N_("/Edit/_Copy") , "<ctrl><shift>c", (GtkItemFactoryCallback)gui_copy, 0, "<StockItem>", GTK_STOCK_COPY},
  {N_("/Edit/Copy _All") , NULL, (GtkItemFactoryCallback)gui_copy_all_clipboard, 0, "<StockItem>", GTK_STOCK_SELECT_ALL},
  {N_("/_Log") , NULL, NULL, 0, "<Branch>"},
  {N_("/Log/To File...") , NULL, (GtkItemFactoryCallback)logging_start, 0, "<StockItem>", GTK_STOCK_MEDIA_RECORD},
  {N_("/Log/Pause") , NULL, (GtkItemFactoryCallback)logging_pause_resume, 0, "<StockItem>", GTK_STOCK_MEDIA_PAUSE},
  {N_("/Log/Stop") , NULL, (GtkItemFactoryCallback)logging_stop, 0, "<StockItem>", GTK_STOCK_MEDIA_STOP},
  {N_("/Log/Clear") , NULL, (GtkItemFactoryCallback)logging_clear, 0, "<StockItem>", GTK_STOCK_CLEAR},
  {N_("/_Configuration"), NULL, NULL, 0, "<Branch>"},
  {N_("/Configuration/_Port"), "<ctrl><shift>S", (GtkItemFactoryCallback)Config_Port_Fenetre, 0, "<StockItem>", GTK_STOCK_PREFERENCES},
  {N_("/Configuration/_Main window"), NULL, (GtkItemFactoryCallback)Config_Terminal, 0, "<StockItem>", GTK_STOCK_SELECT_FONT},
  {N_("/Configuration/Local _echo"), NULL, (GtkItemFactoryCallback)Toggle_Echo, 0, "<CheckItem>"},
  {N_("/Configuration/_CR LF auto"), NULL, (GtkItemFactoryCallback)Toggle_Crlfauto, 0, "<CheckItem>"},
  {N_("/Configuration/_Macros"), NULL, (GtkItemFactoryCallback)Config_macros, 0, "<Item>"},
  {N_("/Configuration/Separator") , NULL, NULL, 0, "<Separator>"},
  {N_("/Configuration/_Load configuration"), NULL, (GtkItemFactoryCallback)config_window, 0, "<StockItem>", GTK_STOCK_OPEN},
  {N_("/Configuration/_Save configuration"), NULL, (GtkItemFactoryCallback)config_window, 1, "<StockItem>", GTK_STOCK_SAVE_AS},
  {N_("/Configuration/_Delete configuration"), NULL, (GtkItemFactoryCallback)config_window, 2, "<StockItem>", GTK_STOCK_DELETE},
  {N_("/Control _signals"), NULL, NULL, 0, "<Branch>"},
  {N_("/Control signals/Send break"), "<ctrl>B", (GtkItemFactoryCallback)signaux, 2, "<Item>"},
  {N_("/Control signals/Toggle DTR"), "F7", (GtkItemFactoryCallback)signaux, 0, "<Item>"},
  {N_("/Control signals/Toggle RTS"), "F8", (GtkItemFactoryCallback)signaux, 1, "<Item>"},
  {N_("/_View"), NULL, NULL, 0, "<Branch>"},
  {N_("/View/_ASCII"), NULL, (GtkItemFactoryCallback)view, ASCII_VIEW, "<RadioItem>"},
  {N_("/View/_Hexadecimal"), NULL, (GtkItemFactoryCallback)view, HEXADECIMAL_VIEW, "<RadioItem>"},
  {N_("/View/Hexadecimal _chars"), NULL, NULL, 0, "<Branch>"},
  {N_("/View/Hexadecimal chars/_8"), NULL, (GtkItemFactoryCallback)hexadecimal_chars_to_display, 8, "<RadioItem>"},
  {N_("/View/Hexadecimal chars/1_0"), NULL, (GtkItemFactoryCallback)hexadecimal_chars_to_display, 10, "/View/Hexadecimal chars/8"},
  {N_("/View/Hexadecimal chars/_16"), NULL, (GtkItemFactoryCallback)hexadecimal_chars_to_display, 16, "/View/Hexadecimal chars/8"},
  {N_("/View/Hexadecimal chars/_24"), NULL, (GtkItemFactoryCallback)hexadecimal_chars_to_display, 24, "/View/Hexadecimal chars/8"},
  {N_("/View/Hexadecimal chars/_32"), NULL, (GtkItemFactoryCallback)hexadecimal_chars_to_display, 32, "/View/Hexadecimal chars/8"},
  {N_("/View/Show _index"), NULL, (GtkItemFactoryCallback)toggle_index, 0, "<CheckItem>"},
  {N_("/View/Separator") , NULL, NULL, 0, "<Separator>"},
  {N_("/View/_Send hexadecimal data") , NULL, (GtkItemFactoryCallback)show_hide_hex, 0, "<CheckItem>"},
  {N_("/_Help"), NULL, NULL, 0, "<LastBranch>"},
  {N_("/Help/_About..."), NULL, (GtkItemFactoryCallback)a_propos, 0, "<StockItem>", GTK_STOCK_DIALOG_INFO}
};

static gchar *translate_menu(const gchar *path, gpointer data)
{
  return _(path);
}

gint show_hide_hex(gpointer *pointer, guint param, GtkWidget *widget)
{
  if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
    gtk_widget_show(GTK_WIDGET(Hex_Box));
  else
    gtk_widget_hide(GTK_WIDGET(Hex_Box));

  return FALSE;
}

gint toggle_index(gpointer *pointer, guint param, GtkWidget *widget)
{
  show_index = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
  set_view(HEXADECIMAL_VIEW);
  return FALSE;
}

gint hexadecimal_chars_to_display(gpointer *pointer, guint param, GtkWidget *widget)
{
  bytes_per_line = param;
  set_view(HEXADECIMAL_VIEW);
  return FALSE;
}

void set_view(guint type)
{
  clear_display();
  set_clear_func(clear_display);
  switch(type)
    {
    case ASCII_VIEW:
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(hex_menu), FALSE);
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ascii_menu), TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(show_index_menu), FALSE);
      gtk_widget_set_sensitive(GTK_WIDGET(hex_chars_menu), FALSE);
      total_bytes = 0;
      set_display_func(put_text);
      break;
    case HEXADECIMAL_VIEW:
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(hex_menu), TRUE);
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ascii_menu), FALSE);
      gtk_widget_set_sensitive(GTK_WIDGET(show_index_menu), TRUE);
      gtk_widget_set_sensitive(GTK_WIDGET(hex_chars_menu), TRUE);
      total_bytes = 0;
      set_display_func(put_hexadecimal);
      break;
    default:
      set_display_func(NULL);
    }
  write_buffer();
}

gint view(gpointer *pointer, guint param, GtkWidget *widget)
{
  if(!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
    return FALSE;

  set_view(param);

  return FALSE;
}

void Set_local_echo(gboolean echo)
{
  echo_on = echo;
  if(echo_menu)
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(echo_menu), echo_on);
}

gint Toggle_Echo(gpointer *pointer, guint param, GtkWidget *widget)
{
  echo_on = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
  configure_echo(echo_on);
  return 0;
}

void Set_crlfauto(gboolean crlfauto)
{
  crlfauto_on = crlfauto;
  if(crlfauto_menu)
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(crlfauto_menu), crlfauto_on);
}

gint Toggle_Crlfauto(gpointer *pointer, guint param, GtkWidget *widget)
{
  crlfauto_on = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
  configure_crlfauto(crlfauto_on);
  return 0;
}

void toggle_logging_pause_resume(gboolean currentlyLogging)
{
    if (currentlyLogging)
    {
        gtk_menu_item_set_label(GTK_MENU_ITEM(log_pause_resume_menu), _("Pause"));
    }
    else
    {
        gtk_menu_item_set_label(GTK_MENU_ITEM(log_pause_resume_menu), _("Resume"));
    }
}

void toggle_logging_sensitivity(gboolean currentlyLogging)
{
   gtk_widget_set_sensitive(GTK_WIDGET(log_start_menu), !currentlyLogging);
   gtk_widget_set_sensitive(GTK_WIDGET(log_stop_menu), currentlyLogging);
   gtk_widget_set_sensitive(GTK_WIDGET(log_pause_resume_menu), currentlyLogging);
   gtk_widget_set_sensitive(GTK_WIDGET(log_clear_menu), currentlyLogging);
}

void create_main_window(void)
{
  GtkWidget *Menu, *Boite, *BoiteH, *Label;
  GtkWidget *Hex_Send_Entry;
  GtkItemFactory *item_factory;
  GtkAccelGroup *accel_group;
  GSList *group;

  Fenetre = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  shortcuts = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(Fenetre), GTK_ACCEL_GROUP(shortcuts));

  gtk_signal_connect(GTK_OBJECT(Fenetre), "destroy", (GtkSignalFunc)gtk_main_quit, NULL);
  gtk_signal_connect(GTK_OBJECT(Fenetre), "delete_event", (GtkSignalFunc)gtk_main_quit, NULL);
  
  Set_window_title("GtkTerm");

  Boite = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(Fenetre), Boite);

  accel_group = gtk_accel_group_new();
  item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accel_group);
  gtk_item_factory_set_translate_func(item_factory, translate_menu, "<main>", NULL);
  gtk_window_add_accel_group(GTK_WINDOW(Fenetre), accel_group);
  gtk_item_factory_create_items(item_factory, NUMBER_OF_ITEMS, Tableau_Menu, NULL);
  Menu = gtk_item_factory_get_widget(item_factory, "<main>");
  log_pause_resume_menu = gtk_item_factory_get_item(item_factory, "/Log/Pause");
  log_start_menu = gtk_item_factory_get_item(item_factory, "/Log/To File...");
  log_stop_menu = gtk_item_factory_get_item(item_factory, "/Log/Stop");
  log_clear_menu = gtk_item_factory_get_item(item_factory, "/Log/Clear");
  echo_menu = gtk_item_factory_get_item(item_factory, "/Configuration/Local echo");
  crlfauto_menu = gtk_item_factory_get_item(item_factory, "/Configuration/LF auto");
  ascii_menu = gtk_item_factory_get_item(item_factory, "/View/ASCII");
  hex_menu = gtk_item_factory_get_item(item_factory, "/View/Hexadecimal");
  hex_chars_menu = gtk_item_factory_get_item(item_factory, "/View/Hexadecimal chars");
  show_index_menu = gtk_item_factory_get_item(item_factory, "/View/Show index");
  group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(ascii_menu));
  gtk_radio_menu_item_set_group(GTK_RADIO_MENU_ITEM(hex_menu), group);

  hex_len_menu = gtk_item_factory_get_item(item_factory, "/View/Hexadecimal chars/16");
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(hex_len_menu), TRUE);

  gtk_box_pack_start(GTK_BOX(Boite), Menu, FALSE, TRUE, 0);

  BoiteH = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(Boite), BoiteH, TRUE, TRUE, 0);

  /* create vte window */
  display = vte_terminal_new();

  /* set terminal properties, these could probably be made user configurable */
  vte_terminal_set_scroll_on_output(VTE_TERMINAL(display), FALSE);
  vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(display), TRUE);
  vte_terminal_set_mouse_autohide(VTE_TERMINAL(display), TRUE);
  vte_terminal_set_backspace_binding(VTE_TERMINAL(display),
                                     VTE_ERASE_ASCII_BACKSPACE);

  clear_display();

  /* make vte window scrollable - inspired by gnome-terminal package */
  scrolled_window = gtk_scrolled_window_new(NULL,
					    vte_terminal_get_adjustment(VTE_TERMINAL(display)));
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				 GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window),
				      GTK_SHADOW_NONE);
  gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(display));

  gtk_box_pack_start_defaults(GTK_BOX(BoiteH), scrolled_window);

  /* set up logging buttons availability */
  toggle_logging_pause_resume(FALSE);
  toggle_logging_sensitivity(FALSE);

  /* hex box is hidden when not in use */
  Hex_Box = gtk_hbox_new(TRUE, 0);
  Label = gtk_label_new(_("Hexadecimal data to send (separator : ';' or space) : "));
  gtk_box_pack_start_defaults(GTK_BOX(Hex_Box), Label);
  Hex_Send_Entry = gtk_entry_new();
  gtk_signal_connect(GTK_OBJECT(Hex_Send_Entry), "activate", (GtkSignalFunc)Send_Hexadecimal, NULL);
  gtk_box_pack_start(GTK_BOX(Hex_Box), Hex_Send_Entry, FALSE, TRUE, 5);
  gtk_box_pack_start(GTK_BOX(Boite), Hex_Box, FALSE, TRUE, 2);

  /* status bar */
  StatusBar = gtk_statusbar_new();
  gtk_box_pack_start(GTK_BOX(Boite), StatusBar, FALSE, FALSE, 0);
  id = gtk_statusbar_get_context_id(GTK_STATUSBAR(StatusBar), "Messages");

  Label = gtk_label_new("RI");
  gtk_box_pack_end(GTK_BOX(StatusBar), Label, FALSE, TRUE, 5);
  gtk_widget_set_sensitive(GTK_WIDGET(Label), FALSE);
  signals[0] = Label;

  Label = gtk_label_new("DSR");
  gtk_box_pack_end(GTK_BOX(StatusBar), Label, FALSE, TRUE, 5);
  signals[1] = Label;

  Label = gtk_label_new("CD");
  gtk_box_pack_end(GTK_BOX(StatusBar), Label, FALSE, TRUE, 5);
  signals[2] = Label;

  Label = gtk_label_new("CTS");
  gtk_box_pack_end(GTK_BOX(StatusBar), Label, FALSE, TRUE, 5);
  signals[3] = Label;

  Label = gtk_label_new("RTS");
  gtk_box_pack_end(GTK_BOX(StatusBar), Label, FALSE, TRUE, 5);
  signals[4] = Label;

  Label = gtk_label_new("DTR");
  gtk_box_pack_end(GTK_BOX(StatusBar), Label, FALSE, TRUE, 5);
  signals[5] = Label;

  g_signal_connect_after(GTK_OBJECT(display), "commit", G_CALLBACK(Got_Input), NULL);

  gtk_timeout_add(POLL_DELAY, (GtkFunction)control_signals_read, NULL);

  gtk_window_set_default_size(GTK_WINDOW(Fenetre), 750, 550);
  gtk_widget_show_all(Fenetre);
  gtk_widget_hide(GTK_WIDGET(Hex_Box));

}

void initialize_hexadecimal_display(void)
{
  total_bytes = 0;
  memset(blank_data, ' ', 128);
  blank_data[bytes_per_line * 3 + 5] = 0;
}

void put_hexadecimal(gchar *string, guint size)
{
  static gchar data[128];
  static gchar data_byte[6];
  static guint bytes;
  glong column, row;

  gint i = 0;

  if(size == 0)
    return;

  while(i < size)
    {
      while(gtk_events_pending()) gtk_main_iteration();
      vte_terminal_get_cursor_position(VTE_TERMINAL(display), &column, &row);

      if(show_index)
	{
	  if(column == 0)
	    /* First byte on line */
	    {
	      sprintf(data, "%6d: ", total_bytes);
	      vte_terminal_feed(VTE_TERMINAL(display), data, strlen(data));
	      bytes = 0;
	    }
	}
      else
	{
	  if(column == 0)
	    bytes = 0;
	}

      /* Print hexadecimal characters */
      data[0] = 0;

      while(bytes < bytes_per_line && i < size)
	{
	  gint avance=0;
	  gchar ascii[1];

	  sprintf(data_byte, "%02X ", (guchar)string[i]);
	  log_chars(data_byte, 3);
	  vte_terminal_feed(VTE_TERMINAL(display), data_byte, 3);

	  avance = (bytes_per_line - bytes) * 3 + bytes + 2;

	  /* Move forward */
	  sprintf(data_byte, "%c[%dC", 27, avance);
	  vte_terminal_feed(VTE_TERMINAL(display), data_byte, strlen(data_byte));

	  /* Print ascii characters */
	  ascii[0] = (string[i] > 0x1F) ? string[i] : '.';
	  vte_terminal_feed(VTE_TERMINAL(display), ascii, 1);

	  /* Move backward */
	  sprintf(data_byte, "%c[%dD", 27, avance + 1);
	  vte_terminal_feed(VTE_TERMINAL(display), data_byte, strlen(data_byte));

	  if(bytes == bytes_per_line / 2 - 1)
	    vte_terminal_feed(VTE_TERMINAL(display), "- ", strlen("- "));

	  bytes++;
	  i++;

	  /* End of line ? */
	  if(bytes == bytes_per_line)
	    {
	      vte_terminal_feed(VTE_TERMINAL(display), "\r\n", 2);
	      total_bytes += bytes;
	    }

	}

    }
}

void put_text(gchar *string, guint size)
{
    log_chars(string, size);
    vte_terminal_feed(VTE_TERMINAL(display), string, size);
}

gint send_serial(gchar *string, gint len)
{
  gint bytes_written;

  bytes_written = Send_chars(string, len);
  if(bytes_written > 0)
    {
      if(echo_on)
	  put_chars(string, bytes_written, crlfauto_on);
    }

  return bytes_written;
}


static void Got_Input(VteTerminal *widget, gchar *text, guint length, gpointer ptr)
{
  send_serial(text, length);
}

gboolean Envoie_car(GtkWidget *widget, GdkEventKey *event, gpointer pointer)
{
  if(g_utf8_validate(event->string, 1, NULL))
    send_serial(event->string, 1);

  return FALSE;
}


gint a_propos(GtkWidget *widget, guint param)
{

  gchar *authors[] = {"Julien Schmitt", "Zach Davis"};
  gtk_show_about_dialog(NULL,
                        "program-name", "GtkTerm",
                        "title", _("About GtkTerm"),
                        "authors", authors,
                        "version", VERSION,
                        "license", "GPL V.2",
                        "website", "https://fedorahosted.org/gtkterm/",
                        "website-label", "GtkTerm Homepage", NULL);

  return FALSE;
}

void show_control_signals(int stat)
{
  if(stat & TIOCM_RI)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[0]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[0]), FALSE);
  if(stat & TIOCM_DSR)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[1]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[1]), FALSE);
  if(stat & TIOCM_CD)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[2]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[2]), FALSE);
  if(stat & TIOCM_CTS)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[3]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[3]), FALSE);
  if(stat & TIOCM_RTS)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[4]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[4]), FALSE);
  if(stat & TIOCM_DTR)
    gtk_widget_set_sensitive(GTK_WIDGET(signals[5]), TRUE);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(signals[5]), FALSE);
}

gint signaux(GtkWidget *widget, guint param)
{
  if(param == 2)
    {
      sendbreak();
      Put_temp_message(_("Break signal sent!"), 800);
    }
  else
    Set_signals(param);
  return FALSE;
}

gboolean control_signals_read(void)
{
  int state;

  state = lis_sig();
  if(state >= 0)
    show_control_signals(state);

  return TRUE;
}

void Set_status_message(gchar *msg)
{
  gtk_statusbar_pop(GTK_STATUSBAR(StatusBar), id);
  gtk_statusbar_push(GTK_STATUSBAR(StatusBar), id, msg);
}

void Set_window_title(gchar *msg)
{
    gchar* header = g_strdup_printf("GtkTerm - %s", msg);
    gtk_window_set_title(GTK_WINDOW(Fenetre), header);
    g_free(header);
}

void show_message(gchar *message, gint type_msg)
{
 GtkWidget *Fenetre_msg;

 if(type_msg==MSG_ERR)
   {
     Fenetre_msg = gtk_message_dialog_new(GTK_WINDOW(Fenetre), 
					  GTK_DIALOG_DESTROY_WITH_PARENT, 
					  GTK_MESSAGE_ERROR, 
					  GTK_BUTTONS_OK, 
					  message, NULL);
   }
 else if(type_msg==MSG_WRN)
   {
     Fenetre_msg = gtk_message_dialog_new(GTK_WINDOW(Fenetre), 
					  GTK_DIALOG_DESTROY_WITH_PARENT, 
					  GTK_MESSAGE_WARNING, 
					  GTK_BUTTONS_OK, 
					  message, NULL);
   }
 else
   return;

 gtk_dialog_run(GTK_DIALOG(Fenetre_msg));
 gtk_widget_destroy(Fenetre_msg);
}

gboolean Send_Hexadecimal(GtkWidget *widget, GdkEventKey *event, gpointer pointer)
{
    guint i;
    gchar *text, *message, **tokens;
    guint scan_val;
    guint sent = 0;

    text = (gchar *)gtk_entry_get_text(GTK_ENTRY(widget));

    if(strlen(text) == 0){
        message = g_strdup_printf(_("0 byte(s) sent!"));
        Put_temp_message(message, 1500);
        gtk_entry_set_text(GTK_ENTRY(widget), "");
        g_free(message);
        return FALSE;
    }

    tokens = g_strsplit_set(text, " ;", -1);

    for(i = 0; tokens[i] != NULL; i++){
        if(sscanf(tokens[i], "%02X", &scan_val) == 1){
            send_serial((gchar*)&scan_val, 1);
            sent++;
        }
    }

    message = g_strdup_printf(_("%d byte(s) sent!"), sent);
    Put_temp_message(message, 2000);
    gtk_entry_set_text(GTK_ENTRY(widget), "");
    g_strfreev(tokens);

    return FALSE;
}

void Put_temp_message(const gchar *text, gint time)
{
  /* time in ms */
  gtk_statusbar_push(GTK_STATUSBAR(StatusBar), id, text);
  gtk_timeout_add(time, (GtkFunction)pop_message, NULL);
}

gboolean pop_message(void)
{
  gtk_statusbar_pop(GTK_STATUSBAR(StatusBar), id);

  return FALSE;
}

void clear_display(void)
{
  initialize_hexadecimal_display();
  if(display)
    vte_terminal_reset(VTE_TERMINAL(display), TRUE, TRUE);
}

gint gui_paste(void)
{
    vte_terminal_paste_clipboard(VTE_TERMINAL(display));
    return 0;
}

gint gui_copy(void)
{
    vte_terminal_copy_clipboard(VTE_TERMINAL(display));
    return 0;
}

gint gui_copy_all_clipboard(void)
{
    vte_terminal_select_all(VTE_TERMINAL(display));
    gui_copy();
    vte_terminal_select_none(VTE_TERMINAL(display));

    return 0;
}
