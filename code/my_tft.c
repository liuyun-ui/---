/*
 * my_tft.c
 *
 *  Created on: 2025锟斤拷10锟斤拷3锟斤拷
 *      Author: 15286
 */
#include "zf_common_headfile.h"
#include "my_tft.h"
#include "get_image.h"

#define PAGE_COUNT          3       // 锟斤拷页锟斤拷
#define PARAM_PER_PAGE      9       // 每页锟斤拷锟斤拷锟斤拷锟斤拷
#define PARAM_FLASH_PAGE    10

typedef enum
{
    STATE_NORMAL=0,
    STATE_SELECT_PARAM,         //选锟斤拷锟斤拷
    STATE_SELECT_DIGIT,         //选位锟斤拷
    STATE_EDIT_DIGIT            //锟斤拷位锟斤拷
}state_enum;

typedef struct
{
    char name[10];
    uint8 digit[5];
}param;

static uint8 current_page=0;
static state_enum system_state=STATE_NORMAL;
static uint8 selected_param=0;
static uint8 selected_digit=0;
uint8 system_in_setting_mode = 0;
uint16 exposure_time = 100;
static uint32 button_press_counter = 0;  // 鎸夐挳鎸変笅璁℃暟鍣�

static param arr[PAGE_COUNT][PARAM_PER_PAGE] =
{
    // 锟斤拷一页锟斤拷锟斤拷
    {
        {"P-Rows", {0, 0, 0, 2, 0}},
        {"Expose", {0, 0, 1, 0, 0}},
        {"P_M   ", {0, 0, 2, 0, 0}},  //20.0

        {"I_M   ", {0, 0, 0, 0, 5}},  //0.5
        {"P_S   ", {0, 0, 1, 0, 0}},  //100
        {"D_S   ", {0, 1, 0, 0, 0}},  //1000

        {"BASESP", {0, 0, 8, 0, 0}},
        {"STR_SP", {0, 1, 0, 0, 0}},  //1000
        {"SHIFTR", {0, 0, 0, 1, 5}}   //1.5

    },
    // 锟节讹拷页锟斤拷锟斤拷
    {
        {"MID   ", {0, 3, 7, 5, 0}},
        {"Humid ", {0, 6, 5, 0, 0}},
        {"Press ", {1, 0, 1, 3, 0}},

        {"Temp2 ", {2, 5, 1, 0, 0}},
        {"Humid2", {0, 7, 5, 0, 0}},
        {"Press2", {1, 2, 1, 3, 0}},

        {"Temp3 ", {2, 5, 0, 0, 1}},
        {"Humid3", {0, 6, 5, 2, 0}},
        {"Press3", {1, 0, 1, 5, 0}}

    },
    // 锟斤拷锟斤拷页锟斤拷锟斤拷
    {
        {"Time  ", {0, 1, 2, 3, 0}},
        {"Date  ", {2, 3, 0, 4, 0}},
        {"Mode  ", {0, 0, 0, 0, 1}},

        {"Time2 ", {0, 1, 2, 6, 0}},
        {"Date2 ", {2, 3, 0, 8, 0}},
        {"Mode2 ", {0, 0, 0, 9, 1}},

        {"Time3 ", {0, 1, 3, 3, 0}},
        {"Date3 ", {2, 3, 7, 4, 0}},
        {"Mode3 ", {0, 0, 2, 0, 1}}
    }
};



void my_tft_init(void)
{
    tft180_init();
    tft180_set_color(RGB565_BLACK, RGB565_WHITE);
    key_init(5);
    load_from_flash();
    if(target_prospective_row_count < 20 || target_prospective_row_count > 50)
    {
        target_prospective_row_count = 20;  // 强锟斤拷锟斤拷为默锟斤拷值
        arr[0][0].digit[3] = target_prospective_row_count / 10;  // 十位
        arr[0][0].digit[4] = target_prospective_row_count % 10;  // 锟斤拷位
    }
}

