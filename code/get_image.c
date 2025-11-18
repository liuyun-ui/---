/*
 * get_image.c
 *
 *  Created on: 2025��10��7��
 *      Author: 15286
 */
#include "zf_common_headfile.h"
#include "get_image.h"
#include <math.h>

int threshold=0;
uint8 image[MT9V03X_H][MT9V03X_W];
int left_bound[MT9V03X_H]={0};
int right_bound[MT9V03X_H]={0};
int midline[MT9V03X_H]={0};
uint8 left_lost[MT9V03X_H];
uint8 right_lost[MT9V03X_H];
int white_start=0;
int longest_column=MT9V03X_W/2;
uint8 cross_flag = 0;
int left_down_find = 0;
int right_down_find = 0;
int left_up_find = 0;
int right_up_find = 0;
int both_lost_time = 0;

int straight_flag=0;
int cur_flag=0;
float near_slope=0;

int prospective_row_start = 0;
int prospective_row_end = 0;
int prospective_row_count = 20;
int target_prospective_row_count=20;
float prospective_weights[70]={0};
float pros_weighted_deviation = 0;
float max_err=40;
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 90
//加权控制

#define STRAIGHT_BOTTOM_OFFSET 5
#define STRAIGHT_BOTTOM_RANGE 45
#define STRAIGHT_SLOPE_THRESHOLD 0.25f

const uint8 Weight[70]=
{
        11, 11, 11, 11, 11, 11, 11, 11, 11, 11,              //图像最远端00 ——09 行权重
        11, 11, 11, 11, 11, 11, 11, 11, 11, 11,              //图像最远端10 ——19 行权重
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,              //图像最远端20 ——29 行权重
        7, 9, 11,11,13,15,17,19,20,20,              //图像最远端30 ——39 行权重
       19,17,15,13,11, 9, 7, 5, 3, 1,              //图像最远端40 ——49 行权重
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,              //图像最远端50 ——59 行权重
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1,              //图像最远端60 ——69 行权重
};

void my_camera_init(void)
{
    my_tft_init();
    tft180_clear();
    mt9v03x_init();
    load_from_flash();
    mt9v03x_set_exposure_time(exposure_time);
    memset(left_lost, 1, sizeof(left_lost));
    memset(right_lost, 1, sizeof(right_lost));
    init_prospective_weights();
}

void init_prospective_weights(void)
{
    float total_weight = 0;

    for(int i = 0; i < prospective_row_count; i++)
    {
        total_weight += Weight[i];
    }
    for(int i = 0; i < prospective_row_count; i++)
    {
        prospective_weights[i] = Weight[i]/total_weight;
    }
}

void camera_display(void)
{
    if(mt9v03x_finish_flag)
    {
        image_binary();
        if(!system_in_setting_mode)
        {
            tft180_show_gray_image(0, 0, (uint8 *)image, MT9V03X_W, MT9V03X_H, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0);
            show_boundaries();
        }
        search_line();
        //cross_detect();
        straight_detect();
        cal_error();
        calculate_pros_weighted_deviation();

        //show_boundaries();
        mt9v03x_finish_flag = 0;
    }
}

