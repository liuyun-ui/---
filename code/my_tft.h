/*
 * my_tft.h
 *
 *  Created on: 2025Äê10ÔÂ4ÈÕ
 *      Author: 15286
 */

#ifndef CODE_MY_TFT_H_
#define CODE_MY_TFT_H_

extern uint8 system_in_setting_mode;
extern uint16 exposure_time;
void my_tft_init(void);
void key_handle(void);
void display_current_page(void);
void my_tft_work(void);
void save_to_flash(void);
void load_from_flash(void);



#endif /* CODE_MY_TFT_H_ */
