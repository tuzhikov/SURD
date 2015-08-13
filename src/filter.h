/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
******************************************************************************/

#ifndef __FILTER_H__
#define __FILTER_H__

#define N_DATA 10

typedef struct filter_
{
    short data[N_DATA];
    unsigned char pdata;
    unsigned char ndata;
    unsigned long S;  
    long S_average;  
    unsigned long S2_average;  
}filter;



void Init_filter (filter* f);
void Filter_add (filter *f, unsigned short d);
unsigned short Get_S2av_filter(filter* f);
unsigned short Get_Sav_filter(filter* f);
unsigned short Get_pred(filter* f, char i);
unsigned char GetFilter_N(filter *f);

void Filter_add_ex(filter *f, unsigned short d, unsigned long aver);

#endif