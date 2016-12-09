#ifndef DETONATOR_H_
#define DETONATOR_H_

#include "serie.h"

void portDetonate(void) {
    char testData[] = "<1234567890ABCDEF>";
    if ( Send_chars(testData, sizeof(testData)) != 0) {
        printf("Sended testData %s.\n",testData);
    }
    else {
        printf("Port not opened!\n");
    }
}

#endif
