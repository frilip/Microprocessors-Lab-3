#include "platform.h"
#include "gpio.h"
#include "delay.h"
#include "uart.h"
#include <stdio.h>

uint32_t IRQ_status;
uint32_t IRQ_port_num;
uint32_t IRQ_pin_index;
uint32_t EXTI_port_set;
uint32_t prioritygroup;
uint32_t priority;

static void (*GPIO_callback)(int status);

void gpio_toggle(Pin pin) {
	// Toggles a GPIO pin.
	// In the absence of a pin toggle register, can be easily
	// implemented with:
	// gpio_set(pin, !gpio_get(pin));
	
	gpio_set(pin, !gpio_get(pin));
}

void gpio_set(Pin pin, int value) {
	// Sets the selected pin to the specified value.
	
	GPIO_TypeDef* p = GET_PORT(pin);
	uint32_t pin_index = GET_PIN_INDEX(pin);
	
	MODIFY_REG(p->ODR,1UL<<pin_index,value<<pin_index);
}

int gpio_get(Pin pin) {
	// Gets the current value of the specified pin.
	
	GPIO_TypeDef* p = GET_PORT(pin);
	uint32_t pin_index = GET_PIN_INDEX(pin);
	
	return READ_BIT(p->IDR,(1<<pin_index));
	
}

void gpio_set_range(Pin pin_base, int count, int value) {
	// Sets a range of pins to the specified value.
	
	// This can be used to write to an entire port, or just
	// a subset of the (consecutive) pins on a port.
	
	// The output pin_base should be set to the LSB of
	// value. Pin (pin_base + 1) should be LSB + 1, etc.
	
	// The mask for the value parameter should be:
	// ((1 << count) - 1).
	
  GPIO_TypeDef* p = GET_PORT(pin_base);
	uint32_t pin_index = GET_PIN_INDEX(pin_base);
	
	MODIFY_REG(p->ODR,((1UL<<count)-1)<<pin_index,value<<pin_index);
}

unsigned int gpio_get_range(Pin pin_base, int count) {
	// Gets a range of pins.
	
	// This can be used to read an entire port, or just
	// a subset of the (consecutive) pins on a port.
	
	// The LSB of the return value should be the state of
	// pin_base. (LSB + 1) of the return value should be
	// the state of (pin_base + 1) etc.
	// e.g. if the state of pin_base is P1_0 (port 1, pin 0)
	// and count is 4 this, function should return:
	// (P1_3 << 3) | (P1_2 << 2) | (P1_1 << 1) | P1_0
	
	// The mask for the value parameter should be:
	// ((1 << count) - 1).
	GPIO_TypeDef* p = GET_PORT(pin_base);
	uint32_t pin_index = GET_PIN_INDEX(pin_base);
	
	return READ_BIT(p->IDR,(((1 << count) - 1)<<pin_index))>>pin_index;
}