int My_Adapt_Threshold(uint8 image[][MT9V03X_W], uint16 width, uint16 height)
{
    #define GrayScale 256
    int pixelCount[GrayScale] = {0};
    float pixelPro[GrayScale] = {0};
    int i, j;
    int pixelSum = width * height;  // 修改：使用全部像素
    int threshold = 0;

    uint32 gray_sum = 0;

    // 统计直方图 - 全采样：遍历所有像素
    for (i = 0; i < height; i++) {  // 修改：步长改为1
        for (j = 0; j < width; j++) {  // 修改：步长改为1
            pixelCount[(int)image[i][j]]++;
            gray_sum += (int)image[i][j];
        }
    }
    for (i = 0; i < GrayScale; i++) {
        pixelPro[i] = (float)pixelCount[i] / pixelSum;
    }

    float w0, w1, u0tmp, u1tmp, u0, u1, u, deltaTmp, deltaMax = 0;
    w0 = w1 = u0tmp = u1tmp = u0 = u1 = u = deltaTmp = 0;

    // 全局平均灰度
    u = gray_sum / (float)pixelSum;

    for (j = 0; j < GrayScale; j++) {
        w0 += pixelPro[j];          // 背景比例
        u0tmp += j * pixelPro[j];   // 背景灰度加权和

        if (w0 < 1e-6 || w0 > 1 - 1e-6) {
            continue;  // 避免除零
        }

        w1 = 1 - w0;                // 前景比例
        u1tmp = u - u0tmp;          // 前景灰度加权和

        u0 = u0tmp / w0;            // 背景平均灰度
        u1 = u1tmp / w1;            // 前景平均灰度

        // 类间方差
        deltaTmp = w0 * w1 * (u0 - u1) * (u0 - u1);

        if (deltaTmp > deltaMax) {
            deltaMax = deltaTmp;
            threshold = j;
        }
    }

    // 边界检查
    if (threshold > 255) threshold = 255;
    if (threshold < 0) threshold = 0;

    return threshold;
}

 // 改进版：基于赛道中心区域的自适应阈值
 int My_Road_Adapt_Threshold(uint8 image[][MT9V03X_W], uint16 width, uint16 height)
 {
       #define GrayScale 256
      int pixelCount[GrayScale] = {0};
     float pixelPro[GrayScale] = {0};
      int i, j;
      
      // 只统计图像中间和下半部分的像素（赛道可能存在的区域）
      int start_row = height / 3;        // 从1/3处开始
     int start_col = width / 4;         // 左侧1/4
      int end_col = width * 3 / 4;       // 右侧3/4
      
      int pixelSum = 0;
      uint32 gray_sum = 0;
      
      // 统计直方图 - 只在可能有赛道的区域采样
      for (i = start_row; i < height; i++) {
          for (j = start_col; j < end_col; j++) {
              pixelCount[(int)image[i][j]]++;
              gray_sum += (int)image[i][j];
              pixelSum++;
          }
      }
      
     // 计算概率分布
      for (i = 0; i < GrayScale; i++) {
          pixelPro[i] = (float)pixelCount[i] / pixelSum;
     }
      
      float w0, w1, u0tmp, u1tmp, u0, u1, u, deltaTmp, deltaMax = 0;
      w0 = w1 = u0tmp = u1tmp = u0 = u1 = u = deltaTmp = 0;
      int threshold = 0;
      // 全局平均灰度
      u = gray_sum / (float)pixelSum;
      // 大津法计算最佳阈值
      for (j = 0; j < GrayScale; j++) {
          w0 += pixelPro[j];
          u0tmp += j * pixelPro[j];
          if (w0 < 1e-6 || w0 > 1 - 1e-6) {
              continue;
          }
          
          w1 = 1 - w0;
         u1tmp = u - u0tmp;
          
          u0 = u0tmp / w0;
          u1 = u1tmp / w1;
          
          deltaTmp = w0 * w1 * (u0 - u1) * (u0 - u1);
          
          if (deltaTmp > deltaMax) {
              deltaMax = deltaTmp;
              threshold = j;
          }
      }
      
      // 边界检查
      if (threshold > 255) threshold = 255;
      if (threshold < 0) threshold = 0;
      
      return threshold;
  }
  
void image_binary(void)
{
    threshold=My_Adapt_Threshold(mt9v03x_image,MT9V03X_W, MT9V03X_H);

    for(int i=0;i<MT9V03X_H;i++)
    {
        for(int j=0;j<MT9V03X_W;j++)
        {
            if(mt9v03x_image[i][j]>threshold)
            {
                image[i][j]=255;
            }
            else
            {
                image[i][j]=0;
            }
        }
    }
}

