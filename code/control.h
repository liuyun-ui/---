#ifndef CONTROL_H
#define CONTROL_H

#include "zf_common_headfile.h"
#include "zf_driver_pwm.h"

#define ENCODER_PULSES_PER_METER 11498
#define CONTROL_PERIOD_MS 2


#define REAL_SPEED_TO_ENCODER(speed_m_s) \
    ((int32_t)((speed_m_s) * ENCODER_PULSES_PER_METER * CONTROL_PERIOD_MS / 1000.0f))

// 缂栫爜鍣ㄩ�熷害(鑴夊啿/6ms) 杞崲涓� 瀹為檯閫熷害(m/s)
#define ENCODER_TO_REAL_SPEED(encoder_speed) \
    ((float)(encoder_speed) * 1000.0f / (ENCODER_PULSES_PER_METER * CONTROL_PERIOD_MS))

#define MOTORL_A ATOM0_CH4_P02_4    // 锟斤拷锟斤拷A锟斤拷ATOM0_CH4_P02_4
#define MOTORL_B ATOM0_CH5_P02_5    // 锟斤拷锟斤拷B锟斤拷ATOM0_CH5_P02_5
#define MOTORR_A ATOM0_CH0_P21_2    // 锟揭碉拷锟紸锟斤拷ATOM0_CH1_P21_3
#define MOTORR_B ATOM0_CH1_P21_3    // 锟揭碉拷锟紹锟斤拷ATOM0_CH0_P21_2
#define STEER_PIN ATOM0_CH1_P33_9

typedef struct
{
    float kp;           // 锟斤拷锟斤拷系锟斤拷
    float ki;           // 锟斤拷锟斤拷系锟斤拷
    int32_t integral;   // 锟斤拷锟斤拷锟斤拷
    int32_t prev_error; // 锟较达拷锟斤拷锟�
    int32_t prev_output;// 锟较达拷锟斤拷锟斤拷锟斤拷锟斤拷锟绞斤拷茫锟�
    int32_t integral_limit; // 锟斤拷锟斤拷锟睫凤拷
} pi_controller_t;

typedef struct {
    pi_controller_t pi;     // PI锟斤拷锟斤拷锟斤拷
    int32_t target_speed;   // 目锟斤拷锟劫度ｏ拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷/锟斤拷锟节ｏ拷
    int32_t current_speed;  // 锟斤拷前锟劫度ｏ拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷/锟斤拷锟节ｏ拷
    int32_t output;         // PI锟斤拷锟斤拷锟絇WM占锟秸比ｏ拷
} motor_control_t;

typedef struct {
    float kp;           // 锟斤拷锟斤拷系锟斤拷
    float kd;           // 微锟斤拷系锟斤拷
    float prev_error;   // 锟较达拷锟斤拷锟�
    int32_t prev_output;// 锟较达拷锟斤拷锟�
} pd_controller_t;

// 锟斤拷锟斤拷锟斤拷平峁癸拷锟�
typedef struct {
    pd_controller_t pd;     // PD锟斤拷锟斤拷锟斤拷
    int32_t current_angle;  // 锟斤拷前锟斤拷锟斤拷嵌锟�
    int32_t output;         // PD锟斤拷锟斤拷锟絇WM占锟秸憋拷偏锟斤拷锟斤拷锟斤拷
    int32_t angle_limit;    // 锟角讹拷锟睫凤拷
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
extern float Shift_Ratio;
extern float Err_diff;

int32_t speed_pi_calculate(pi_controller_t *pi, int32_t error);
int32_t pd_controller_calculate(pd_controller_t *pd, float current_error);
void control_init(void);
void MotorOutput(int32_t l_duty, int32_t r_duty);
void calculate_motor_speed(void);
void motor_speed_control(void);
void Direction_control(void);
void Steer(int angle);
void calculate_target_speeds(void);

// 閫熷害璁剧疆鍑芥暟锛堜娇鐢ㄥ疄闄呴�熷害m/s锛�
void set_base_speed_real(float speed_m_s);
void set_straight_speed_real(float speed_m_s);
void set_cur_speed_real(float speed_m_s);
void set_ur_cur_speed_real(float speed_m_s);

// 閫熷害鑾峰彇鍑芥暟锛堣繑鍥炲疄闄呴�熷害m/s锛�
float get_base_speed_real(void);
float get_current_real_speed_left(void);
float get_current_real_speed_right(void);

#endif  // CONTROL_H