void gpio_set_mode(Pin pin, PinMode mode) {
	// Sets the output mode of a pin.
	
	// If the clock for the pin's port needs to be enabled, it
	// should be done here.
	
	// The modes configure the GPIO pin as follows:
	//  - Reset:    resets to a default state.
	//  - Input:    sets as a high impedance input.
	//  - Output:   sets as a push-pull output.
	//  - PullUp:   enables the internal pull-up resistor
	//              with the pin configured as an input, and
	//              sets the output to logic high (through
	//              the pull-up resistor).
	//  - PullDown: enables the internal pull-down resistor
	//              with the pin configured as an input, and
	//              sets the output to logic low (through the
	//              pull-down).
	
	GPIO_TypeDef* p = GET_PORT(pin);
	uint32_t pin_index = GET_PIN_INDEX(pin);
	RCC->AHB1ENR|=1UL<<GET_PORT_INDEX(pin);//enable clock output
	// Enable clock for interrupts
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
	// Enable debug in low-power mode
	DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY;

	switch(mode) {
		case Reset:
			MODIFY_REG(p->MODER, 3UL<<((pin_index)*2), 0UL<<((pin_index)*2));  
			MODIFY_REG(p->PUPDR, 3UL<<((pin_index)*2), 0UL<<((pin_index)*2));  
			break;
		case Input:
			MODIFY_REG(p->MODER, 3UL<<((pin_index)*2), 0UL<<((pin_index)*2));  
			MODIFY_REG(p->PUPDR, 3UL<<((pin_index)*2), 0UL<<((pin_index)*2));  
			break;
		case Output:
			MODIFY_REG(p->MODER, 3UL<<((pin_index)*2), 1UL<<((pin_index)*2)); 
			MODIFY_REG(p->PUPDR, 3UL<<((pin_index)*2), 0UL<<((pin_index)*2)); 
			break;
		case PullUp:
			MODIFY_REG(p->MODER, 3UL<<((pin_index)*2), 0UL<<((pin_index)*2));  
			MODIFY_REG(p->PUPDR, 3UL<<((pin_index)*2), 1UL<<((pin_index)*2)); 
			break;
		case PullDown:
			MODIFY_REG(p->MODER, 3UL<<((pin_index)*2), 0UL<<((pin_index)*2));  
			MODIFY_REG(p->PUPDR, 3UL<<((pin_index)*2), 10UL<<((pin_index)*2)); 
			break;
	}
}

void gpio_set_trigger(Pin pin, TriggerMode trig) {
	// Sets the interrupt trigger for the specified pin.
	
	// The modes are as follows:
	//  - None:    Disable the trigger.
	//  - Rising:  Trigger on transition from logic low to
	//             high.
	//  - Falling: Trigger on transition from logic high to
	//             low.
	
	uint32_t pin_index = GET_PIN_INDEX(pin);
	
	switch(trig){
		case None:
			EXTI->IMR &= ~(1<<pin_index);
		break;
		case Rising:
			EXTI->IMR |= (1<<pin_index);
			EXTI->RTSR|= (1<<pin_index);
		break;
		case Falling:
			EXTI->IMR |= (1<<pin_index);
			EXTI->FTSR|= (1<<pin_index);
			EXTI->PR 	&= (0<<pin_index);
		break;
	}
}