void search_line(void)
{
    int max_white_count = 0;
    longest_column = MT9V03X_W / 2;

    memset(left_lost, 1, sizeof(left_lost));
    memset(right_lost, 1, sizeof(right_lost));

    prospective_row_count=target_prospective_row_count;

    for(int j = 0; j < MT9V03X_W; j++)
    {
        int current_white_count = 0;
        for(int i = 0; i < MT9V03X_H; i++)
        {
            if(image[i][j] == 255)
            {
                current_white_count++;
            }
        }
        if(current_white_count > max_white_count)
        {
            max_white_count = current_white_count;
            longest_column = j;
        }
    }
    white_start=MT9V03X_H-5;
    for(int i = MT9V03X_H-5; i >0; i--)
    {
        if(image[i][longest_column] == 255)
        {
            white_start= i;
        }
        else
        {
            break;
        }
    }

    int target_prospective_row_start=MT9V03X_H-10-target_prospective_row_count;
    if(target_prospective_row_start<white_start)
    {
        prospective_row_start=white_start;
    }
    else
    {
        prospective_row_start=target_prospective_row_start;
    }
    if(MT9V03X_H-1<white_start)
    {
        prospective_row_count=MT9V03X_H-white_start-10;
    }
    else
    {
        prospective_row_count=MT9V03X_H-10-prospective_row_start;
    }
    init_prospective_weights();

    for(int i = 0; i < MT9V03X_H; i++)
    {
        left_bound[i] = longest_column;
        right_bound[i] = longest_column;

        for(int j = longest_column; j >= 2; j--)
        {
            if(image[i][j] == 255 &&
               image[i][j-1] == 0 &&
               image[i][j-2] == 0)
            {
                left_bound[i] = j;
                left_lost[i] = 0;
                break;
            }
            else
            {
                left_bound[i] = 0;
            }
        }
        for(int j = longest_column; j < MT9V03X_W - 2; j++)
        {
            if(image[i][j] == 255 &&
               image[i][j+1] == 0 &&
               image[i][j+2] == 0)
            {
                right_bound[i] = j;
                right_lost[i] = 0;
                break;
            }
            else
            {
                right_bound[i] = MT9V03X_W-1;
            }
        }
    }
}

void cal_error(void)
{
    for(int i = 0; i < MT9V03X_H; i++)
    {
        if(left_lost[i]&&right_lost[i])
        {
            midline[i]=MT9V03X_W/2;
        }
        else
        {
            midline[i]=(left_bound[i]+right_bound[i])/2;
        }
    }
}

void calculate_pros_weighted_deviation(void)
{
    float sum_weighted_midline = 0;
    float sum_weights = 0;

    for(int i = 0; i < prospective_row_count; i++)
    {
        int row = prospective_row_start + i;
        if(row < MT9V03X_H)
        {
            sum_weighted_midline += prospective_weights[i] * midline[row];
            sum_weights += prospective_weights[i];
        }
    }
    if(sum_weights > 0)
    {
        int weighted_pros_midline = (int)(sum_weighted_midline / sum_weights);
        pros_weighted_deviation = (float)(weighted_pros_midline - (MT9V03X_W / 2));
    }
    else
    {
        pros_weighted_deviation = 0;
    }

    if(abs(pros_weighted_deviation)>=max_err)
    {
        if(pros_weighted_deviation>0)
        {
            pros_weighted_deviation=max_err;
        }
        else
        {
            pros_weighted_deviation=0-max_err;
        }
    }
}

static float calculate_midline_avg_slope(int start_row, int end_row)
{
    if(start_row < 0)
    {
        start_row = 0;
    }
    if(end_row >= MT9V03X_H)
    {
        end_row = MT9V03X_H - 1;
    }
    if(end_row - start_row < 2)
    {
        return 0.0f;
    }

    int prev_row = -1;
    float slope_sum = 0.0f;
    int slope_count = 0;

    for(int row = start_row; row <= end_row; row++)
    {
        if(left_lost[row] && right_lost[row])
        {
            continue;
        }

        if(prev_row >= 0)
        {
            int delta_row = row - prev_row;
            if(delta_row != 0)
            {
                slope_sum += (float)(midline[row] - midline[prev_row]) / (float)delta_row;
                slope_count++;
            }
        }

        prev_row = row;
    }

    if(slope_count == 0)
    {
        return 0.0f;
    }

    return slope_sum / slope_count;
}

