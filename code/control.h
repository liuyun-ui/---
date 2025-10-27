#ifndef CONTROL_H
#define CONTROL_H

#include "zf_common_headfile.h"
#include "zf_driver_pwm.h"

#define MOTORL_A ATOM0_CH4_P02_4    // 左电机A相ATOM0_CH4_P02_4
#define MOTORL_B ATOM0_CH5_P02_5    // 左电机B相ATOM0_CH5_P02_5
#define MOTORR_A ATOM0_CH0_P21_2    // 右电机A相ATOM0_CH1_P21_3
#define MOTORR_B ATOM0_CH1_P21_3    // 右电机B相ATOM0_CH0_P21_2
#define STEER_PIN ATOM0_CH1_P33_9

typedef struct
{
    float kp;           // 比例系数
    float ki;           // 积分系数
    int32_t integral;   // 积分项
    int32_t prev_error; // 上次误差
    int32_t prev_output;// 上次输出（增量式用）
    int32_t integral_limit; // 积分限幅
} pi_controller_t;

typedef struct {
    pi_controller_t pi;     // PI控制器
    int32_t target_speed;   // 目标速度（编码器脉冲数/周期）
    int32_t current_speed;  // 当前速度（编码器脉冲数/周期）
    int32_t output;         // PI输出（PWM占空比）
} motor_control_t;

typedef struct {
    float kp;           // 比例系数
    float kd;           // 微分系数
    float prev_error;   // 上次误差
    int32_t prev_output;// 上次输出
} pd_controller_t;

// 方向控制结构体
typedef struct {
    pd_controller_t pd;     // PD控制器
    int32_t current_angle;  // 当前舵机角度
    int32_t output;         // PD输出（PWM占空比偏移量）
    int32_t angle_limit;    // 角度限幅
} steer_control_t;

extern steer_control_t car_steer;
extern motor_control_t left_motor;
extern motor_control_t right_motor;

extern uint8 Go;

extern int STEER_MID;
extern int STEER_LEFT_MAX;
extern int STEER_RIGHT_MAX;

extern float P_M;
extern float I_M;
extern float P_S;
extern float D_S;

extern int base_speed;
extern int straight_speed;
extern int cur_speed;
extern int ur_cur_speed;
extern int Speed_Left_Set, Speed_Right_Set;
extern float Shift_Ratio;   //变速系数
extern float Err_diff;      //差速系数

int32_t speed_pi_calculate(pi_controller_t *pi, int32_t error);
int32_t pd_controller_calculate(pd_controller_t *pd, float current_error);
void control_init(void);
void MotorOutput(int32_t l_duty, int32_t r_duty);
void calculate_motor_speed(void);
void motor_speed_control(void);
void Direction_control(void);
void Steer(int angle);
void calculate_target_speeds(void);
#endif  // CONTROL_H