void gpio_set_callback(Pin pin, void (*callback)(int status)) {
	// Set up and enable the interrupt on the passed pin's
	// port.
	
	// The callback function should be stored in an internal
	// static function pointer.
	
	// When the port ISR is fired, the callback function
	// should be executed. The status parameter should equal
	// a mask of which pin triggered the interrupt, for
	// example if a callback is set on pin P1_2 (port 1, pin
	// 2) and the interrupt is triggered on port 1, the
	// function callback should be called with argument
	// status equalling 0b00000100.
	// This allows the user to determine the interrupt source
	// with (status & GET_PIN_INDEX(P1_2)).
	__enable_irq();
  IRQ_status = 0;
	IRQ_port_num = GET_PORT_INDEX(pin);
	IRQ_pin_index = GET_PIN_INDEX(pin);
	EXTI_port_set = IRQ_port_num<<(IRQ_pin_index % 4) * 4;
	
	GPIO_callback = callback;
	//Connect the pin to external interrupt line
	switch(IRQ_pin_index){
		case 0:
			SYSCFG->EXTICR[0]|= EXTI_port_set;
			prioritygroup = NVIC_GetPriorityGrouping(); // will return 5
			priority = NVIC_EncodePriority(prioritygroup, 1, 1 ); // Pri=1 , SubPri=1
			NVIC_SetPriority(EXTI0_IRQn, priority);
		  NVIC_EnableIRQ(EXTI0_IRQn);		
		break;

		case 1:
			SYSCFG->EXTICR[0]|= EXTI_port_set;
			prioritygroup = NVIC_GetPriorityGrouping(); // will return 5
			priority = NVIC_EncodePriority(prioritygroup, 1, 1 ); // Pri=1 , SubPri=1
			NVIC_SetPriority(EXTI1_IRQn, priority);
		  NVIC_EnableIRQ(EXTI1_IRQn);
		break;
		
    case 2:
			SYSCFG->EXTICR[0]|= EXTI_port_set;
			prioritygroup = NVIC_GetPriorityGrouping(); // will return 5
			priority = NVIC_EncodePriority(prioritygroup, 1, 1 ); // Pri=1 , SubPri=1
			NVIC_SetPriority(EXTI2_IRQn, priority);
		  NVIC_EnableIRQ(EXTI2_IRQn);
		break;
		
		case 3:
			SYSCFG->EXTICR[0]|= EXTI_port_set;
			prioritygroup = NVIC_GetPriorityGrouping(); // will return 5
			priority = NVIC_EncodePriority(prioritygroup, 1, 1 ); // Pri=1 , SubPri=1
			NVIC_SetPriority(EXTI3_IRQn, priority);
		  NVIC_EnableIRQ(EXTI3_IRQn);
		break;
		
		case 4:
			SYSCFG->EXTICR[1]|= EXTI_port_set;
			prioritygroup = NVIC_GetPriorityGrouping(); // will return 5
			priority = NVIC_EncodePriority(prioritygroup, 1, 1 ); // Pri=1 , SubPri=1
			NVIC_SetPriority(EXTI4_IRQn, priority);
		  NVIC_EnableIRQ(EXTI4_IRQn);
		break;
		
		case 5:
    case 6:
		case 7:
			SYSCFG->EXTICR[1]|= EXTI_port_set;
			prioritygroup = NVIC_GetPriorityGrouping(); // will return 5
			priority = NVIC_EncodePriority(prioritygroup, 1, 1 ); // Pri=1 , SubPri=1
			NVIC_SetPriority(EXTI9_5_IRQn, priority);
		  NVIC_EnableIRQ(EXTI9_5_IRQn);
		break;
		
		case 8:
		case 9:
			SYSCFG->EXTICR[2]|= EXTI_port_set;
			prioritygroup = NVIC_GetPriorityGrouping(); // will return 5
			priority = NVIC_EncodePriority(prioritygroup, 1, 1 ); // Pri=1 , SubPri=1
			NVIC_SetPriority(EXTI9_5_IRQn,3);
		  NVIC_EnableIRQ(EXTI9_5_IRQn);
		break;
			
		case 10:
		case 11:
			SYSCFG->EXTICR[2]|= EXTI_port_set;
			prioritygroup = NVIC_GetPriorityGrouping(); // will return 5
			priority = NVIC_EncodePriority(prioritygroup, 1, 1 ); // Pri=1 , SubPri=1
			NVIC_SetPriority(EXTI15_10_IRQn,3);
		  NVIC_EnableIRQ(EXTI15_10_IRQn);
		break;
		
		case 12:
		case 13:
    case 14:
		case 15:
			SYSCFG->EXTICR[3]|= EXTI_port_set;
			prioritygroup = NVIC_GetPriorityGrouping(); // will return 5
			priority = NVIC_EncodePriority(prioritygroup, 1, 1 ); // Pri=1 , SubPri=1
			NVIC_SetPriority(EXTI15_10_IRQn,0);
		  NVIC_EnableIRQ(EXTI15_10_IRQn);
		break;
	}
}

//Note: only four interrupt lines are implemented i.e. only use pin 0-4
void EXTI0_IRQHandler(void){
	
	GPIO_TypeDef* p = ((GPIO_TypeDef*)(AHB1PERIPH_BASE + 0x0400 * IRQ_port_num));
	IRQ_status = EXTI->PR>>IRQ_pin_index & 1;		
	NVIC_ClearPendingIRQ(EXTI0_IRQn);
	EXTI->PR|=(1<<IRQ_pin_index);
	
	if(p->IDR&(1<<IRQ_pin_index)){
		GPIO_callback(IRQ_pin_index);
	}
}