void show_boundaries(void)
{
    float scale_x = (float)DISPLAY_WIDTH / MT9V03X_W;
    float scale_y = (float)DISPLAY_HEIGHT / MT9V03X_H;

    for(int i = 0; i < MT9V03X_H; i++)
    {
        uint16 y_scaled = (uint16)(i * scale_y);
        if(y_scaled >= DISPLAY_HEIGHT) continue;

        if(!left_lost[i] && left_bound[i] >= 0 && left_bound[i] < MT9V03X_W)
        {
            uint16 x_scaled = (uint16)(left_bound[i] * scale_x);
            if(x_scaled < DISPLAY_WIDTH && y_scaled < DISPLAY_HEIGHT)
            {
                tft180_draw_point(x_scaled, y_scaled, RGB565_GREEN);
            }
        }
        if(!right_lost[i] && right_bound[i] >= 0 && right_bound[i] < MT9V03X_W)
        {
            uint16 x_scaled = (uint16)(right_bound[i] * scale_x);
            if(x_scaled < DISPLAY_WIDTH && y_scaled < DISPLAY_HEIGHT)
            {
                tft180_draw_point(x_scaled, y_scaled, RGB565_RED);
            }
        }
        if(midline[i] >= 0 && midline[i] < MT9V03X_W)
        {
            uint16 x_scaled = (uint16)(midline[i] * scale_x);
            if(x_scaled < DISPLAY_WIDTH && y_scaled < DISPLAY_HEIGHT)
            {
                tft180_draw_point(x_scaled, y_scaled, RGB565_BLUE);
            }
        }
    }

    if(prospective_row_start >= 0)
    {
        uint16 y_start = (uint16)(prospective_row_start * scale_y);
        uint16 y_end = (uint16)((prospective_row_start + prospective_row_count) * scale_y);

        if(y_start < DISPLAY_HEIGHT)
        {
            for(int x = DISPLAY_WIDTH/4; x < (DISPLAY_WIDTH*3/4); x += 2)
            {
                tft180_draw_point(x, y_start, RGB565_PURPLE);
            }
        }
        if(y_end < DISPLAY_HEIGHT)
        {
            for(int x = DISPLAY_WIDTH/4; x < (DISPLAY_WIDTH*3/4); x += 2)
            {
                tft180_draw_point(x, y_end, RGB565_PURPLE);
            }
        }
    }
    tft180_show_float(0, DISPLAY_HEIGHT + 10, pros_weighted_deviation, 2, 1);
    tft180_show_float(0, DISPLAY_HEIGHT + 25, left_motor.current_speed, 2, 1);
    tft180_show_float(0, DISPLAY_HEIGHT + 40, left_motor.target_speed, 2, 1);
    tft180_show_float(0, DISPLAY_HEIGHT + 50, straight_flag, 2, 1);
    tft180_show_float(20, DISPLAY_HEIGHT + 50, white_start, 2, 1);
    tft180_show_float(50, DISPLAY_HEIGHT + 50, near_slope, 2, 2);
}

void straight_detect(void)
{
    straight_flag = 0;

    int bottom_end = MT9V03X_H - 1 - STRAIGHT_BOTTOM_OFFSET;
    if(bottom_end < 1)
    {
        return;
    }

    int bottom_start = bottom_end - STRAIGHT_BOTTOM_RANGE;
    if(bottom_start < 0)
    {
        bottom_start = 0;
    }

    near_slope = calculate_midline_avg_slope(bottom_start, bottom_end);

    if((fabsf(near_slope) <= STRAIGHT_SLOPE_THRESHOLD)&&white_start<=23)
    {
        straight_flag = 1;
    }
}

