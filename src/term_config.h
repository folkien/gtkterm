/***********************************************************************/
/* config.h                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Configuration of the serial port                               */
/*      - Header file -                                                */
/*                                                                     */
/***********************************************************************/

#ifndef TERM_CONFIG_H_
#define TERM_CONFIG_H_

gint Config_Port_Fenetre(GtkWidget *widget, guint param);
gint Lis_Config(GtkWidget *bouton, GtkWidget **Combos);
gint Config_Terminal(GtkWidget *widget, guint param);
void Set_Font(void);
gint config_window(gpointer *, guint);
void Verify_configuration(void);
gint Load_configuration_from_file(gchar *);
gint Check_configuration_file(void);
void check_text_input(GtkEditable *editable,
		       gchar       *new_text,
		       gint         new_text_length,
		       gint        *position,
		       gpointer     user_data);

struct configuration_port {
  gchar port[1024];
  gint vitesse;                // 300 - 600 - 1200 - ... - 115200
  gint bits;                   // 5 - 6 - 7 - 8
  gint stops;                  // 1 - 2
  gint parite;                 // 0 : None, 1 : Odd, 2 : Even
  gint flux;                   // 0 : None, 1 : Xon/Xoff, 2 : RTS/CTS, 3 : RS485halfduplex
  gint delai;                  // end of char delay: in ms
  gint rs485_rts_time_before_transmit;
  gint rs485_rts_time_after_transmit;
  gchar car;             // caractere � attendre
  gboolean echo;               // echo local
  gboolean crlfauto;         // line feed auto
};

typedef struct {
  gboolean transparency;
  gboolean show_cursor;
  gint rows;
  gint columns;
  gint scrollback;
  gboolean visual_bell;
  GdkColor foreground_color;
  GdkColor background_color;
  gdouble background_saturation;
  gchar *font;
} display_config_t;


#define DEFAULT_FONT "FiraCode, 12"
#define DEFAULT_SCROLLBACK 200

#define DEFAULT_PORT "/dev/ttyACM0"
#define DEFAULT_SPEED 9600
#define DEFAULT_PARITY 0
#define DEFAULT_BITS 8
#define DEFAULT_STOP 1
#define DEFAULT_FLOW 0
#define DEFAULT_DELAY 0
#define DEFAULT_CHAR -1
#define DEFAULT_DELAY_RS485 30
#define DEFAULT_ECHO FALSE

extern gchar *config_file;

#endif
