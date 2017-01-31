#ifndef DETONATOR_H_
#define DETONATOR_H_

#include "serie.h"
#include "widgets.h"
#include <stdint.h>

#define testDataSize 256

static char testData[testDataSize];
static uint8_t testDataInitialized = 0;
static uint8_t detonationEnd = 0;

void detonateInitArray()
{
	for (size_t i=0;i<testDataSize;i++)
	{
		testData[i] = i;
	}
}

static void button_clicked_int (GtkButton* button, gpointer func_data)
{
	detonationEnd = 1;
}

void portDetonate(void)
{
	GtkWidget *Dialogue, *btn;
	//TODO do it by pthread
	if (!testDataInitialized)
	{
		detonateInitArray();
	}
	Dialogue = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(Dialogue), "Detonator - click to end.");
	btn = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_signal_connect_object(GTK_OBJECT(btn), "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT(Dialogue));
	gtk_signal_connect_object(GTK_OBJECT(btn), "clicked", GTK_SIGNAL_FUNC(button_clicked_int), 0);
	gtk_signal_connect(GTK_OBJECT(Dialogue), "destroy", (GtkSignalFunc) gtk_widget_destroy, NULL);
	gtk_signal_connect(GTK_OBJECT(Dialogue), "delete_event", (GtkSignalFunc) gtk_widget_destroy, NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Dialogue)->action_area), btn, TRUE, TRUE, 0);
	gtk_window_set_modal(Dialogue, 0);
	//gtk_window_set_transient_for(Dialogue,*this);
	gtk_widget_show_all(Dialogue);

	detonationEnd = 0;
	while (!detonationEnd)
	{
		if (Send_chars(testData, testDataSize) != 0)
		{
			printf("Sended testData %s.\n", testData);
		}
		else
		{
			printf("Port not opened!\n");
		}
	}
}

#endif