//����
void left_add_line(int x1, int y1, int x2, int y2)
{
    int i, max, a1, a2;
    int hx;
    // �߽���
    if(x1 >= MT9V03X_W-1) x1 = MT9V03X_W-1;
    else if(x1 <= 0) x1 = 0;
    if(y1 >= MT9V03X_H-1) y1 = MT9V03X_H-1;
    else if(y1 <= 0) y1 = 0;
    if(x2 >= MT9V03X_W-1) x2 = MT9V03X_W-1;
    else if(x2 <= 0) x2 = 0;
    if(y2 >= MT9V03X_H-1) y2 = MT9V03X_H-1;
    else if(y2 <= 0) y2 = 0;

    a1 = y1;
    a2 = y2;
    if(a1 > a2)
    {
        max = a1;
        a1 = a2;
        a2 = max;
    }
    for(i = a1; i <= a2; i++)
    {
        hx = (i - y1) * (x2 - x1) / (y2 - y1) + x1;
        if(hx >= MT9V03X_W) hx = MT9V03X_W;
        else if(hx <= 0) hx = 0;
        left_bound[i] = hx;
        left_lost[i] = 0;
    }
}

//����
void right_add_line(int x1, int y1, int x2, int y2)
{
    int i, max, a1, a2;
    int hx;

    // �߽���
    if(x1 >= MT9V03X_W-1) x1 = MT9V03X_W-1;
    else if(x1 <= 0) x1 = 0;
    if(y1 >= MT9V03X_H-1) y1 = MT9V03X_H-1;
    else if(y1 <= 0) y1 = 0;
    if(x2 >= MT9V03X_W-1) x2 = MT9V03X_W-1;
    else if(x2 <= 0) x2 = 0;
    if(y2 >= MT9V03X_H-1) y2 = MT9V03X_H-1;
    else if(y2 <= 0) y2 = 0;

    a1 = y1;
    a2 = y2;
    if(a1 > a2)
    {
        max = a1;
        a1 = a2;
        a2 = max;
    }

    for(i = a1; i <= a2; i++)
    {
        hx = (i - y1) * (x2 - x1) / (y2 - y1) + x1;
        if(hx >= MT9V03X_W) hx = MT9V03X_W;
        else if(hx <= 0) hx = 0;
        right_bound[i] = hx;
        right_lost[i] = 0;
    }
}

void find_down_point(int start, int end)
{
    int i, t;
    right_down_find = 0;
    left_down_find = 0;

    if(start < end)
    {
        t = start;
        start = end;
        end = t;
    }

    if(start >= MT9V03X_H-1-5)
        start = MT9V03X_H-1-5;
    if(end <= 5)
        end = 5;

    for(i = start; i >= end; i--)
    {
        // ��߽����¹յ���
        if(left_down_find == 0 &&
           abs(left_bound[i] - left_bound[i+1]) <= 5 &&
           abs(left_bound[i+1] - left_bound[i+2]) <= 5 &&
           abs(left_bound[i+2] - left_bound[i+3]) <= 5 &&
           (left_bound[i] - left_bound[i-2]) >= 8 &&
           (left_bound[i] - left_bound[i-3]) >= 15 &&
           (left_bound[i] - left_bound[i-4]) >= 15)
        {
            left_down_find = i;
        }

        // �ұ߽����¹յ���
        if(right_down_find == 0 &&
           abs(right_bound[i] - right_bound[i+1]) <= 5 &&
           abs(right_bound[i+1] - right_bound[i+2]) <= 5 &&
           abs(right_bound[i+2] - right_bound[i+3]) <= 5 &&
           (right_bound[i] - right_bound[i-2]) <= -8 &&
           (right_bound[i] - right_bound[i-3]) <= -15 &&
           (right_bound[i] - right_bound[i-4]) <= -15)
        {
            right_down_find = i;
        }

        if(left_down_find != 0 && right_down_find != 0)
        {
            break;
        }
    }
}