void EXTI1_IRQHandler(void){
	
	GPIO_TypeDef* p = ((GPIO_TypeDef*)(AHB1PERIPH_BASE + 0x0400 * IRQ_port_num));
	IRQ_status = EXTI->PR>>IRQ_pin_index & 1;	
	NVIC_ClearPendingIRQ(EXTI1_IRQn);
	EXTI->PR|=(1<<IRQ_pin_index);
	
	if(p->IDR&(1<<IRQ_pin_index)){
		GPIO_callback(IRQ_pin_index);
	}
	
}

void EXTI2_IRQHandler(void){
	
	GPIO_TypeDef* p = ((GPIO_TypeDef*)(AHB1PERIPH_BASE + 0x0400 * IRQ_port_num));
	IRQ_status = EXTI->PR>>IRQ_pin_index & 1;	
	NVIC_ClearPendingIRQ(EXTI2_IRQn);
	EXTI->PR|=(1<<IRQ_pin_index);
	
	if(p->IDR&(1<<IRQ_pin_index)){
		GPIO_callback(IRQ_pin_index);
	}
	
}

void EXTI3_IRQHandler(void){
	
	GPIO_TypeDef* p = ((GPIO_TypeDef*)(AHB1PERIPH_BASE + 0x0400 * IRQ_port_num));
	IRQ_status = EXTI->PR>>IRQ_pin_index & 1;	
	NVIC_ClearPendingIRQ(EXTI3_IRQn);
	EXTI->PR|=(1<<IRQ_pin_index);
	
	if(p->IDR&(1<<IRQ_pin_index)){
		GPIO_callback(IRQ_pin_index);
	}
	
}

void EXTI4_IRQHandler(void){
	
	GPIO_TypeDef* p = ((GPIO_TypeDef*)(AHB1PERIPH_BASE + 0x0400 * IRQ_port_num));
	IRQ_status = EXTI->PR>>IRQ_pin_index & 1;
	NVIC_ClearPendingIRQ(EXTI4_IRQn);
	EXTI->PR|=(1<<IRQ_pin_index);
	
	if(p->IDR&(1<<IRQ_pin_index)){
		GPIO_callback(IRQ_pin_index);
	}
}

void EXTI9_5_IRQHandler(void){
	
	GPIO_TypeDef* p = ((GPIO_TypeDef*)(AHB1PERIPH_BASE + 0x0400 * IRQ_port_num));
	IRQ_status = (EXTI->PR>>IRQ_pin_index) & 1;
	NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
	EXTI->PR|=(1<<IRQ_pin_index);
	
	if(p->IDR&(1<<IRQ_pin_index)){
		GPIO_callback(IRQ_pin_index);
	}
}

void EXTI15_10_IRQHandler(void){
	
	GPIO_TypeDef* p = ((GPIO_TypeDef*)(AHB1PERIPH_BASE + 0x0400 * IRQ_port_num));
	IRQ_status = (EXTI->PR>>IRQ_pin_index) & 1;
	NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
	EXTI->PR|=(1<<IRQ_pin_index);
	
	GPIO_callback(IRQ_pin_index); // No check is needed, since we are using a Falling Edge trigger...
	
	// if(!(p->IDR&(1<<IRQ_pin_index))){
	// 	GPIO_callback(IRQ_pin_index);
	// }
	
}

// ****************** DHT11 *********************** //

void DHT11_init(DHT11_InitTypeDef *DHT11, int GPIO_Pin) {
	DHT11->_Pin = GPIO_Pin;
	DHT11->Temperature = 0.0f;
	DHT11->Humidity = 0.0f;
}

/*
void HAL_DHT11_DeInit(DHT11_InitTypeDef *DHT11) {
	HAL_GPIO_DeInit(DHT11->_GPIOx, DHT11->_Pin);
	HAL_TIM_Base_Stop(DHT11->_Tim);
}*/

