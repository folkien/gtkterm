/***********************************************************************/
/* fichier.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Raw / text file transfer management                            */
/*                                                                     */
/*   ChangeLog                                                         */
/*   (All changes by Julien Schmitt except when explicitly written)    */
/*                                                                     */
/*      - 0.99.5 : changed all calls to strerror() by strerror_utf8()  */
/*      - 0.99.4 : added auto CR LF function by Sebastien              */
/*                 modified ecriture() to use send_serial()            */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.98.4 : modified to use new buffer                          */
/*      - 0.98 : file transfer completely rewritten / optimized        */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <glib.h>

#include "term_config.h"
#include "widgets.h"
#include "serie.h"
#include "buffer.h"

#include <config.h>
#include <glib/gi18n.h>

/* Global variables */
gint nb_car;
gint car_written;
gint current_buffer_position;
gint bytes_read;
GtkAdjustment *adj;
GtkWidget *ProgressBar;
gint Fichier;
guint callback_handler;
gchar *fic_defaut = NULL;
GtkWidget *Window;
gboolean waiting_for_char = FALSE;
gboolean waiting_for_timer = FALSE;
gboolean input_running = FALSE;
gchar *str = NULL;
FILE *Fic;

/* Local functions prototype */
gint Envoie_fichier(GtkFileChooser *FS);
gint Sauve_fichier(GtkFileChooser *FS);
gint close_all(void);
void ecriture(gpointer data, gint source, GdkInputCondition condition);
gboolean timer(gpointer pointer);
gboolean idle(gpointer pointer);
void remove_input(void);
void add_input(void);
void write_file(char *, unsigned int);

extern struct configuration_port config;

gint fichier(GtkWidget *widget, guint param)
{
    GtkWidget *file_select;
    gint retval;

    file_select = gtk_file_chooser_dialog_new(_("File selection"), GTK_WINDOW(Fenetre), 
					      GTK_FILE_CHOOSER_ACTION_SAVE, 
					      _("Cancel"), GTK_RESPONSE_CANCEL,
					      _("OK"), GTK_RESPONSE_OK, NULL);
    if(fic_defaut != NULL)
	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_select), fic_defaut);

    retval = gtk_dialog_run(GTK_DIALOG(file_select));
    if (retval == GTK_RESPONSE_OK)
    {
	switch(param)
	{
	    case 1:
		Envoie_fichier(GTK_FILE_CHOOSER(file_select));
		break;
	    case 2:
		Sauve_fichier(GTK_FILE_CHOOSER(file_select));
		break;
	}
    }

    gtk_widget_destroy(file_select);
    return FALSE;
}

gint Envoie_fichier(GtkFileChooser *FS)
{
    gchar *NomFichier;
    gchar *msg;
    GtkWidget *Bouton_annuler, *Box;

    NomFichier = gtk_file_chooser_get_filename(FS);

    if(!g_file_test(NomFichier, G_FILE_TEST_IS_REGULAR))
    {
	str = g_strdup_printf(_("Error opening file\n"));
	show_message(str, MSG_ERR);
	g_free(NomFichier);
	g_free(str);
	return FALSE;
    }
    Fichier = open(NomFichier, O_RDONLY);
    if(Fichier != -1)
    {
	fic_defaut = g_strdup(NomFichier);
	msg = g_strdup_printf(_("%s : transfer in progress..."), NomFichier);

	gtk_statusbar_push(GTK_STATUSBAR(StatusBar), id, msg);
	car_written = 0;
	current_buffer_position = 0;
	bytes_read = 0;
	nb_car = lseek(Fichier, 0L, SEEK_END);
	lseek(Fichier, 0L, SEEK_SET);

	Window = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(Window), msg);
	g_free(msg);
	Box = gtk_vbox_new(TRUE, 10);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(Window)->vbox), Box);
	ProgressBar = gtk_progress_bar_new();
	
	gtk_box_pack_start(GTK_BOX(Box), ProgressBar, FALSE, FALSE, 5);

	Bouton_annuler = gtk_button_new_with_label(_("Cancel"));
	gtk_signal_connect_object(GTK_OBJECT(Bouton_annuler), "clicked", GTK_SIGNAL_FUNC(close_all), NULL);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(Window)->action_area), Bouton_annuler);

	gtk_signal_connect_object(GTK_OBJECT(Window), "delete_event", GTK_SIGNAL_FUNC(close_all), NULL);

	gtk_window_set_default_size(GTK_WINDOW(Window), 250, 100);
	gtk_window_set_modal(GTK_WINDOW(Window), TRUE);
	gtk_widget_show_all(Window);

	add_input();
    }
    else
    {
	str = g_strdup_printf(_("Cannot read file %s: %s\n"), NomFichier, strerror(errno));
	show_message(str, MSG_ERR);
	g_free(str);
    }
    g_free(NomFichier);
    return FALSE;
}

