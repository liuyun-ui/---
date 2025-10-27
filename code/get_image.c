/*
 * get_image.c
 *
 *  Created on: 2025年10月7日
 *      Author: 15286
 */
#include "zf_common_headfile.h"
#include "get_image.h"

int threshold=0;
uint8 image[MT9V03X_H][MT9V03X_W];
int left_bound[MT9V03X_H]={0};
int right_bound[MT9V03X_H]={0};
int midline[MT9V03X_H]={0};
uint8 left_lost[MT9V03X_H];
uint8 right_lost[MT9V03X_H];
int white_start=0;
int longest_column=MT9V03X_W/2;
// 十字路口相关变量
uint8 cross_flag = 0;
int left_down_find = 0;
int right_down_find = 0;
int left_up_find = 0;
int right_up_find = 0;
int both_lost_time = 0;

//直弯道检测
int straight_flag=0;
int cur_flag=0;


//前瞻行有关变量
int prospective_row_start = 0;      // 前瞻起始行
int prospective_row_end = 0;        // 前瞻结束行
int prospective_row_count = 20;     // 动态的前瞻行数量
int target_prospective_row_count=20;// 目标前瞻行数量（默认20，范围20-50）
float prospective_weights[50]={0};  // 前瞻行权重数组（近大远小）
float pros_weighted_deviation = 0;  // 前瞻行加权误差
float max_err=30;                   //误差限幅
// 显示区域定义
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 120  // 屏幕上半部分高度

void my_camera_init(void)
{
    my_tft_init();
    tft180_clear();
    mt9v03x_init();
    load_from_flash();
    mt9v03x_set_exposure_time(exposure_time);         //初始化时设置曝光
    memset(left_lost, 1, sizeof(left_lost));
    memset(right_lost, 1, sizeof(right_lost));
    init_prospective_weights();
}

void init_prospective_weights(void)
{
    float total_weight = 0;

    for(int i = 0; i < prospective_row_count; i++)
    {
        // 线性递增权重：从起始行向下，行号越大权重越大
        prospective_weights[i] = (float)(i + 1);  // 第0行权重1，第1行权重2，以此类推
        total_weight += prospective_weights[i];
    }
    // 归一化权重
    for(int i = 0; i < prospective_row_count; i++)
    {
        prospective_weights[i] /= total_weight;
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
            show_boundaries();//显示边界
        }
        search_line();//巡线
        //cross_detect();
        straight_detect();
        cal_error();//找中线
        calculate_pros_weighted_deviation();//计算前瞻行的加权误差

        //show_boundaries();//显示边界
        mt9v03x_finish_flag = 0;
    }
}

/*
int My_Adapt_Threshold(uint8 image[][MT9V03X_W],uint16 width, uint16 height)   //大津算法，注意计算阈值的一定要是原图像
{
    #define GrayScale 256
    int pixelCount[GrayScale];
    float pixelPro[GrayScale];
    int i, j;
    int pixelSum = width * height/4;
    int  threshold = 0;

    for (i = 0; i < GrayScale; i++)
    {
        pixelCount[i] = 0;
        pixelPro[i] = 0;
    }

    uint32 gray_sum=0;

    for (i = 0; i < height; i+=2)//统计灰度级中每个像素在整幅图像中的个数
    {
        for (j = 0; j <width; j+=2)
        {
            pixelCount[(int)image[i][j]]++;    //统计每个灰度级的像素数量
            gray_sum += (int)image[i][j];      //灰度值总和
        }
    }

    for (i = 0; i < GrayScale; i++) //计算每个像素值的点在整幅图像中的比例
    {
        pixelPro[i] = (float)pixelCount[i] / pixelSum;
    }

    float w0, w1, u0tmp, u1tmp, u0, u1, u, deltaTmp, deltaMax = 0;
    w0 = w1 = u0tmp = u1tmp = u0 = u1 = u = deltaTmp = 0;

    for (j = 0; j < GrayScale; j++)//遍历灰度级[0,255]
    {
        w0 += pixelPro[j];  //背景部分每个灰度值的像素点所占比例之和   即背景部分的比例
        u0tmp += j * pixelPro[j];  //背景部分 每个灰度值的点的比例 *灰度值
        w1=1-w0;
        u1tmp=gray_sum/pixelSum-u0tmp;
        u0 = u0tmp / w0;              //背景平均灰度
        u1 = u1tmp / w1;              //前景平均灰度
        u = u0tmp + u1tmp;            //全局平均灰度
        deltaTmp = w0 * pow((u0 - u), 2) + w1 * pow((u1 - u), 2);//平方
        if (deltaTmp > deltaMax)
        {
            deltaMax = deltaTmp;//最大类间方差法
            threshold = j;
        }
        if (deltaTmp < deltaMax)
        {
            break;
        }
    }
    if(threshold>255)
        threshold=255;
    if(threshold<0)
        threshold=0;
  return threshold;
}
*/

