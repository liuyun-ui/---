/*
 * my_tft.c
 *
 *  Created on: 2025年10月3日
 *      Author: 15286
 */
#include "zf_common_headfile.h"
#include "my_tft.h"
#include "get_image.h"
#include "control.h"

#define PAGE_COUNT          3       // 总页数
#define PARAM_PER_PAGE      9       // 每页参数个数
#define PARAM_FLASH_PAGE    10

typedef enum
{
    STATE_NORMAL=0,
    STATE_SELECT_PARAM,         //选择参数
    STATE_SELECT_DIGIT,         //选位数
    STATE_EDIT_DIGIT            //编辑位数
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
static uint32 button_press_counter = 0;  // 按钮按下计数器

static param arr[PAGE_COUNT][PARAM_PER_PAGE] =
{
    // 第一页参数
    {
        {"P-Rows", {0, 0, 0, 2, 0}},
        {"Expose", {0, 0, 1, 0, 0}},
        {"P_M   ", {0, 0, 0, 0, 0}},  //0.0 (格式: 00000 = 0×10 = 0.0)

        {"I_M   ", {0, 0, 0, 1, 0}},  //1.0 (格式: 00010 = 10÷10 = 1.0)
        {"P_S   ", {0, 0, 0, 5, 0}},  //80.0 (格式: 0080.0) 5.0
        {"D_S   ", {0, 0, 0, 0, 0}},  //800.0 (格式: 0800.0)

        {"BASESP", {0, 0, 8, 0, 0}},  // 基础速度 0.800 m/s
        {"STR_SP", {0, 1, 0, 0, 0}},  // 直道速度 1.000 m/s
        {"CURV_S", {0, 0, 6, 0, 0}}   // 弯道速度 0.600 m/s

    },
    // 第二页参数
    {
        {"MID   ", {0, 3, 6, 0, 7}},
        {"SHIFT ", {0, 0, 0, 1, 1}},
        {"ERROD ", {0, 0, 0, 1, 1}},

        {"Temp2 ", {2, 5, 1, 0, 0}},
        {"Humid2", {0, 7, 5, 0, 0}},
        {"Press2", {1, 2, 1, 3, 0}},

        {"Temp3 ", {2, 5, 0, 0, 1}},
        {"Humid3", {0, 6, 5, 2, 0}},
        {"Press3", {1, 0, 1, 5, 0}}

    },
    // 第三页参数
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
    if(target_prospective_row_count < 20 || target_prospective_row_count > 70)
    {
        target_prospective_row_count = 20;  // 强制设为默认值
        arr[0][0].digit[3] = target_prospective_row_count / 10;  // 十位
        arr[0][0].digit[4] = target_prospective_row_count % 10;  // 个位
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

    if(flash_check(0,10))
    {
        flash_read_page_to_buffer(0,10);
        memcpy(arr,flash_union_buffer,sizeof(arr));
        flash_buffer_clear();
        target_prospective_row_count = arr[0][0].digit[3] * 10 + arr[0][0].digit[4];
        // 范围检查
        if(target_prospective_row_count < 20) target_prospective_row_count = 20;
        if(target_prospective_row_count > 70) target_prospective_row_count = 70;

        exposure_time = arr[0][1].digit[2] * 100 + arr[0][1].digit[3] * 10 + arr[0][1].digit[4];
        if(exposure_time < 10) exposure_time = 10;
        if(exposure_time > 999) exposure_time = 999;

        // P_M 格式: XXXXX (如 00000 表示 0.0，00015 表示 1.5，00200 表示 20.0)
        P_M = (float)(arr[0][2].digit[0] * 10000 + arr[0][2].digit[1] * 1000 + 
                     arr[0][2].digit[2] * 100 + arr[0][2].digit[3] * 10 + arr[0][2].digit[4]) / 10.0f;
        
        // I_M 格式: XXXXX (修改为两位小数，如 00010 表示 0.10，00100 表示 1.00)
        I_M = (float)(arr[0][3].digit[0] * 10000 + arr[0][3].digit[1] * 1000 + 
                     arr[0][3].digit[2] * 100 + arr[0][3].digit[3] * 10 + arr[0][3].digit[4]) / 100.0f;  // 改为除以100

        // P_S 格式: XXX.X (如 080.0 表示 80.0)
        P_S = (float)(arr[0][4].digit[1] * 1000 + arr[0][4].digit[2] * 100 + 
                     arr[0][4].digit[3] * 10 + arr[0][4].digit[4]) / 10.0f;
        // D_S 格式: XXXX.X (如 0800.0 表示 800.0) - 使用全部5位
        D_S = (float)(arr[0][5].digit[0] * 10000 + arr[0][5].digit[1] * 1000 + arr[0][5].digit[2] * 100 +
                     arr[0][5].digit[3] * 10 + arr[0][5].digit[4]) / 10.0f;
        
        // 加载速度参数（格式：X.XXX m/s，如 0.800）
        float base_sp = (float)(arr[0][6].digit[1] * 1000 + arr[0][6].digit[2] * 100 +
                               arr[0][6].digit[3] * 10 + arr[0][6].digit[4]) / 1000.0f;
        float str_sp = (float)(arr[0][7].digit[1] * 1000 + arr[0][7].digit[2] * 100 +
                              arr[0][7].digit[3] * 10 + arr[0][7].digit[4]) / 1000.0f;
        float curv_sp = (float)(arr[0][8].digit[1] * 1000 + arr[0][8].digit[2] * 100 +
                               arr[0][8].digit[3] * 10 + arr[0][8].digit[4]) / 1000.0f;
        
        // 设置速度（自动转换为编码器速度）
        set_base_speed_real(base_sp);
        set_straight_speed_real(str_sp);
        set_cur_speed_real(curv_sp);
        
        STEER_MID=(arr[1][0].digit[1] * 1000 + arr[1][0].digit[2] * 100 +
                                    arr[1][0].digit[3] * 10 + arr[1][0].digit[4]);

        // SHIFT 比例与 ERROD 比例，格式：XXXXX → 除以10得到一位小数（如 00011 → 1.1）
        Shift_Ratio = (float)(arr[1][1].digit[0] * 10000 + arr[1][1].digit[1] * 1000 +
                               arr[1][1].digit[2] * 100 + arr[1][1].digit[3] * 10 + arr[1][1].digit[4]) / 10.0f;
        Err_diff    = (float)(arr[1][2].digit[0] * 10000 + arr[1][2].digit[1] * 1000 +
                               arr[1][2].digit[2] * 100 + arr[1][2].digit[3] * 10 + arr[1][2].digit[4]) / 10.0f;
    }

    else
     {
       save_to_flash();
     }
}

//按钮处理  P20_6(废弃), P20_7, P11_2, P11_3
void key_handle(void)
{
    // P20_6 按钮已废弃，不再处理
    static uint8 p207_prev_state = 1;  // 记录上一次按钮状态，1表示未按下
    uint8 p207_current_state = gpio_get_level(P20_7);
    
    // 按钮按下时（电平为0）
    if(p207_current_state == 0)
    {
        button_press_counter++;
        
        // 检测长按（计数器超过100，相当于约500ms）
        if(button_press_counter > 100)
        {
            // 长按逻辑 - 只执行一次
            if(button_press_counter == 101)  // 只在第101次执行
            {
                if(system_in_setting_mode)
                {
                    // 在下位机界面时，长按翻页
                    if(system_state==STATE_NORMAL || system_state==STATE_SELECT_PARAM)
                    {
                        current_page=(current_page+1)%3;
                        display_current_page();
                    }
                }
            }
        }
    }
    else  // 按钮释放时（电平为1）
    {
        // 检测按钮从按下到释放的边沿
        if(p207_prev_state == 0 && button_press_counter > 0 && button_press_counter <= 100)
        {
            // 短按逻辑 - 只在释放时执行一次
            if(!system_in_setting_mode)
            {
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
        
        // 按钮释放后重置计数器
        button_press_counter = 0;
    }
    
    // 保存当前按钮状态
    p207_prev_state = p207_current_state;

    if(gpio_get_level(P11_2)==0)
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

    if(gpio_get_level(P11_3)==0)
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
                            // 范围检查
                            if(target_prospective_row_count < 20) target_prospective_row_count = 20;
                            if(target_prospective_row_count > 70) target_prospective_row_count = 70;
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

                        case 2: // P_M 电机P参数 格式: XXXXX (如 00010 表示 1.0)
                            // 从存储的数值转换为实际的P_M参数（支持小数点后1位）
                            P_M = (float)(arr[0][2].digit[0] * 10000 + arr[0][2].digit[1] * 1000 + 
                                         arr[0][2].digit[2] * 100 + arr[0][2].digit[3] * 10 + arr[0][2].digit[4]) / 10.0f;
                            left_motor.pi.kp = P_M;   // 更新左电机PI控制器参数
                            right_motor.pi.kp = P_M;  // 更新右电机PI控制器参数
                            break;

                            // 在key_handle函数的编辑部分，找到I_M的处理代码，修改为：
                            case 3: // I_M 电机I参数 格式: XXXXX (修改为两位小数，如 00010 表示 0.10)
                                // 从存储的数值转换为实际的I_M参数（支持小数点后2位）
                                I_M = (float)(arr[0][3].digit[0] * 10000 + arr[0][3].digit[1] * 1000 +
                                             arr[0][3].digit[2] * 100 + arr[0][3].digit[3] * 10 + arr[0][3].digit[4]) / 100.0f;  // 改为除以100
                                left_motor.pi.ki = I_M;   // 更新左电机PI控制器参数
                                right_motor.pi.ki = I_M;  // 更新右电机PI控制器参数
                                break;

                        case 4: // P_S 方向控制P参数 格式: XXX.X (如 080.0)
                            // 从存储的数值转换为实际的P_S参数（支持小数点后1位）
                            P_S = (float)(arr[0][4].digit[1] * 1000 + arr[0][4].digit[2] * 100 + 
                                         arr[0][4].digit[3] * 10 + arr[0][4].digit[4]) / 10.0f;
                            car_steer.pd.kp = P_S;  // 更新PD控制器参数
                            break;

                        case 5: // D_S 方向控制D参数 格式: XXXX.X (如 0800.0)
                            // 从存储的数值转换为实际的D_S参数（支持小数点后1位）
                            D_S = (float)(arr[0][5].digit[0] * 10000 + arr[0][5].digit[1] * 1000 + arr[0][5].digit[2] * 100 +
                                         arr[0][5].digit[3] * 10 + arr[0][5].digit[4]) / 10.0f;
                            car_steer.pd.kd = D_S;  // 更新PD控制器参数
                            break;

                        case 6: // BASESP 基础速度 (格式：0.XXX m/s)
                            {
                                float base_sp = (float)(arr[0][6].digit[1] * 1000 + arr[0][6].digit[2] * 100 +
                                                       arr[0][6].digit[3] * 10 + arr[0][6].digit[4]) / 1000.0f;
                                set_base_speed_real(base_sp);
                            }
                            break;

                        case 7: // STR_SP 直道速度 (格式：0.XXX m/s)
                            {
                                float str_sp = (float)(arr[0][7].digit[1] * 1000 + arr[0][7].digit[2] * 100 +
                                                      arr[0][7].digit[3] * 10 + arr[0][7].digit[4]) / 1000.0f;
                                set_straight_speed_real(str_sp);
                            }
                            break;

                        case 8: // CURV_S 弯道速度 (格式：0.XXX m/s)
                            {
                                float curv_sp = (float)(arr[0][8].digit[1] * 1000 + arr[0][8].digit[2] * 100 +
                                                       arr[0][8].digit[3] * 10 + arr[0][8].digit[4]) / 1000.0f;
                                set_cur_speed_real(curv_sp);
                            }
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
                        case 1: // SHIFT 比例（XXXXX → /10.0）
                            Shift_Ratio = (float)(arr[1][1].digit[0] * 10000 + arr[1][1].digit[1] * 1000 +
                                                  arr[1][1].digit[2] * 100 + arr[1][1].digit[3] * 10 + arr[1][1].digit[4]) / 10.0f;
                            break;
                        case 2: // ERROD 比例（XXXXX → /10.0）
                            Err_diff = (float)(arr[1][2].digit[0] * 10000 + arr[1][2].digit[1] * 1000 +
                                               arr[1][2].digit[2] * 100 + arr[1][2].digit[3] * 10 + arr[1][2].digit[4]) / 10.0f;
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
    // P20_6 按钮已废弃，不再处理
    
    if(gpio_get_level(P20_7)==0)
    {
        button_press_counter++;
        
        // 检测长按（计数器超过100，相当于约500ms）
        if(button_press_counter > 100)
        {
            // 长按逻辑
            if(system_in_setting_mode)
            {
                // 在下位机界面时，长按翻页
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
            button_press_counter = 0;  // 重置计数器
        }
    }
    else
    {
        // 鎸夐挳閲婃斁鏃跺鐞嗙煭鎸夐�昏緫
        if(button_press_counter > 0 && button_press_counter <= 100)
        {
            // 鐭寜閫昏緫
            if(!system_in_setting_mode)
            {
                // 鍦ㄦ憚鍍忓ご鐣岄潰鏃讹紝鐭寜鍒囨崲鍒颁笅浣嶆満椤甸潰
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
                // 鍦ㄤ笅浣嶆満鐣岄潰鏃讹紝鐭寜淇敼鍙傛暟
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

                        case 5: // D_S 锟斤拷锟轿拷锟较碉拷锟� (1000)
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
