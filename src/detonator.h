#ifndef DETONATOR_H_
#define DETONATOR_H_

#include "serie.h"
#include "widgets.h"
#include <stdint.h>
#include <pthread.h>

#define DETONATOR_READABLE_ONLY 1
#define testPacketSize 4096

static uint8_t testData[testPacketSize];
static uint8_t testDataInitialized = 0;
static uint8_t detonationEnd = 0;
static uint32_t packetNumber= 0;

#if DETONATOR_READABLE_ONLY == 0
void detonateInitArray()
{
	for (size_t i=0;i<testPacketSize;i++)
	{
		testData[i] = i%256;
	}
    testDataInitialized = 1;
}
#else
void detonateInitArray()
{
	for (size_t i=0;i<testPacketSize;i++)
	{
		testData[i] = 48+(i%42);
	}
    testDataInitialized = 1;
}
#endif

static void disableDetonationTest (GtkButton* button, gpointer func_data)
{
	detonationEnd = 1;
}

static void * bombardSerialPort(void *data)
{
	detonationEnd = 0;
	while (!detonationEnd)
	{
		if (Send_chars(testData, testPacketSize) != 0)
		{
			printf("%d test packet sent.\n",packetNumber++);
		}
		else
		{
			printf("Port not opened!\n");
		}
        usleep(100);
	}
    return NULL;
}

void portDetonate(void)
{
	GtkWidget *Dialogue, *btn;
    pthread_t serialWriteThread;
    pthread_attr_t attr;

    packetNumber = 0;
	if (!testDataInitialized)
	{
		detonateInitArray();
	}

    pthread_attr_init(&attr);
    if(pthread_create(&serialWriteThread, &attr, &bombardSerialPort, NULL)) {
        printf("Error creating thread\n");
    }
	Dialogue = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(Dialogue), "Detonator - click to end.");
	btn = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_signal_connect_object(GTK_OBJECT(btn), "clicked", GTK_SIGNAL_FUNC(disableDetonationTest), 0);
	gtk_signal_connect_object(GTK_OBJECT(btn), "clicked", (GtkSignalFunc) gtk_widget_destroy, GTK_OBJECT(Dialogue));
	gtk_signal_connect(GTK_OBJECT(Dialogue), "destroy", (GtkSignalFunc) gtk_widget_destroy, NULL);
	gtk_signal_connect(GTK_OBJECT(Dialogue), "delete_event", (GtkSignalFunc) gtk_widget_destroy, NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(Dialogue)->action_area), btn, TRUE, TRUE, 0);
	gtk_window_set_modal(Dialogue, 0);
	//gtk_window_set_transient_for(Dialogue,*this);
	gtk_widget_show_all(Dialogue);
}

#endif