void ecriture(gpointer data, gint source, GdkInputCondition condition)
{
    static gchar buffer[BUFFER_EMISSION];
    static gchar *current_buffer;
    static gint bytes_to_write;
    gint bytes_written;
    gchar *car;

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), (gfloat)car_written/(gfloat)nb_car );

    if(car_written < nb_car)
    {
	/* Read the file only if buffer totally sent or if buffer empty */
	if(current_buffer_position == bytes_read)
	{
	    bytes_read = read(Fichier, buffer, BUFFER_EMISSION);

	    current_buffer_position = 0;
	    current_buffer = buffer;
	    bytes_to_write = bytes_read;
	}

    if(current_buffer == NULL)
    {
        /* something went wrong... */
	    g_free(str);
	    str = g_strdup_printf(_("Error sending file\n"));
	    show_message(str, MSG_ERR);
	    close_all();
	    return;
    }

	car = current_buffer;

	if(config.delai != 0 || config.car != -1)
	{
	    /* search for next LF */
	    bytes_to_write = current_buffer_position;
	    while(*car != LINE_FEED && bytes_to_write < bytes_read)
	    {
		car++;
		bytes_to_write++;
	    }
	    if(*car == LINE_FEED)
		bytes_to_write++;
	}

	/* write to serial port */
	bytes_written = send_serial(current_buffer, bytes_to_write - current_buffer_position);

	if(bytes_written == -1)
	{
	    /* Problem while writing, stop file transfer */
	    g_free(str);
	    str = g_strdup_printf(_("Error sending file: %s\n"), strerror(errno));
	    show_message(str, MSG_ERR);
	    close_all();
	    return;
	}

	car_written += bytes_written;
	current_buffer_position += bytes_written;
	current_buffer += bytes_written;

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ProgressBar), (gfloat)car_written/(gfloat)nb_car );

	if(config.delai != 0 && *car == LINE_FEED)
	{
	    remove_input();
	    g_timeout_add(config.delai, (GSourceFunc)timer, NULL);
	    waiting_for_timer = TRUE;
	}
	else if(config.car != -1 && *car == LINE_FEED)
	{
	    remove_input();
	    waiting_for_char = TRUE;
	}
    }
    else
    {
	close_all();
	return;
    }
    return;
}

gboolean timer(gpointer pointer)
{
    if(waiting_for_timer == TRUE)
    {
	add_input();
	waiting_for_timer = FALSE;
    }
    return FALSE;
}

void add_input(void)
{
    if(input_running == FALSE)
    {
	input_running = TRUE;
	callback_handler = gtk_input_add_full(serial_port_fd, GDK_INPUT_WRITE, (GdkInputFunction)ecriture, NULL, NULL, NULL);
    }
}

void remove_input(void)
{
    if(input_running == TRUE)
    {
	gtk_input_remove(callback_handler);
	input_running = FALSE;
    }
}

gint close_all(void)
{
    remove_input();
    waiting_for_char = FALSE;
    waiting_for_timer = FALSE;
    gtk_statusbar_pop(GTK_STATUSBAR(StatusBar), id);
    close(Fichier);
    gtk_widget_destroy(Window);

    return FALSE;
}

void write_file(char *data, unsigned int size)
{
    fwrite(data, size, 1, Fic);
}

gint Sauve_fichier(GtkFileChooser *FS)
{
    gchar *NomFichier;

    NomFichier = gtk_file_chooser_get_filename(FS);
    if ((!NomFichier || (strcmp(NomFichier, ""))) == 0)
    {
	str = g_strdup_printf(_("File error\n"));
	show_message(str, MSG_ERR);
	g_free(str);
	g_free(NomFichier);
	return FALSE;
    }
    Fic = fopen(NomFichier, "w");
    if(Fic == NULL)
    {
	str = g_strdup_printf(_("Cannot open file %s: %s\n"), NomFichier, strerror(errno));
	show_message(str, MSG_ERR);
	g_free(str);
    }
    else
    {
	fic_defaut = g_strdup(NomFichier);

	write_buffer_with_func(write_file);

	fclose(Fic);
    }
    g_free(NomFichier);
    return FALSE;
}