//大津法

int My_Adapt_Threshold(uint8 image[][MT9V03X_W], uint16 width, uint16 height)
{
    #define GrayScale 256
    int pixelCount[GrayScale] = {0};
    float pixelPro[GrayScale] = {0};
    int i, j;
    int pixelSum = width * height / 4;  // 注意：这里采样了1/4的像素
    int threshold = 0;

    uint32 gray_sum = 0;

    // 统计直方图（采样1/4像素）
    for (i = 0; i < height; i += 2) {
        for (j = 0; j < width; j += 2) {
            pixelCount[(int)image[i][j]]++;
            gray_sum += (int)image[i][j];
        }
    }

    // 计算概率分布
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
        // 删除提前终止条件！
    }

    // 边界检查
    if (threshold > 255) threshold = 255;
    if (threshold < 0) threshold = 0;

    return threshold;
}

/*
uint8 My_Adapt_Threshold(uint8 *image, uint16 col, uint16 row)
{
    #define GrayScale 256//定义256个灰度级
    uint16 width = col;   //图像宽度
    uint16 height = row;  //图像长度
    int pixelCount[GrayScale];  //每个灰度值所占像素个数
    float pixelPro[GrayScale]; //每个灰度值所占总像素比例
    int i, j;
    int sumPixel = width * height;//总像素点
    uint8 threshold = 0; //最佳阈值
    uint8* data = image;  //指向像素数据的指针
    for (i = 0; i < GrayScale; i++)
    {
        pixelCount[i] = 0;
        pixelPro[i] = 0;
    }
    //统计灰度级中每个像素在整幅图像中的个数
    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            pixelCount[(int)data[i * width + j]]++;  //将像素值作为计数数组的下标
             //   pixelCount[(int)image[i][j]]++;    若不用指针用这个
        }
    }
    float w0=0, w1=0, u0Sum=0, u1Sum=0, u0=0, u1=0, u=0, variance=0, maxVariance = 0;
    for (i = 0; i < GrayScale; i++)
    {
        pixelPro[i] = (float)pixelCount[i] / sumPixel;
         u += i * pixelPro[i];  //总平均灰度
    }
    for (i = 0; i < GrayScale; i++)     // i作为阈值 阈值从1-255遍历
    {

        for (j = 0; j < GrayScale; j++)    //求目标前景和背景
        {
            if (j <= i)   //前景部分
            {
                w0 += pixelPro[j];
                u0Sum += j * pixelPro[j];
            }
            else   //背景部分
            {
                w1 += pixelPro[j];
                u1Sum += j * pixelPro[j];
            }
        }
        u0 = u0Sum / w0;
        u1 = u1Sum / w1;
        variance = w0 * pow((u0 - u), 2) + w1 * pow((u1 - u), 2);  //类间方差计算公式
        if (variance > maxVariance)   //判断是否为最大类间方差
        {
            maxVariance = variance;
            threshold = i;
        }
    }
    return threshold;
}
*/
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
    // 确定最长白列所在行
    white_start=MT9V03X_H-5;
    for(int i = MT9V03X_H-5; i >0; i--)
    {
        if(image[i][longest_column] == 0)
        {
            white_start= i;
            break;
        }
    }
    //目标的前瞻起始行 是在MT9V03X_H-20-target_prospective_row_count处
    //但如果起始行比白列所在行还要前，就只取到白列所在行
    //如果MT9V03X_H-20还在白列前，就把图像底部到白列全作为前瞻行
    int target_prospective_row_start=MT9V03X_H-20-target_prospective_row_count;
    if(target_prospective_row_start<white_start)
    {
        prospective_row_start=white_start;
    }
    else
    {
        prospective_row_start=target_prospective_row_start;
    }
    if(MT9V03X_H-20<white_start)
    {
        prospective_row_count=MT9V03X_H-white_start-1;
    }
    else
    {
        prospective_row_count=MT9V03X_H-20-prospective_row_start;
    }
    init_prospective_weights();

    for(int i = 0; i < MT9V03X_H; i++)
    {
        // 初始化当前行的边界为最长白列位置
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
//计算中线
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
//计算误差
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
        //pros_weighted_deviation=max_err;
    }
}

