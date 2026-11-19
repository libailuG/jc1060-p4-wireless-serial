#pragma once
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Display only: decimal notation, first six characters including sign/dot.
 * The editor and wire retain their original full-precision values. */
static inline void wireless_number_display(double value, char out[7]) {
    char candidate[384];
    snprintf(candidate,sizeof(candidate),"%.6f",value);
    char *dot=strchr(candidate,'.');
    if (dot) {
        char *end=candidate+strlen(candidate)-1;
        while (end>dot && *end=='0') *end--=0;
        if (end==dot) *end=0;
    }
    if (!strcmp(candidate,"-0")) strcpy(candidate,"0");
    snprintf(out,7,"%.6s",candidate);
    size_t len=strlen(out);
    if (len && out[len-1]=='.') out[len-1]=0;
}
