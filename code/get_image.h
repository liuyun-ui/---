/*
 * get_image.h
 *
 *  Created on: 2025Äê10ÔÂ7ÈÕ
 *      Author: 15286
 */

#ifndef CODE_GET_IMAGE_H_
#define CODE_GET_IMAGE_H_

extern int target_prospective_row_count;
extern float pros_weighted_deviation;
extern int straight_flag;
extern int white_start;

void my_camera_init(void);
void camera_display(void);
void image_binary(void);
int My_Adapt_Threshold(uint8 image[][MT9V03X_W], uint16 width, uint16 height);
void show_boundaries(void);
void search_line(void);
void cal_error(void);
void init_prospective_weights(void);
void calculate_pros_weighted_deviation(void);

void left_add_line(int x1, int y1, int x2, int y2);
void right_add_line(int x1, int y1, int x2, int y2);
void find_down_point(int start, int end);
void find_up_point(int start, int end);
void cross_detect(void);
void straight_detect(void);
#endif /* CODE_GET_IMAGE_H_ */
