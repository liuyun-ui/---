#include "zf_common_headfile.h"
#include "Control.h"
#include "zf_driver_pwm.h"

//A向GND
#define MOTORL_A ATOM0_CH4_P02_4    // 左电机A相ATOM0_CH4_P02_4
#define MOTORL_B ATOM0_CH5_P02_5    // 左电机B相ATOM0_CH5_P02_5

#define MOTORR_A ATOM0_CH0_P21_2    // 右电机A相ATOM0_CH1_P21_3
#define MOTORR_B ATOM0_CH1_P21_3    // 右电机B相ATOM0_CH0_P21_2
#define STEER_PIN ATOM0_CH1_P33_9
uint8 Go=1;   //常规循迹

motor_control_t left_motor, right_motor;
steer_control_t car_steer;

int Steer_Angle=0;
int STEER_MID=3750;
int STEER_LEFT_MAX=2600;
int STEER_RIGHT_MAX=4900;

float P_M=20;
float I_M=0.5;
float P_S=80.0;
float D_S=800;

int base_speed=800;
int straight_speed=1000;
int cur_speed=600;
int ur_cur_speed=400;
float Shift_Ratio=1.5;   //变速系数
float Err_diff=1.1;      //差速系数
int Speed_Left_Set=0;
int Speed_Right_Set=0;

static uint8_t speed_control_phase = 0;
// 增量式PI控制器
int32_t speed_pi_calculate(pi_controller_t *pi, int32_t error)
{
    // 计算比例项增量：当前误差 - 上次误差
    int32_t delta_p = error - pi->prev_error;
    // 计算积分项：当前误差
    int32_t delta_i = error;
    int32_t delta_output = (int32_t)(pi->kp * delta_p + pi->ki * delta_i);
    int32_t output = pi->prev_output + delta_output;
    // 输出限幅
    if(output > 30000) output = 30000;
    if(output < -30000) output = -30000;
    // 保存状态
    pi->prev_error = error;
    pi->prev_output = output;
    return output;
}

int32_t pd_controller_calculate(pd_controller_t *pd, float current_error)
{
    float derivative;
    int32_t output;
    // 计算微分项
    derivative = current_error - pd->prev_error;
    // PD控制计算：输出 = kp * 误差 + kd * 微分
    output = (int32_t)(pd->kp * current_error + pd->kd * derivative);
    // 更新状态
    pd->prev_error = current_error;
    pd->prev_output = output;
    return output;
}

void control_init(void)
{
    // 左编码器 - TIM2，计数引脚P33_7，方向引脚P33_6
    encoder_dir_init(TIM2_ENCODER, TIM2_ENCODER_CH1_P33_7, TIM2_ENCODER_CH2_P33_6);

    // 右编码器 - TIM4，计数引脚P02_8，方向引脚P00_9
    encoder_dir_init(TIM4_ENCODER, TIM4_ENCODER_CH1_P02_8, TIM4_ENCODER_CH2_P00_9);

    pwm_init(MOTORL_A, 50, HALF_MAXDUTY);  // 初始50%占空比
    pwm_init(MOTORL_B,50, HALF_MAXDUTY);
    pwm_init(MOTORR_A,50, HALF_MAXDUTY);
    pwm_init(MOTORR_B,50, HALF_MAXDUTY);
    pwm_init(STEER_PIN,50,STEER_MID);

    left_motor.pi.kp = P_M;      // 比例系数
    left_motor.pi.ki = I_M;      // 积分系数
    left_motor.pi.integral = 0;
    left_motor.pi.prev_error = 0;
    left_motor.pi.prev_output = 0;
    left_motor.pi.integral_limit = 2000;
    left_motor.target_speed = 0;
    left_motor.current_speed = 0;
    left_motor.output = 0;

    right_motor.pi.kp = P_M;
    right_motor.pi.ki = I_M;
    right_motor.pi.integral = 0;
    right_motor.pi.prev_error = 0;
    right_motor.pi.prev_output = 0;
    right_motor.pi.integral_limit = 2000;
    right_motor.target_speed = 0;
    right_motor.current_speed = 0;
    right_motor.output = 0;


    car_steer.pd.kp = P_S;
    car_steer.pd.kd = D_S;
    car_steer.pd.prev_error = 0.0f;
    car_steer.pd.prev_output = 0;
    car_steer.current_angle = 0;
    car_steer.output = 0;
    car_steer.angle_limit = 1100;

    // 初始化2ms定时器中断
    pit_init(CCU60_CH0, 5000);  // 2000us = 2ms
    pit_enable(CCU60_CH0);      // 使能中断

     pit_init(CCU61_CH0, 6000);  // 2000us = 2ms
     pit_enable(CCU61_CH0);      // 使能中断

}

