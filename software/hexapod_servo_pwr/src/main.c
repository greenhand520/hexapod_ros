#include "stc8g.h"

//延迟一个机器周期
#define __nop __asm nop __endasm    

// P5.4 按键输入
#define KEY_PIN P54  
// P5.5 DCDC使能
#define CE_PIN P55   
// P3.3 DCDC方向
#define DIR_PIN P33  
// P3.2 USB检测
#define USB_PIN P32  
// P3.0 DCDC2控制
#define DCDC2_PIN P30 

__bit usb_plugged = 0;
__bit dcdc_enabled = 0;
__bit key_pressed = 0;
__bit usb_state_changed = 0; 
unsigned int key_press_time = 0;

void GPIO_Init(void);
void Enter_Sleep(void);
void WakeUp_Handler(void);
void DCDC_Enable(__bit enable);
void DCDC_Set_Direction(__bit direction);
void DCDC2_Enable(__bit enable);
void Delay_ms(unsigned int ms);
void Handle_USB_Plug(void);
void Handle_USB_Unplug(void);

void main(void) {
    GPIO_Init();
    
    // 关闭DCDC
    DCDC_Enable(0);  
    // 默认设置为输出方向
    DCDC_Set_Direction(1); 
    // 关闭DCDC2
    DCDC2_Enable(0); 
    
    // 读取初始USB状态
    usb_plugged = (USB_PIN == 0);
    
    while(1) {
        // 检查USB状态变化标志
        if(usb_state_changed) {
            usb_state_changed = 0; 
            if(usb_plugged) {
                Handle_USB_Plug();
            } else {
                Handle_USB_Unplug();
            }
        }
        
        // 按键处理，按下
        if(KEY_PIN) {
            if(!key_pressed) {
                key_pressed = 1;
                key_press_time = 0;
                WakeUp_Handler(); 
            } else {
                key_press_time++;
                // 长按约1秒
                if(key_press_time > 100) {   
                    if (dcdc_enabled) {
                        // dcdc开启且无usb插入时，长按关机
                        if (!usb_plugged) {
                            DCDC_Enable(0);
                            DCDC2_Enable(0);
                        }
                    } else {
                        // 长按使能DCDC
                        if(!usb_plugged) {
                            // 无USB插入，设置为输出模式
                            DCDC_Set_Direction(1);
                            Delay_ms(50);
                            DCDC_Enable(1);
                        } else {
                            // USB插入，设置为充电模式（但通常USB插入时已经自动处理）
                            // todo：添加额外逻辑
                        }
                    }
                    key_press_time = 0;
                }
            }
        } else {
            // 按键释放
            key_pressed = 0;
            key_press_time = 0;
        }
        
        // 延时
        Delay_ms(10);
        
        // 如果没有活动，进入睡眠
        if(!key_pressed && !usb_plugged && !dcdc_enabled) {
            Enter_Sleep();
        }
    }
}

void GPIO_Init(void) {
    // P5.4 按键输入（高阻输入模式，无内部上拉）
    P5M0 &= ~0x10; 
    // 高阻输入模式
    P5M1 |= 0x10;   
    
    // P5.5 DCDC_CE（推挽输出，默认高电平）
    P5M0 |= 0x20;
    P5M1 &= ~0x20;
    // 默认关闭DCDC
    CE_PIN = 1;
    
    // P3.3 DCDC_DIR（推挽输出）
    P3M0 |= 0x08;
    P3M1 &= ~0x08;
    // 默认输出方向
    DIR_PIN = 1; 
    
    // P3.2 USB检测（设置为高阻输入，用于外部中断）
    P3M0 &= ~0x04;
    // 高阻输入模式
    P3M1 |= 0x04;  
    
    // P3.0 DCDC2控制（开漏输出）
    P3M0 |= 0x01;
    P3M1 |= 0x01;
    // 默认关闭DCDC2
    DCDC2_PIN = 0; 
    
    // 配置P3.2（USB检测）端口中断
    P3INTE |= 0x04;    
    // 使能P3.2端口中断
    // 设置为电平变化触发
    P3IM0 |= 0x04;     
    // 设置为电平变化触发
    P3IM1 |= 0x04;     
    // 清除中断标志
    P3INTF = 0x00;
    
    EA = 1;        // 全局中断使能
}

// P3端口中断服务函数 - USB检测
void P3_ISR(void) __interrupt (8) {
    // 检查是否是P3.2触发的中断
    if(P3INTF & 0x04) {
        Delay_ms(5); 
        usb_plugged = (USB_PIN == 0);  
        usb_state_changed = 1;
        // 清除P3.2中断标志
        P3INTF &= ~0x04;  
        WakeUp_Handler();
    }
}

// 进入睡眠模式
void Enter_Sleep(void) {
    // USB检测中断
    P3INTE |= 0x04;  
    // 全局中断使能
    EA = 1;     
    
    // 进入睡眠模式
    PCON |= 0x02; 
    __nop;
    __nop;
}

// 唤醒处理
void WakeUp_Handler(void) {
    // todo：添加一些唤醒后的特殊处理
}

void DCDC_Enable(__bit enable) {
    if(enable) {
        // 低电平使能
        CE_PIN = 0; 
        dcdc_enabled = 1;
    } else {
        // 高电平关闭
        CE_PIN = 1; 
        dcdc_enabled = 0;
    }
}

// 设置DCDC方向
void DCDC_Set_Direction(__bit direction) {
    // 1=输出，0=充电
    DIR_PIN = direction; 
}

// 使能DCDC2
void DCDC2_Enable(__bit enable) {
    // 开漏输出，高电平使能
    DCDC2_PIN = enable; 
}

// USB插入处理
void Handle_USB_Plug(void) {
    if(dcdc_enabled) {
        // 如果DCDC正在输出，先关闭
        DCDC_Enable(0);
        Delay_ms(100);
    }
    
    // 设置方向为充电
    DCDC_Set_Direction(0);
    Delay_ms(50);
    DCDC_Enable(1);
    DCDC2_Enable(1);
}

// USB拔出处理
void Handle_USB_Unplug(void) {
    if(dcdc_enabled) {
        DCDC_Enable(0);
        Delay_ms(100);
        // 设置方向为输出
        DCDC_Set_Direction(1);
        Delay_ms(50);
        DCDC_Enable(1);
    }
    DCDC2_Enable(1);
}

// 简单延时函数，延迟1ms
void Delay_ms(unsigned int ms) {
    // TMOD &= 0xF0;    // 清除T0配置
    // TMOD |= 0x01;    // T0为16位定时器模式
    // for(; ms>0; ms--)
    // {
    //     TH0 = (65536 - 921) / 256;  // 11.0592MHz，1ms初值
    //     TL0 = (65536 - 921) % 256;
    //     TR0 = 1;      // 启动定时器
    //     while(!TF0);  // 等待溢出
    //     TR0 = 0;      // 停止定时器
    //     TF0 = 0;      // 清除标志
    // }
    unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 1000; j++) {
            __nop;
        }
    }
}