void save_to_flash(void)
{
    flash_erase_page(0,10);
    flash_buffer_clear();
    memcpy(flash_union_buffer,arr,sizeof(arr));
    flash_write_page_from_buffer(0,10);
    system_delay_ms(10);
}

void load_from_flash(void)
{
/*
    if(flash_check(0,10))
    {
        flash_read_page_to_buffer(0,10);
        memcpy(arr,flash_union_buffer,sizeof(arr));
        flash_buffer_clear();
        target_prospective_row_count = arr[0][0].digit[3] * 10 + arr[0][0].digit[4];
        // 锟斤拷围锟斤拷锟�
        if(target_prospective_row_count < 20) target_prospective_row_count = 20;
        if(target_prospective_row_count > 50) target_prospective_row_count = 50;

        exposure_time = arr[0][1].digit[2] * 100 + arr[0][1].digit[3] * 10 + arr[0][1].digit[4];
        if(exposure_time < 10) exposure_time = 10;
        if(exposure_time > 999) exposure_time = 999;

        P_S = (float)(arr[0][4].digit[2] * 100 + arr[0][4].digit[3] * 10 + arr[0][4].digit[4]);
        D_S = (float)(arr[0][5].digit[1] * 1000 + arr[0][5].digit[2] * 100 +
                     arr[0][5].digit[3] * 10 + arr[0][5].digit[4]);
    }
*/
      //else
     //{
        save_to_flash();
     //}
}