/*
const char* const HAL_DHT11_GetErrorMsg(DHT11_StatusTypeDef Status) {
	return ErrorMsg[Status];
}
*/

/*
static bool DHT11_ObserveState(DHT11_InitTypeDef *DHT11, uint8_t FinalState) {
	__HAL_TIM_SET_COUNTER(DHT11->_Tim, 0);
	while(__HAL_TIM_GET_COUNTER(DHT11->_Tim) < DHT11_MAX_TIMEOUT) {
		if(HAL_GPIO_ReadPin(DHT11->_GPIOx, DHT11->_Pin) == FinalState) return true;
	}

	return false;
}
*/

DHT11_StatusTypeDef DHT11_ReadData(DHT11_InitTypeDef *DHT11) {
	uint8_t Bits = 0;
	uint8_t Packets[DHT11_MAX_BYTE_PACKETS] = {0};
	uint8_t PacketIndex = 0;

	// char temp[100];
	// sprintf(temp, "%d\r\n", DHT11->_Pin);
	// uart_print(temp);
	
	gpio_set_mode(DHT11->_Pin, Output);
	// PULLING the Line to Low and waits for 20ms
	gpio_set(DHT11->_Pin, 0);
	delay_ms(20);
	// PULLING the Line to HIGH and waits for 40us
	gpio_set(DHT11->_Pin, 1);
	delay_us(40);

	// __disable_irq();
	gpio_set_mode(DHT11->_Pin, Input);

	// If the Line is still HIGH, that means DHT11 is not responding
	if(gpio_get(DHT11->_Pin)) {
		// __enable_irq();
		return DHT11_ERROR;
	}


	// Now DHT11 have pulled the Line to LOW, we will wait till it PULLS is HIGH
	// if(!DHT11_ObserveState(DHT11, GPIO_PIN_SET)) {
	// 	__enable_irq();
	// 	return DHT11_TIMEOUT;
	// }

	// Now DHT11 have pulled the Line to HIGH, we will wait till it PULLS is to LOW
	// which means the handshake is done
	// if(!DHT11_ObserveState(DHT11, GPIO_PIN_RESET)) {
	// 		__enable_irq();
	// 		return DHT11_TIMEOUT;
	// }

	while(Bits < 40) {
			// DHT11 is now starting to transmit One Bit
			// We will wait till it PULL the Line to HIGH
			if(!DHT11_ObserveState(DHT11, GPIO_PIN_SET)) {
				__enable_irq();
				return DHT11_TIMEOUT;
			}

			// Now we will just count the us it stays HIGH
			// 28us means 0
			// 70us means 1
			__HAL_TIM_SET_COUNTER(DHT11->_Tim, 0);
			while(HAL_GPIO_ReadPin(DHT11->_GPIOx, DHT11->_Pin) == GPIO_PIN_SET) {
				if(__HAL_TIM_GET_COUNTER(DHT11->_Tim) > DHT11_MAX_TIMEOUT) {
					return DHT11_TIMEOUT;
				}
			}

			Packets[PacketIndex] = Packets[PacketIndex] << 1;
			Packets[PacketIndex] |= (__HAL_TIM_GET_COUNTER(DHT11->_Tim) > 50); // 50us is good in between
			Bits++;
			if(!(Bits % 8)) PacketIndex++;
	}

	__enable_irq();

	// Last 8 bits are Checksum, which is the sum of all the previously transmitted 4 bytes
	if(Packets[4] != (Packets[0] + Packets[1] + Packets[2] + Packets[3])) {
		return DHT11_CHECKSUM_MISMATCH;
	}

	DHT11->Humidity = Packets[0] + (Packets[1] * 0.1f);
	DHT11->Temperature = Packets[2] + (Packets[3] * 0.1f);
	*/

	return DHT11_OK;
}


// *******************************ARM University Program Copyright © ARM Ltd 2016*************************************
