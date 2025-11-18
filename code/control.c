#include "zf_common_headfile.h"
#include "Control.h"
#include "zf_driver_pwm.h"

//A锟斤拷GND
#define MOTORL_A ATOM0_CH4_P02_4    // 锟斤拷锟斤拷A锟斤拷ATOM0_CH4_P02_4
#define MOTORL_B ATOM0_CH5_P02_5    // 锟斤拷锟斤拷B锟斤拷ATOM0_CH5_P02_5

#define MOTORR_A ATOM0_CH0_P21_2    // 锟揭碉拷锟紸锟斤拷ATOM0_CH1_P21_3
#define MOTORR_B ATOM0_CH1_P21_3    // 锟揭碉拷锟紹锟斤拷ATOM0_CH0_P21_2
#define STEER_PIN ATOM1_CH1_P33_9
uint8 Go=1;

motor_control_t left_motor, right_motor;
steer_control_t car_steer;

int Steer_Angle=0;
int STEER_MID=3607;
int STEER_LEFT_MAX=2707;
int STEER_RIGHT_MAX=4507;
float P_M=0.0;      // 增量式PI的P：阻尼项，先设为0
float I_M=5.0;      // 增量式PI的I：主驱动力（相当于位置式的P）
float P_S=80.0;
float D_S=800;


int base_speed = 18;
int straight_speed = 69;
int cur_speed = 42;
int ur_cur_speed = 28;
float Shift_Ratio=1.1;
float Err_diff=1.1;
int Speed_Left_Set=0;
int Speed_Right_Set=0;


int32_t speed_pi_calculate(pi_controller_t *pi, int32_t error)
{
    // P项：误差的变化量
    int32_t delta_p = error - pi->prev_error;
    // I项：当前误差
    int32_t delta_i = error;

    // 计算增量输出
    int32_t delta_output = (int32_t)(pi->kp * delta_p + pi->ki * delta_i);

    // 累加到上一次的输出
    int32_t output = pi->prev_output + delta_output;

    // 输出限幅，防止积分饱和
    if(output > 11500) output = 11500;
    if(output < -11500) output = -11500;

    // 保存状态（保存限幅后的值，防止积分饱和）
    pi->prev_error = error;
    pi->prev_output = output;

    return output;
}

int32_t pd_controller_calculate(pd_controller_t *pd, float current_error)
{

    int32_t output;

    output = (int32_t)(pd->kp * current_error + pd->kd * (current_error - pd->prev_error));

    pd->prev_error = current_error;
    pd->prev_output = output;

    return output;
}

void control_init(void)
{
    // 锟斤拷锟斤拷锟斤拷锟� - TIM2锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷P33_7锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷P33_6
    encoder_dir_init(TIM5_ENCODER, TIM5_ENCODER_CH1_P10_3, TIM5_ENCODER_CH2_P10_1);

    // 锟揭憋拷锟斤拷锟斤拷 - TIM4锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷P02_8锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷P00_9
    encoder_dir_init(TIM6_ENCODER,TIM6_ENCODER_CH1_P20_3, TIM6_ENCODER_CH2_P20_0);
    pwm_init(MOTORL_A, 17000, 500);  // 电机PWM频率17kHz
    pwm_init(MOTORL_B, 17000, 500);
    pwm_init(MOTORR_A, 17000, 500);
    pwm_init(MOTORR_B, 17000,500);
    pwm_init(STEER_PIN, 50, STEER_MID);      // 舵机保持50Hz

    left_motor.pi.kp = P_M;      // 锟斤拷锟斤拷系锟斤拷
    left_motor.pi.ki = I_M;      // 锟斤拷锟斤拷系锟斤拷
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
    car_steer.angle_limit = 500;

    pit_init(CCU60_CH0, 500);
    pit_enable(CCU60_CH0);

     pit_init(CCU61_CH0, 2000);
     pit_enable(CCU61_CH0);
}

void MotorOutput(int32_t l_duty, int32_t r_duty)
{
    // 锟斤拷锟斤拷锟斤拷锟斤拷锟轿�
    if(l_duty > PWM_DUTY_MAX) l_duty = PWM_DUTY_MAX;
    if(l_duty < -PWM_DUTY_MAX) l_duty = -PWM_DUTY_MAX;
    if(r_duty > PWM_DUTY_MAX) r_duty = PWM_DUTY_MAX;
    if(r_duty < -PWM_DUTY_MAX) r_duty = -PWM_DUTY_MAX;

    // 锟斤拷锟斤拷锟秸硷拷毡锟�
    int32_t l_half_duty = l_duty / 2;
    int32_t r_half_duty = r_duty / 2;

    // 锟斤拷锟斤拷锟斤拷锟�
    pwm_set_duty(MOTORL_A, HALF_MAXDUTY - l_half_duty);
    pwm_set_duty(MOTORL_B, HALF_MAXDUTY + l_half_duty);

    // 锟揭碉拷锟斤拷锟斤拷
    pwm_set_duty(MOTORR_A, HALF_MAXDUTY - r_half_duty);
    pwm_set_duty(MOTORR_B, HALF_MAXDUTY + r_half_duty);

}

