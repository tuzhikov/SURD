/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
******************************************************************************/


#include "math.h"
#include "filter.h"


void Init_filter (filter* f)
{
    f->ndata = 0;
    f->pdata = 0;
    for (unsigned char i = 0; i< N_DATA; i++)
        f->data[i] = 0;
    f->S = 0;
    f->S_average = 0;
    f->S2_average =0;
}


void Filter_add (filter *f, unsigned short d)
{
    unsigned short temp_d = f->data[f->pdata];
    f->data[f->pdata] = d;
    if (++f->pdata == N_DATA)
        f->pdata = 0;
    if (f->ndata < N_DATA)
        f->ndata++;
    
    
    f->S += d-temp_d;
/*    for (unsigned char i = 0; i< f->ndata; i++)
      f->S += f->data[i];
*/    
    f->S_average = f->S/f->ndata;
    
    f->S2_average = 0;
    for (unsigned char i = 0; i< f->ndata; i++)
      f->S2_average += (f->S_average - f->data[i]) * (f->S_average - f->data[i]);
    
    f->S2_average /= f->ndata;
    
    f->S2_average = (unsigned short)sqrt(f->S2_average);
}


unsigned short Get_S2av_filter(filter* f)
{
    return f->S2_average;
}

unsigned short Get_Sav_filter(filter* f)
{
    return f->S_average;   
}

unsigned short Get_pred(filter* f, char i)
{
    return f->data[(N_DATA+f->pdata-1-i)%N_DATA];
}


void Filter_add_ex(filter *f, unsigned short d, unsigned long aver)
{
    unsigned long max, min, temp;
    min = Get_Sav_filter(f)*N_DATA/aver;
    max = d*N_DATA/aver;
    if (min > max)
    {
        temp = max;
        max = min;
        min = temp;
    }
    
    min++;
    max++;
    
    max = (max-min+1)*N_DATA/max;
    
    if (!max)
        max++;
    else
        if (max > 2)
            max>>=1;
        
    // чем больше отклонение, тем больше напихаем значений в фильтр
    while (max)
    {
        max--;
        Filter_add (f, d);
    }
}

unsigned char GetFilter_N(filter *f)
{
    return f->ndata;   
}