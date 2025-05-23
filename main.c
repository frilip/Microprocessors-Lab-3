#include "platform.h"
#include <stdio.h>
#include <stdint.h>
#include "uart.h"
#include <string.h>
#include "queue.h"
#include "gpio.h"
#include "timer.h"
#include <stdbool.h>
#include "delay.h"


/*
*--------------------- 3nd project in Microprocessors -----------------------*
*/



/*         UART variable definitions         */
#define BUFF_SIZE 128 //read buffer length
#define MENU_LINES 6
#define MAX_STATE_TIME 700

struct menu_text {
	const char *text;
};

struct menu_text test[4] = {"1.", "2.", "3.", "4."};
const char* password = "password";
const char* menu = "==== Environmental System ====\r\nOptions:\r\n";
const char* highlight_front = "\033[43;30m";
const char* highlight_back = "\033[0m";
char display_message [100];
uint8_t selection = 1;

Queue rx_queue;       // Queue for storing received characters
char buff[BUFF_SIZE]; // The UART read string will be stored here
uint32_t buff_index;


/*       Interrupt Service Routine for UART receive       */
void uart_rx_isr(uint8_t rx) {
	// Check if the received character is a printable ASCII character
	if (rx >= 0x0 && rx <= 0x7F ) {
		// Store the received character
		queue_enqueue(&rx_queue, rx);
	}
}

void uart_menu() {
	strcat(display_message, menu);
	for (int i = 0; i < 4; i++) {
		if (i == selection) {
			strcat(display_message, highlight_front);
			strcat(display_message, test[i].text);
			strcat(display_message, highlight_back);
		} else {
			strcat(display_message, test[i].text);
		}
		strcat(display_message, "\r\n");
	}
	strcat(display_message, "Command: ");
	uart_print(display_message);
	
	for (int i = 0; i < buff_index; i++) {
		uart_print("\033[1C");
	}
}




bool check_password(const char * buff){
	return !strcmp(buff, password);
}




void clear_text(int lines){
	
	for (int i = 0; i < lines; i++) {
		uart_print("\033[A\r            ");
	}
	uart_tx('\r');
	display_message[0] = '\0';
}


void touch_sensor_isr(int status) {
	uart_print("Don't touch me!\r\n");
}


/*      Interrupt Sevice Routine for LED blinking      */
void TIM2_IRQHandler(void) {
	// toggle the LED every 200ms
	
	if (TIM2->SR & TIM_SR_UIF) {	// Check if update interrupt flag is set
		TIM2->SR &= ~TIM_SR_UIF;		// Clear the flag immediately
	}
}

int observer(Pin pin, int state) {
	unsigned int n;
	char temp[50];
	
	n = 0;
	while (n < MAX_STATE_TIME) {
		delay_us(2);
		n++;
		if ((state && gpio_get(pin) > 0) || (!state && !gpio_get(pin))) {
			return n;
		}
	}
	
	return 0;
}