//锟斤拷锟斤拷锟斤拷锟斤拷  P20_6(搴熷純), P20_7, P11_2, P11_3
void key_handle(void)
{
    // P20_6 鎸夐挳宸插簾寮冿紝涓嶅啀澶勭悊
    
    if(gpio_get_level(P20_7)==0)
    {
        button_press_counter++;
        
        // 妫�娴嬮暱鎸夛紙璁℃暟鍣ㄨ秴杩�100锛岀浉褰撲簬绾�500ms锛�
        if(button_press_counter > 100)
        {
            // 闀挎寜閫昏緫
            if(system_in_setting_mode)
            {
                // 鍦ㄤ笅浣嶆満鐣岄潰鏃讹紝闀挎寜缈婚〉
                if(system_state==STATE_NORMAL)
                {
                    current_page=(current_page+1)%3;
                    display_current_page();
                }
                else if(system_state==STATE_SELECT_PARAM)
                {
                    current_page=(current_page+1)%3;
                    display_current_page();
                }
            }
            button_press_counter = 0;  // 閲嶇疆璁℃暟鍣�
        }
    
        else
        {
            if(button_press_counter > 0 && button_press_counter <= 100)
            {
                // 鐭寜閫昏緫
                if(!system_in_setting_mode)
                {
                    // 鍦ㄦ憚鍍忓ご鐣岄潰鏃讹紝鐭寜鍒囨崲鍒颁笅浣嶆満椤甸潰
                    system_in_setting_mode = !system_in_setting_mode;
                    tft180_clear();
                    display_current_page();
                    system_state = STATE_NORMAL;
                    current_page = 0;
                    selected_param = 0;
                    selected_digit = 0;
                }
                else if(system_in_setting_mode)
                {
                    // 鍦ㄤ笅浣嶆満鐣岄潰鏃讹紝鐭寜淇敼鍙傛暟
                    switch(system_state)
                    {
                        case STATE_NORMAL:
                            system_state=STATE_SELECT_PARAM;
                            selected_param = 0;
                            selected_digit = 0;
                            break;

                        case STATE_SELECT_PARAM:
                        selected_param=(selected_param+1)%PARAM_PER_PAGE;
                            break;

                        case STATE_SELECT_DIGIT:
                            system_state=STATE_SELECT_PARAM;
                            break;

                        case STATE_EDIT_DIGIT:
                            system_state=STATE_SELECT_PARAM;
                            //save_to_flash();
                            break;
                    }
                    display_current_page();
                }
            }
            button_press_counter = 0;  // 閲嶇疆璁℃暟鍣�
        }
    }

    else if(gpio_get_level(P11_2)==0)
    {
        if(system_in_setting_mode)
        {
            system_delay_ms(30);
            while(gpio_get_level(P11_2)==0);
            system_delay_ms(30);

            if(system_state==STATE_SELECT_PARAM)
            {
                system_state=STATE_SELECT_DIGIT;
                selected_digit=0;
            }
            else if(system_state==STATE_SELECT_DIGIT)
            {
                selected_digit=(selected_digit+1)%5;
            }
            else if(system_state==STATE_EDIT_DIGIT)
            {
                system_state=STATE_SELECT_DIGIT;
            }
            display_current_page();
        }

    }

    else if(gpio_get_level(P11_3)==0)
    {
        system_delay_ms(30);
        while(gpio_get_level(P11_3)==0);
        system_delay_ms(30);

        if(system_in_setting_mode)
        {
            if(system_state == STATE_NORMAL||system_state == STATE_SELECT_PARAM)
            {
                system_in_setting_mode = !system_in_setting_mode;
                save_to_flash();
                if(!system_in_setting_mode)
                {
                    tft180_clear();  // 锟剿筹拷时锟斤拷锟斤拷锟侥�
                }
                return;  // 直锟接凤拷锟斤拷
            }
            if(system_state==STATE_SELECT_DIGIT)
            {
                system_state=STATE_EDIT_DIGIT;
                arr[current_page][selected_param].digit[selected_digit]=(arr[current_page][selected_param].digit[selected_digit]+1)%10;
                display_current_page();
            }
            else if(system_state==STATE_EDIT_DIGIT)
            {
                arr[current_page][selected_param].digit[selected_digit]=(arr[current_page][selected_param].digit[selected_digit]+1)%10;
                /*
                if(current_page == 0 && selected_param == 0)
                {
                    target_prospective_row_count = arr[0][0].digit[3] * 10 + arr[0][0].digit[4];
                    // 锟斤拷围锟斤拷锟斤拷
                    if(target_prospective_row_count < 20) target_prospective_row_count = 20;
                    if(target_prospective_row_count > 50) target_prospective_row_count = 50;
                    init_prospective_weights();
                }
                else if(current_page == 0 && selected_param == 1)
                {
                    exposure_time = arr[0][1].digit[2] * 100 + arr[0][1].digit[3] * 10 + arr[0][1].digit[4];

                    // 锟斤拷围锟斤拷锟斤拷
                    if(exposure_time < 10) exposure_time = 10;
                    if(exposure_time > 999) exposure_time = 999;

                    // 锟斤拷锟斤拷锟斤拷锟斤拷头锟截癸拷时锟斤拷
                    mt9v03x_set_exposure_time(exposure_time);
                }
                */
                if(current_page == 0)
                {
                    switch(selected_param)
                    {
                        case 0: // P-Rows 前瞻锟斤拷
                            target_prospective_row_count = arr[0][0].digit[3] * 10 + arr[0][0].digit[4];
                            // 锟斤拷围锟斤拷锟斤拷
                            if(target_prospective_row_count < 20) target_prospective_row_count = 20;
                            if(target_prospective_row_count > 50) target_prospective_row_count = 50;
                            init_prospective_weights();
                            break;

                        case 1: // Expose 锟截癸拷
                            exposure_time = arr[0][1].digit[2] * 100 + arr[0][1].digit[3] * 10 + arr[0][1].digit[4];
                            // 锟斤拷围锟斤拷锟斤拷
                            if(exposure_time < 10) exposure_time = 10;
                            if(exposure_time > 999) exposure_time = 999;
                            // 锟斤拷锟斤拷锟斤拷锟斤拷头锟截癸拷时锟斤拷
                            mt9v03x_set_exposure_time(exposure_time);
                            break;

                        case 4: // P_S 锟斤拷锟斤拷锟斤拷锟较碉拷锟� (100)
                            // 锟斤拷锟芥储锟斤拷锟斤拷值转锟斤拷为实锟绞碉拷P_S锟斤拷锟斤拷 (3位锟斤拷锟斤拷)
                            P_S = (float)(arr[0][4].digit[2] * 100 + arr[0][4].digit[3] * 10 + arr[0][4].digit[4]);
                            break;

                        case 5: // D_S 锟斤拷锟轿拷锟较碉拷锟� (1000)
                            // 锟斤拷锟芥储锟斤拷锟斤拷值转锟斤拷为实锟绞碉拷D_S锟斤拷锟斤拷 (4位锟斤拷锟斤拷)
                            D_S = (float)(arr[0][5].digit[1] * 1000 + arr[0][5].digit[2] * 100 +
                                         arr[0][5].digit[3] * 10 + arr[0][5].digit[4]);
                            break;
                   }
                   display_current_page();
                }
                else if(current_page == 1)
                {
                    switch(selected_param)
                    {
                        case 0:
                            STEER_MID=(arr[1][0].digit[1] * 1000 + arr[1][0].digit[2] * 100 +
                            arr[1][0].digit[3] * 10 + arr[1][0].digit[4]);
                            break;
                    }
                    display_current_page();
                }
           }



        }
        // 锟斤拷锟斤拷状态锟侥达拷锟斤拷
    }
}