void calculate_motor_speed(void)
{
    // 直接读取编码器计数值（这就是6ms内的脉冲增量）
    left_motor.current_speed = encoder_get_count(TIM5_ENCODER);   // 左电机编码器
    right_motor.current_speed = -encoder_get_count(TIM6_ENCODER); // 右电机编码器（取反，因为镜像安装）
    
    // 读取后立即清零编码器，为下一个周期做准备
    encoder_clear_count(TIM5_ENCODER);
    encoder_clear_count(TIM6_ENCODER);
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

        /*
        Speed_Left_Set = Speed_Left_Set - white_start*Shift_Ratio;
        Speed_Right_Set = Speed_Right_Set - white_start*Shift_Ratio;
        */
/*
        Speed_Left_Set = Speed_Left_Set + pros_weighted_deviation*Err_diff;
        Speed_Right_Set = Speed_Right_Set - pros_weighted_deviation*Err_diff;
*/
    }
}

void motor_speed_control(void)
{
    calculate_motor_speed();

    left_motor.target_speed = Speed_Left_Set;
    right_motor.target_speed = Speed_Right_Set;

    // 左电机PI控制
    int32_t left_error = left_motor.target_speed - left_motor.current_speed;
    left_motor.output = speed_pi_calculate(&left_motor.pi, left_error);
    
    // 右电机PI控制
    int32_t right_error = right_motor.target_speed - right_motor.current_speed;
    right_motor.output = speed_pi_calculate(&right_motor.pi, right_error);

    // PI函数内部已经限幅了，这里不需要再限幅
    
    MotorOutput(left_motor.output, right_motor.output);
}

void Direction_control(void)
{
    if(Go == 1)
    {
        float current_error = 0-pros_weighted_deviation;
        float base_kp = P_S;
        if(straight_flag)
        {
            base_kp = P_S - 4.0f;
            if(base_kp < 0.0f)
            {
                base_kp = 0.0f;
            }
        }

        car_steer.pd.kp = base_kp;
        float original_kd = car_steer.pd.kd;
        if(straight_flag)
        {
            car_steer.pd.kd = original_kd - 20.0f;
            if(car_steer.pd.kd < 0.0f)
            {
                car_steer.pd.kd = 0.0f;
            }
        }

        car_steer.output = pd_controller_calculate(&car_steer.pd, current_error);
        if(car_steer.output > car_steer.angle_limit)
            car_steer.output = car_steer.angle_limit;
        else if(car_steer.output < -car_steer.angle_limit)
            car_steer.output = -car_steer.angle_limit;
        car_steer.pd.kd = original_kd;
        car_steer.current_angle = car_steer.output;
        Steer(car_steer.current_angle);
    }
}


void Steer(int angle)
{
    int pwm_value;
    pwm_value = STEER_MID + angle;
    if(pwm_value > STEER_RIGHT_MAX) pwm_value = STEER_RIGHT_MAX;
    if(pwm_value < STEER_LEFT_MAX) pwm_value = STEER_LEFT_MAX;
    pwm_set_duty(STEER_PIN, pwm_value);
}

// ==================== 速度设置函数（使用实际速度m/s） ====================

// 设置基础速度（m/s）
void set_base_speed_real(float speed_m_s)
{
    base_speed = REAL_SPEED_TO_ENCODER(speed_m_s);
}

// 设置直道速度（m/s）
void set_straight_speed_real(float speed_m_s)
{
    straight_speed = REAL_SPEED_TO_ENCODER(speed_m_s);
}

// 设置弯道速度（m/s）
void set_cur_speed_real(float speed_m_s)
{
    cur_speed = REAL_SPEED_TO_ENCODER(speed_m_s);
}

// 设置急弯速度（m/s）
void set_ur_cur_speed_real(float speed_m_s)
{
    ur_cur_speed = REAL_SPEED_TO_ENCODER(speed_m_s);
}

// ==================== 速度获取函数（返回实际速度m/s） ====================

// 获取基础速度（m/s）
float get_base_speed_real(void)
{
    return ENCODER_TO_REAL_SPEED(base_speed);
}

// 获取左电机当前实际速度（m/s）
float get_current_real_speed_left(void)
{
    return ENCODER_TO_REAL_SPEED(left_motor.current_speed);
}

// 获取右电机当前实际速度（m/s）
float get_current_real_speed_right(void)
{
    return ENCODER_TO_REAL_SPEED(right_motor.current_speed);
}




