void find_up_point(int start, int end)
{
    int i, t;
    left_up_find = 0;
    right_up_find = 0;

    if(start < end)
    {
        t = start;
        start = end;
        end = t;
    }

    if(end <= 5)
        end = 5;
    if(start >= MT9V03X_H-1-5)
        start = MT9V03X_H-1-5;

    for(i = start; i >= end; i--)
    {
        // ��߽����Ϲյ���
        if(left_up_find == 0 &&
           abs(left_bound[i] - left_bound[i-1]) <= 5 &&
           abs(left_bound[i-1] - left_bound[i-2]) <= 5 &&
           abs(left_bound[i-2] - left_bound[i-3]) <= 5 &&
           (left_bound[i] - left_bound[i+2]) >= 8 &&
           (left_bound[i] - left_bound[i+3]) >= 15 &&
           (left_bound[i] - left_bound[i+4]) >= 15)
        {
            left_up_find = i;
        }

        // �ұ߽����Ϲյ���
        if(right_up_find == 0 &&
           abs(right_bound[i] - right_bound[i-1]) <= 5 &&
           abs(right_bound[i-1] - right_bound[i-2]) <= 5 &&
           abs(right_bound[i-2] - right_bound[i-3]) <= 5 &&
           (right_bound[i] - right_bound[i+2]) <= -8 &&
           (right_bound[i] - right_bound[i+3]) <= -15 &&
           (right_bound[i] - right_bound[i+4]) <= -15)
        {
            right_up_find = i;
        }

        if(left_up_find != 0 && right_up_find != 0)
        {
            break;
        }
    }

    if(abs(right_up_find - left_up_find) >= 30)
    {
        right_up_find = 0;
        left_up_find = 0;
    }
}


void cross_detect(void)
{
    int down_search_start = 0;
    cross_flag = 0;

    both_lost_time = 0;
    for(int i = 0; i < MT9V03X_H; i++)
    {
        if(left_lost[i] && right_lost[i])
        {
            both_lost_time++;
        }
    }

    left_up_find = 0;
    right_up_find = 0;
    left_down_find = 0;
    right_down_find = 0;

    if(both_lost_time >= 10)
    {
        find_up_point(MT9V03X_H-1, 0);

        if(left_up_find != 0 && right_up_find != 0)
        {
            cross_flag = 1;
            down_search_start = (left_up_find > right_up_find) ? left_up_find : right_up_find;
            find_down_point(MT9V03X_H-5, down_search_start + 2);
            if(left_down_find <= left_up_find)
                left_down_find = 0;
            if(right_down_find <= right_up_find)
                right_down_find = 0;

            if(left_down_find != 0 && right_down_find != 0)
            {
                left_add_line(left_bound[left_up_find], left_up_find,
                             left_bound[left_down_find], left_down_find);
                right_add_line(right_bound[right_up_find], right_up_find,
                              right_bound[right_down_find], right_down_find);
            }
            else if(left_down_find == 0 && right_down_find != 0)
            {
                for(int i = left_up_find-1; i < MT9V03X_H; i++)
                {
                    if(i+1 < MT9V03X_H && left_bound[i+1] != 0)
                    {
                        left_bound[i] = left_bound[i+1];
                        left_lost[i] = 0;
                    }
                }
                right_add_line(right_bound[right_up_find], right_up_find,
                              right_bound[right_down_find], right_down_find);
            }
            else if(left_down_find != 0 && right_down_find == 0)
            {
                // ֻ�����µ������� - ֱ���ӳ��ұ߽�
                left_add_line(left_bound[left_up_find], left_up_find,
                             left_bound[left_down_find], left_down_find);
                for(int i = right_up_find-1; i < MT9V03X_H; i++)
                {
                    if(i+1 < MT9V03X_H && right_bound[i+1] != 0)
                    {
                        right_bound[i] = right_bound[i+1];
                        right_lost[i] = 0;
                    }
                }
            }
            else
            {

                for(int i = left_up_find-1; i < MT9V03X_H; i++)
                {
                    if(i+1 < MT9V03X_H && left_bound[i+1] != 0)
                    {
                        left_bound[i] = left_bound[i+1];
                        left_lost[i] = 0;
                    }
                }
                for(int i = right_up_find-1; i < MT9V03X_H; i++)
                {
                    if(i+1 < MT9V03X_H && right_bound[i+1] != 0)
                    {
                        right_bound[i] = right_bound[i+1];
                        right_lost[i] = 0;
                    }
                }
            }
        }
    }
}