/*
//锟斤拷锟斤拷锟斤拷锟斤拷  P20_6(搴熷純), P20_7, P11_2, P11_3
void key_handle(void)
{
    // P20_6 鎸夐挳宸插簾寮冿紝涓嶅啀澶勭悊
    
    if(gpio_get_level(P20_7)==0)
    {
        button_press_counter++;
        
        // 妫�娴嬮暱鎸夛紙璁℃暟鍣ㄨ秴杩�100锛岀浉褰撲簬绾�500ms锛�
        if(button_press_counter > 100)
        {
            // 闀挎寜閫昏緫
            if(system_in_setting_mode)
            {
                // 鍦ㄤ笅浣嶆満鐣岄潰鏃讹紝闀挎寜缈婚〉
                if(system_state==STATE_NORMAL)
                {
                    current_page=(current_page+1)%3;
                    display_current_page();
                }
                else if(system_state==STATE_SELECT_PARAM)
                {
                    current_page=(current_page+1)%3;
                    display_current_page();
                }
            }
            button_press_counter = 0;  // 閲嶇疆璁℃暟鍣�
        }
    }
    else
    {
        // 鎸夐挳閲婃斁鏃跺鐞嗙煭鎸夐�昏緫
        if(button_press_counter > 0 && button_press_counter <= 100)
        {
            // 鐭寜閫昏緫
            if(!system_in_setting_mode)
            {
                // 鍦ㄦ憚鍍忓ご鐣岄潰鏃讹紝鐭寜鍒囨崲鍒颁笅浣嶆満椤甸潰
                system_in_setting_mode = !system_in_setting_mode;
                tft180_clear();
                display_current_page();
                system_state = STATE_NORMAL;
                current_page = 0;
                selected_param = 0;
                selected_digit = 0;
            }
            else if(system_in_setting_mode)
            {
                // 鍦ㄤ笅浣嶆満鐣岄潰鏃讹紝鐭寜淇敼鍙傛暟
                switch(system_state)
                {
                    case STATE_NORMAL:
                        system_state=STATE_SELECT_PARAM;
                        selected_param = 0;
                        selected_digit = 0;
                        break;

                    case STATE_SELECT_PARAM:
                       selected_param=(selected_param+1)%PARAM_PER_PAGE;
                        break;

                    case STATE_SELECT_DIGIT:
                        system_state=STATE_SELECT_PARAM;
                        break;

                    case STATE_EDIT_DIGIT:
                        system_state=STATE_SELECT_PARAM;
                        //save_to_flash();
                        break;
                }
                display_current_page();
            }
        }
        button_press_counter = 0;  // 閲嶇疆璁℃暟鍣�
    }

    else if(gpio_get_level(P11_2)==0)
    {
        if(system_in_setting_mode)
        {
            system_delay_ms(30);
            while(gpio_get_level(P11_2)==0);
            system_delay_ms(30);

            if(system_state==STATE_SELECT_PARAM)
            {
                system_state=STATE_SELECT_DIGIT;
                selected_digit=0;
            }
            else if(system_state==STATE_SELECT_DIGIT)
            {
                selected_digit=(selected_digit+1)%5;
            }
            else if(system_state==STATE_EDIT_DIGIT)
            {
                system_state=STATE_SELECT_DIGIT;
            }
            display_current_page();
        }

    }

    else if(gpio_get_level(P11_3)==0)
    {
        system_delay_ms(30);
        while(gpio_get_level(P11_3)==0);
        system_delay_ms(30);

        if(system_in_setting_mode)
        {
            if(system_state == STATE_NORMAL||system_state == STATE_SELECT_PARAM)
            {
                system_in_setting_mode = !system_in_setting_mode;
                save_to_flash();
                if(!system_in_setting_mode)
                {
                    tft180_clear();  // 锟剿筹拷时锟斤拷锟斤拷锟侥�
                }
                return;  // 直锟接凤拷锟斤拷
            }
            if(system_state==STATE_SELECT_DIGIT)
            {
                system_state=STATE_EDIT_DIGIT;
                arr[current_page][selected_param].digit[selected_digit]=(arr[current_page][selected_param].digit[selected_digit]+1)%10;
                display_current_page();
            }
            else if(system_state==STATE_EDIT_DIGIT)
            {
                arr[current_page][selected_param].digit[selected_digit]=(arr[current_page][selected_param].digit[selected_digit]+1)%10;

                if(current_page == 0 && selected_param == 0)
                {
                    target_prospective_row_count = arr[0][0].digit[3] * 10 + arr[0][0].digit[4];
                    // 锟斤拷围锟斤拷锟斤拷
                    if(target_prospective_row_count < 20) target_prospective_row_count = 20;
                    if(target_prospective_row_count > 50) target_prospective_row_count = 50;
                    init_prospective_weights();
                }
                else if(current_page == 0 && selected_param == 1)
                {
                    exposure_time = arr[0][1].digit[2] * 100 + arr[0][1].digit[3] * 10 + arr[0][1].digit[4];

                    // 锟斤拷围锟斤拷锟斤拷
                    if(exposure_time < 10) exposure_time = 10;
                    if(exposure_time > 999) exposure_time = 999;

                    // 锟斤拷锟斤拷锟斤拷锟斤拷头锟截癸拷时锟斤拷
                    mt9v03x_set_exposure_time(exposure_time);
                }

                if(current_page == 0)
                {
                    switch(selected_param)
                    {
                        case 0: // P-Rows 前瞻锟斤拷
                            target_prospective_row_count = arr[0][0].digit[3] * 10 + arr[0][0].digit[4];
                            // 锟斤拷围锟斤拷锟斤拷
                            if(target_prospective_row_count < 20) target_prospective_row_count = 20;
                            if(target_prospective_row_count > 50) target_prospective_row_count = 50;
                            init_prospective_weights();
                            break;

                        case 1: // Expose 锟截癸拷
                            exposure_time = arr[0][1].digit[2] * 100 + arr[0][1].digit[3] * 10 + arr[0][1].digit[4];
                            // 锟斤拷围锟斤拷锟斤拷
                            if(exposure_time < 10) exposure_time = 10;
                            if(exposure_time > 999) exposure_time = 999;
                            // 锟斤拷锟斤拷锟斤拷锟斤拷头锟截癸拷时锟斤拷
                            mt9v03x_set_exposure_time(exposure_time);
                            break;

                        case 4: // P_S 锟斤拷锟斤拷锟斤拷锟较碉拷锟� (100)
                            // 锟斤拷锟芥储锟斤拷锟斤拷值转锟斤拷为实锟绞碉拷P_S锟斤拷锟斤拷 (3位锟斤拷锟斤拷)
                            P_S = (float)(arr[0][4].digit[2] * 100 + arr[0][4].digit[3] * 10 + arr[0][4].digit[4]);
                            break;

                        case 5: // D_S 锟斤拷锟轿拷锟较碉拷锟� (1000)
                            // 锟斤拷锟芥储锟斤拷锟斤拷值转锟斤拷为实锟绞碉拷D_S锟斤拷锟斤拷 (4位锟斤拷锟斤拷)
                            D_S = (float)(arr[0][5].digit[1] * 1000 + arr[0][5].digit[2] * 100 +
                                         arr[0][5].digit[3] * 10 + arr[0][5].digit[4]);
                            break;
                   }
                   display_current_page();
                }
                else if(current_page == 1)
                {
                    switch(selected_param)
                    {
                        case 0:
                            STEER_MID=(arr[1][0].digit[1] * 1000 + arr[1][0].digit[2] * 100 +
                            arr[1][0].digit[3] * 10 + arr[1][0].digit[4]);
                            break;
                    }
                    display_current_page();
                }
           }



        }
        // 锟斤拷锟斤拷状态锟侥达拷锟斤拷
    }
}
*/
void display_current_page(void)
{
    tft180_clear();
    uint16 y_pos=0;
    char num_string[7];
    for(int i=0;i<PARAM_PER_PAGE;i++)
    {
        y_pos=i*16;
        sprintf(num_string,"%d%d%d%d%d",arr[current_page][i].digit[0],
                arr[current_page][i].digit[1],
                arr[current_page][i].digit[2],
                arr[current_page][i].digit[3],
                arr[current_page][i].digit[4]);
        if(system_state >= STATE_SELECT_PARAM && selected_param == i)
        {
            //选锟斤拷锟斤拷锟斤拷一锟斤拷锟斤拷锟斤拷
            if(system_state >= STATE_SELECT_DIGIT)//锟斤拷始选锟斤拷位锟斤拷
            {
                uint8 j=0;
                tft180_set_color(RGB565_BLACK, RGB565_WHITE);
                for(j=0;j<selected_digit;j++)
                {
                    tft180_show_char(70 + j *8, y_pos, num_string[j]);
                }

                tft180_set_color(RGB565_BLUE, RGB565_WHITE);
                tft180_show_char(70 + j *8, y_pos, num_string[j]);

                tft180_set_color(RGB565_BLACK, RGB565_WHITE);
                for(j=selected_digit+1;j<5;j++)
                {
                    tft180_show_char(70 + j * 8, y_pos, num_string[j]);
                }

                tft180_set_color(RGB565_BLUE, RGB565_WHITE);
                tft180_show_string(0, y_pos,arr[current_page][i].name );
            }
            else
            {
                tft180_set_color(RGB565_BLACK, RGB565_WHITE);
                tft180_show_string(70, y_pos, num_string);

                tft180_set_color(RGB565_BLUE, RGB565_WHITE);
                tft180_show_string(0, y_pos,arr[current_page][i].name);
            }
        }
        else
        {
            tft180_set_color(RGB565_BLACK, RGB565_WHITE);
            tft180_show_string(0, y_pos,arr[current_page][i].name);
            tft180_show_string(70, y_pos, num_string);
        }
    }
}

void my_tft_work(void)
{
    key_scanner();
    key_handle();

}































