void MotorOutput(int32_t l_duty, int32_t r_duty)
{
    // 限制输出范围
    if(l_duty > PWM_DUTY_MAX) l_duty = PWM_DUTY_MAX;
    if(l_duty < -PWM_DUTY_MAX) l_duty = -PWM_DUTY_MAX;
    if(r_duty > PWM_DUTY_MAX) r_duty = PWM_DUTY_MAX;
    if(r_duty < -PWM_DUTY_MAX) r_duty = -PWM_DUTY_MAX;

    // 计算半占空比
    int32_t l_half_duty = l_duty / 2;
    int32_t r_half_duty = r_duty / 2;

    // 左电机输出
    pwm_set_duty(MOTORL_A, HALF_MAXDUTY - l_half_duty);
    pwm_set_duty(MOTORL_B, HALF_MAXDUTY + l_half_duty);

    // 右电机输出
    pwm_set_duty(MOTORR_A, HALF_MAXDUTY - r_half_duty);
    pwm_set_duty(MOTORR_B, HALF_MAXDUTY + r_half_duty);

}

void calculate_motor_speed(void)
{
    static int32_t left_encoder_prev = 0;
    static int32_t right_encoder_prev = 0;

    // 读取编码器值
    int32_t left_encoder_now = -1 * encoder_get_count(TIM2_ENCODER);  // 左编码器取反
    int32_t right_encoder_now = encoder_get_count(TIM4_ENCODER);      // 右编码器

    // 计算速度（2ms内的脉冲数）
    left_motor.current_speed = left_encoder_now - left_encoder_prev;
    right_motor.current_speed = right_encoder_now - right_encoder_prev;

    // 更新上一次的编码器值
    left_encoder_prev = left_encoder_now;
    right_encoder_prev = right_encoder_now;
}

void calculate_target_speeds(void)
{
    if(Go==1)
    {
        Speed_Left_Set = base_speed;
        Speed_Right_Set = base_speed;
        if(straight_flag==1)
        {
            Speed_Left_Set = straight_speed;
            Speed_Right_Set = straight_speed;
        }
        Speed_Left_Set = Speed_Left_Set - white_start*Shift_Ratio;
        Speed_Right_Set = Speed_Right_Set - white_start*Shift_Ratio;
        Speed_Left_Set = Speed_Left_Set - pros_weighted_deviation*Err_diff;
        Speed_Right_Set = Speed_Right_Set + pros_weighted_deviation*Err_diff;
    }
}

void motor_speed_control(void)
{
    calculate_motor_speed();

    left_motor.target_speed=Speed_Left_Set;
    right_motor.target_speed=Speed_Right_Set;

    // 左电机PI控制
    int32_t left_error = left_motor.target_speed - left_motor.current_speed;
    left_motor.output = speed_pi_calculate(&left_motor.pi, left_error);
    // 右电机PI控制
    int32_t right_error = right_motor.target_speed - right_motor.current_speed;
    right_motor.output = speed_pi_calculate(&right_motor.pi, right_error);

    if(left_motor.output > 30000) left_motor.output = 30000;
    if(left_motor.output < -30000) left_motor.output = -30000;
    if(right_motor.output > 30000) right_motor.output = 30000;
    if(right_motor.output < -30000) right_motor.output = -30000;

    MotorOutput(left_motor.output, right_motor.output);

}
/*
void motor_speed_control(void)
{
    switch(speed_control_phase)
    {
        case 0:
            // 阶段0：计算左电机
            calculate_motor_speed();
            left_motor.target_speed = Speed_Left_Set;
            int32_t left_error = left_motor.target_speed - left_motor.current_speed;
            left_motor.output = speed_pi_calculate(&left_motor.pi, left_error);
            break;

        case 1:
            // 阶段1：计算右电机
            right_motor.target_speed = Speed_Right_Set;
            int32_t right_error = right_motor.target_speed - right_motor.current_speed;
            right_motor.output = speed_pi_calculate(&right_motor.pi, right_error);
            break;

        case 2:
            // 阶段2：输出限幅和电机输出
            if(left_motor.output > 30000) left_motor.output = 30000;
            if(left_motor.output < -30000) left_motor.output = -30000;
            if(right_motor.output > 30000) right_motor.output = 30000;
            if(right_motor.output < -30000) right_motor.output = -30000;
            MotorOutput(left_motor.output, right_motor.output);
            break;
    }

    // 切换到下一阶段
    speed_control_phase = (speed_control_phase + 1) % 3;

}
*/
//ERROR_SCALE
void Direction_control(void)
{
    if(Go == 1)  // 常规循迹模式
    {
        float current_error = 0-pros_weighted_deviation;
        car_steer.output = pd_controller_calculate(&car_steer.pd, current_error);
        // 角度限幅
        if(car_steer.output > car_steer.angle_limit)
            car_steer.output = car_steer.angle_limit;
        else if(car_steer.output < -car_steer.angle_limit)
            car_steer.output = -car_steer.angle_limit;
        car_steer.current_angle = car_steer.output;
        Steer(car_steer.current_angle);

    }
}

void Steer(int angle)
{
    int pwm_value;
    pwm_value = STEER_MID + angle;
    // 最终限幅确保在有效范围内
    if(pwm_value > STEER_RIGHT_MAX) pwm_value = STEER_RIGHT_MAX;
    if(pwm_value < STEER_LEFT_MAX) pwm_value = STEER_LEFT_MAX;
    pwm_set_duty(STEER_PIN, pwm_value);
}




