void show_boundaries(void)
{
    // 使用显示区域的尺寸进行计算
    float scale_x = (float)DISPLAY_WIDTH / MT9V03X_W;
    float scale_y = (float)DISPLAY_HEIGHT / MT9V03X_H;

    for(int i = 0; i < MT9V03X_H; i++)
    {
        uint16 y_scaled = (uint16)(i * scale_y);
        // 只显示在显示区域内
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

        // 只在显示区域内绘制
        if(y_start < DISPLAY_HEIGHT)
        {
            // 绘制起始行（最长白列所在行）
            for(int x = DISPLAY_WIDTH/4; x < (DISPLAY_WIDTH*3/4); x += 2)
            {
                tft180_draw_point(x, y_start, RGB565_PURPLE);
            }
        }
        if(y_end < DISPLAY_HEIGHT)
        {
            // 绘制结束行
            for(int x = DISPLAY_WIDTH/4; x < (DISPLAY_WIDTH*3/4); x += 2)
            {
                tft180_draw_point(x, y_end, RGB565_PURPLE);
            }
        }
    }
    // 在显示区域下方显示前瞻误差
    tft180_show_float(0, DISPLAY_HEIGHT + 10, pros_weighted_deviation, 2, 1);
}

void straight_detect(void)
{
    straight_flag=0;
    if(white_start<=50)   //参数需调
    {
        if(pros_weighted_deviation>=-5&&pros_weighted_deviation<=5)
        {
            straight_flag=1;
        }
    }
}

//补线
void left_add_line(int x1, int y1, int x2, int y2)
{
    int i, max, a1, a2;
    int hx;
    // 边界检查
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

//补线
void right_add_line(int x1, int y1, int x2, int y2)
{
    int i, max, a1, a2;
    int hx;

    // 边界检查
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
        // 左边界向下拐点检测
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

        // 右边界向下拐点检测
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
        // 左边界向上拐点检测
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

        // 右边界向上拐点检测
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

    // 纵向撕裂过大，视为误判
    if(abs(right_up_find - left_up_find) >= 30)
    {
        right_up_find = 0;
        left_up_find = 0;
    }
}


//十字检测
void cross_detect(void)
{
    int down_search_start = 0;
    cross_flag = 0;

    // 统计双边丢线时间
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

    // 十字路口通常有双边丢线
    if(both_lost_time >= 10)
    {
        find_up_point(MT9V03X_H-1, 0);

        // 必须同时找到两个上拐点才认为是十字
        if(left_up_find != 0 && right_up_find != 0)
        {
            cross_flag = 1;
            // 用两个上拐点中靠下的作为下点搜索上限
            down_search_start = (left_up_find > right_up_find) ? left_up_find : right_up_find;
            find_down_point(MT9V03X_H-5, down_search_start + 2);
            // 检查下点位置合理性
            if(left_down_find <= left_up_find)
                left_down_find = 0;
            if(right_down_find <= right_up_find)
                right_down_find = 0;

            // 根据找到的点进行补线
            if(left_down_find != 0 && right_down_find != 0)
            {
                // 四个点都在
                left_add_line(left_bound[left_up_find], left_up_find,
                             left_bound[left_down_find], left_down_find);
                right_add_line(right_bound[right_up_find], right_up_find,
                              right_bound[right_down_find], right_down_find);
            }
            else if(left_down_find == 0 && right_down_find != 0)
            {
                // 只有右下的三个点 - 直接延长左边界
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
                // 只有左下的三个点 - 直接延长右边界
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
                // 只有两个上点 - 直接延长双边
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