int main() {
	bool status_login = true;
	
	// Variables to help with UART read / write
	uint8_t rx_char = 0;
	
	// Initialize the receive queue and UART
	queue_init(&rx_queue, 128);
	uart_init(115200);
	uart_set_rx_callback(uart_rx_isr); // Set the UART receive callback function
	uart_enable(); // Enable UART module
	
	__enable_irq(); // Enable interrupts
	
	uart_print("\r\n");// Print newline
	
	
	// Initialize the led interrupt timer
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;  // Enable clock for TIM2
	// set the amount of ticks relative to the clock speed
	TIM2->PSC = 15999;  // Prescaler: divide 16 MHz by 16000 = 1 kHz
	TIM2->ARR = 9999;   // Auto-reload: 200 ticks at 1 kHz = 200 ms
	TIM2->DIER |= TIM_DIER_UIE;     // Enable update interrupt (overflow interrupt)
	NVIC_SetPriority(TIM2_IRQn, 3);  // set the priority 
	NVIC_EnableIRQ(TIM2_IRQn);
	TIM2->CR1 |= TIM_CR1_CEN;	   // Start TIM2
	
	// Initialize the Touch sensor
	// gpio_set_mode(PC_8, PullDown); // Set touch sensor out pin to PullDown (input)
	// gpio_set_trigger(PC_8, Rising);
	// gpio_set_callback(PC_8, touch_sensor_isr);
	
	// Initialize the DHT11 sensor
	char temp[100];
	// DHT11_InitTypeDef *my_DHT11;
	// DHT11_init(my_DHT11, PC_8);
	/*
	if (DHT11_ReadData(my_DHT11) == 2) {
		uart_print("Ton hpiame...\r\n");
	} else {
		uart_print("Den ton hpiame...\r\n");
	}
	*/
	// sprintf(temp, "Temperature: %f, Humidity: %f\r\n", my_DHT11->Temperature, my_DHT11->Humidity);
	// uart_print(temp);
	
	
	
	
	
	uint8_t Bits = 0;
	uint8_t Packets[DHT11_MAX_BYTE_PACKETS] = {0};
	int states[40] = {0};
	uint8_t PacketIndex = 0;
	uint8_t state_time;
	float Humidity;
	float Temperature;
	
	gpio_set_mode(PC_8, Output);
	// PULLING the Line to Low and waits for 20ms
	gpio_set(PC_8, 0);
	delay_ms(20);
	// PULLING the Line to HIGH and waits for 40us
	gpio_set(PC_8, 1);
	delay_us(40);

	// __disable_irq();
	gpio_set_mode(PC_8, Input);

	// If the Line is still HIGH, that means DHT11 is not responding
	if(gpio_get(PC_8)) {
		uart_print("ERROR!\r\n");
	}
	
	// Wait for HIGH
	if(!observer(PC_8, 1)) {
		uart_print("TIMEOUT OUTSIDE 1!\r\n");
	}

	// Now DHT11 have pulled the Line to HIGH, we will wait till it PULLS is to LOW
	// which means the handshake is done
	if(!observer(PC_8, 0)) {
		uart_print("TIMEOUT OUTSIDE 2!\r\n");
	}
	
	// WE ARE LOW HERE
	while(Bits < 40) {
		/*
		if (!gpio_get(PC_8)) {
			uart_print("LOW\r\n");
		}
		*/
		// DHT11 is now starting to transmit One Bit
		// We will wait till it PULL the Line to HIGH
		// uart_tx('-');
		if(!observer(PC_8, 1)) {
			uart_print("TIMEOUT 1!\r\n");
		}

		// Now we will just count the us it stays HIGH
		// 28us means 0
		// 70us means 1
		state_time = 2 * observer(PC_8, 0);
		states[Bits] = state_time;
		if(!(state_time)) {
			uart_print("TIMEOUT 2!\r\n");
		}

		Packets[PacketIndex] = Packets[PacketIndex] << 1;
		Packets[PacketIndex] |= (state_time > 10); // 50us is good in between
		Bits++;
		if(!(Bits % 8)) PacketIndex++;
	}
	
	// Last 8 bits are Checksum, which is the sum of all the previously transmitted 4 bytes
	if(Packets[4] != (Packets[0] + Packets[1] + Packets[2] + Packets[3])) {
		uart_print("MISMATCH!\r\n");
	}

	Humidity = Packets[0] + (Packets[1] * 0.1f);
	Temperature = Packets[2] + (Packets[3] * 0.1f);
	sprintf(temp, "Humidity: %f, Temperature: %f\r\n", Humidity, Temperature);
	uart_print(temp);
	
	for (int i; i < 40; i++) {
		sprintf(temp, "%d ", states[i]);
		uart_print(temp);
	}
	uart_print("\r\n");
	
	/*
	while(1) {
		
		// Prompt the user to enter the password
		if (status_login && check_password(buff)){
			status_login = false;
			uart_print("Succesful Login\r\n");
			buff_index = 0; // Reset buffer index
			uart_menu();
		} else if (status_login) {
			uart_print("Enter your password: ");
		}
		
		buff_index = 0; // Reset buffer index
		
		do {
			// Wait until a digit or dash is received in the queue
			while (!queue_dequeue(&rx_queue, &rx_char))
				__WFI(); // Wait for Interrupt
			
			if (rx_char == 0x9 && !status_login) {
				clear_text(MENU_LINES);
				selection = (selection + 1) % 4;
				uart_menu();
				continue;
			}

			if (rx_char == 0x7F) { // Handle backspace character
				if (buff_index > 0) {
					buff_index--; // Move buffer index back
					uart_tx(rx_char); // Send backspace character to erase on terminal
				}
			} else if (rx_char >= 0x20 && rx_char <= 0x7E || rx_char == '\r') {
				// Store and echo the received character back
				buff[buff_index++] = (char)rx_char; // Store digit or dash in buffer
				uart_tx(rx_char); // Echo digit or dash back to terminal
			}
		} while (rx_char != '\r' && buff_index < BUFF_SIZE); // Continue until Enter key or buffer full
		
		// Replace the last character with null terminator to make it a valid C string
		buff[buff_index - 1] = '\0';
		uart_print("\r\n"); // Print newline
		uart_print(buff);
		
		// Check if buffer overflow occurred
		if (buff_index >= BUFF_SIZE) {
			uart_print("Stop trying to overflow my buffer! I resent that!\r\n");
		}
	}
	*/
}
