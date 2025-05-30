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
#define BUFF_SIZE 128 // read buffer length
#define MENU_LINES 9
#define MAX_STATE_TIME 700

#define DHT11 PC_8
#define TOUCH PC_6
#define LED PC_5

typedef struct {
	const char *text;
} menu_text;

menu_text test[4] = {"1. Increase period of read/write data by 1s (max 10s).", 
														"2. Decrease period of read/write data by 1s (min 2s.)",
														"3. Switch between temperature/ humidity/ both.",
														"4. Print current data and System Mode."};
const char *password = "password";
const char *menu = "                ==== Environmental System ====\r\nOptions:\r\n";
const char *highlight_front = "\033[43;30m";
const char *highlight_back = "\033[0m";
char display_message[1500];
int aem_sum;
bool danger = false;
bool print_mode = true;
bool print_menu = false;
bool update_values = false;
bool update_touch_sensor = false;
unsigned int dangerous_values = 0;
unsigned int touch_sensor_clicks = 0;
uint8_t reading_period = 6;
uint8_t selection = 0;

Queue rx_queue;       // Queue for storing received characters
char buff[BUFF_SIZE]; // The UART read string will be stored here
uint32_t buff_index;
														
enum DHT11_output_options {
	BOTH = 0,
	FREQUENCY,
	HUMIDITY
};

enum uart_mode {
	MAIN = 0,
	PASSWORD,
	AEM
};
enum uart_mode MODE = PASSWORD;

enum DHT11_output_options display_cases = BOTH;
float temperature;
int humidity;

/*
enum mode_options {
	MODE_A = 0,
	MODE_B
};
enum mode_options mode = MODE_A;
*/

char mode = 'A';
uint8_t safe_values_count = 0;

/* ----------------------------------------------------------------- */
/* -----------------   HANDLER FUNCTIONS - START   ----------------- */
/* ----------------------------------------------------------------- */

void uart_menu_handler(uint8_t selection, bool highlight) {
	
	// Move up enough lines to clear the previous menu instance
	for (int i = 0; i < MENU_LINES; i++) {
		uart_print("\033[A\r");
	}
	
	display_message[0] = '\0';
	strcat(display_message, menu);
	for (int i = 0; i < 4; i++) {
		if (i == selection && highlight) {
			strcat(display_message, highlight_front);
			strcat(display_message, test[i].text);
			strcat(display_message, highlight_back);
		} else {
			strcat(display_message, test[i].text);
		}
		strcat(display_message, "\r\n");
	}
	strcat(display_message, "\n\n\n");
	strcat(display_message, "Command: ");
	uart_print(display_message);
	
	for (int i = 0; i < buff_index; i++) {
		uart_print("\033[1C");
	}
}

void DHT11_data_handler() {		
	
	switch (display_cases){
		case BOTH:
			sprintf(display_message, "Humidity: %d, Temperature: %f, reading with period = %d sec \r\n\n", humidity, temperature, reading_period);
			break;
		case FREQUENCY:
			sprintf(display_message, "Temperature: %f,               reading with period = %d sec \r\n\n", temperature, reading_period);
			break;
		case HUMIDITY:
			sprintf(display_message, "Humidity: %d,                         reading with period = %d sec \r\n\n", humidity, reading_period);
			break;
	}
	
	
	// print in correct place
	uart_print("\033[?25l"); // Hide the cursor
	uart_print("\033[A\033[A\r                            ");
	uart_print(display_message);
	// return the cursor where it was, buff_index + 9 to the right (9 is from the "Command: " text)
	for (int i = 0; i < buff_index + 9; i++) {
		uart_print("\033[1C");
	}
	uart_print("\033[?25h"); // Reveal the cursor
}

void update_timer_frequency(uint32_t new_reading_period_seconds) {
    TIM2->CR1 &= ~TIM_CR1_CEN;  // 1. Stop the timer

    // 2. Update the prescaler and ARR
    TIM2->PSC = 15999;  // 16 MHz / 16000 = 1 kHz tick
    TIM2->ARR = 1000 * new_reading_period_seconds - 1;  // 3. Set new period in milliseconds

    TIM2->CNT = 0;  // 4. Reset the counter (optional but recommended)
    TIM2->CR1 |= TIM_CR1_CEN;  // 5. Restart the timer
}

int observer(Pin pin, int state) {
	unsigned int n = 0;
	
	while (n < MAX_STATE_TIME) {
		delay_us(2);
		n++;
		if ((state && gpio_get(pin) > 0) || (!state && !gpio_get(pin))) {
			return n;
		}
	}
	
	return 0;
}

void DHT11_read_data(Pin pin) {
	uint8_t Bits = 0;
	uint8_t Packets[DHT11_MAX_BYTE_PACKETS] = {0};
	uint8_t PacketIndex = 0;
	uint8_t state_time;
	
	__disable_irq();
	
	gpio_set_mode(DHT11, Output);
	// PULLING the Line to Low and waits for 20ms
	gpio_set(DHT11, 0);
	delay_ms(20);
	// PULLING the Line to HIGH and waits for 40us
	gpio_set(DHT11, 1);
	delay_us(40);

	// __disable_irq();
	gpio_set_mode(DHT11, Input);

	// If the Line is still HIGH, that means DHT11 is not responding
	if(gpio_get(DHT11)) {
		uart_print("ERROR!\r\n");
	}
	
	// Wait for HIGH
	if(!observer(DHT11, 1)) {
		uart_print("TIMEOUT OUTSIDE 1!\r\n");
	}

	// Now DHT11 have pulled the Line to HIGH, we will wait till it PULLS is to LOW
	// which means the handshake is done
	if(!observer(DHT11, 0)) {
		uart_print("TIMEOUT OUTSIDE 2!\r\n");
	}
	
	// WE ARE LOW HERE
	while(Bits < 40) {

		// DHT11 is now starting to transmit One Bit
		// We will wait till it PULL the Line to HIGH
		if(!observer(DHT11, 1)) {
			uart_print("TIMEOUT 1!\r\n");
		}

		// Now we will just count the us it stays HIGH
		// 28us means 0
		// 70us means 1
		state_time = 2 * observer(DHT11, 0);
		if (!(state_time)) {
			uart_print("TIMEOUT 2!\r\n");
		}

		Packets[PacketIndex] = Packets[PacketIndex] << 1;
		Packets[PacketIndex] |= (state_time > 10); // 50us is good in between
		Bits++;
		if(!(Bits % 8)) PacketIndex++;
	}
	
	__enable_irq();
	
	// Last 8 bits are Checksum, which is the sum of all the previously transmitted 4 bytes
	if(Packets[4] != (Packets[0] + Packets[1] + Packets[2] + Packets[3])) {
		uart_print("MISMATCH!\r\n");
	}
	
	humidity = Packets[0] + (Packets[1] * 0.1f);
	temperature = Packets[2] + (Packets[3] * 0.1f);
	
	if (temperature > 35 || humidity > 80) {
		dangerous_values++;
		if (dangerous_values % 3 == 0) {
			__disable_irq();
			uart_print("\033[2J\033[H\n");
			uart_print("TRIGGERING SOFTWARE RESET IN 1 SECOND");
			delay_ms(1000);
			NVIC_SystemReset();
		}
	} else if (mode == 'B' && (temperature > 25 || humidity > 60)) {
		safe_values_count = 0;
		danger = true;
	} else if (danger && temperature < 25 && humidity < 60) {
		safe_values_count++;
		if (safe_values_count >= 5) {
			danger = false;
		}
	}
}

void status_handler() {
	
	// update menu, show higlight
	__disable_irq();
	uart_menu_handler(selection, 1);
	__enable_irq();
	
	// unwrite command
	for (int i = 0; i < buff_index; i++){
		uart_tx(0x7F);
	}
	
	if (!strcmp(buff, "status")){
		// STATUS ACTION HERE
		NVIC_EnableIRQ(TIM3_IRQn);
		TIM3->CR1 |= TIM_CR1_CEN;
		DHT11_read_data(DHT11);
		DHT11_data_handler();
		sprintf(display_message, "\033[A\r                            MODE: %c, Number of MODE changes: %d\r\n\033[9C", mode, touch_sensor_clicks);
		uart_print(display_message);
		print_mode = true;
	}
}

void password_handler() {
	if (!strcmp(buff, password)){
		MODE = AEM;
		uart_print("\r                                                  \r"); // ???
		uart_print("Enter your AEM: ");
	} else {
		// login phase, wrong password
		// delete written 
		uart_print("\r                                                \r");
		
		uart_print("Incorrect password! Try again: ");
	}
}

void aem_handler(){
	
	if (buff_index > 6 || buff_index < 2) {
		uart_print("\r                                                  \r"); // ???
		uart_print("Please enter a valid AEM: ");
		return;
	}

	for (int i = 0; i < buff_index - 1; i++){
		if ((buff[i] < '0' || buff[i] > '9')){
			uart_print("\r                                                  \r"); // ???
			uart_print("Please enter a valid AEM: ");
			return;
		}
	}
	aem_sum = (int)buff[buff_index - 3] + (int)buff[buff_index - 2] - 96; // We subtract 96 to transform to the actual
	if (aem_sum < 2) aem_sum = 2;
	else if (aem_sum > 10) aem_sum = 10;
	MODE = MAIN;
	uart_print("\r                                                  \r"); // ???
	uart_menu_handler(selection, 1);
	NVIC_EnableIRQ(TIM2_IRQn);
	TIM2->CR1 |= TIM_CR1_CEN;	   // Start TIM2
}

/* --------------------------------------------------------------- */
/* -----------------   HANDLER FUNCTIONS - END   ----------------- */
/* --------------------------------------------------------------- */




/* ------------------------------------------------------------- */
/* -----------------   ISR FUNCTIONS - START   ----------------- */
/* ------------------------------------------------------------- */

/*       Interrupt Service Routine for UART receive       */
void uart_rx_isr(uint8_t rx) {
	// Check if the received character is a printable ASCII character
	if (rx >= 0x0 && rx <= 0x7F ) {
		// Store the received character
		queue_enqueue(&rx_queue, rx);
	}
}

/*      Interrupt Sevice Routine for reading DHT11      */
void TIM2_IRQHandler(void) {

	if (TIM2->SR & TIM_SR_UIF) {	// Check if update interrupt flag is set
		TIM2->SR &= ~TIM_SR_UIF;		// Clear the flag immediately
	}
	
	update_values = true;
}

/*      Interrupt Sevice Routine for erasing the status display message      */
void TIM3_IRQHandler(void) {
	
	if (TIM3->SR & TIM_SR_UIF) {	// Check if update interrupt flag is set
		TIM3->SR &= ~TIM_SR_UIF;		// Clear the flag immediately
	}
	
	print_mode = false;
}

void led_blinking_isr() {
	if (danger) gpio_toggle(LED);
}

void touch_sensor_isr(int status) {
	
	touch_sensor_clicks++;
	update_touch_sensor = true;
	if (mode == 'A') {
		mode = 'B';
		timer_init(500000);
		timer_enable();
	} else {
		mode = 'A';
		timer_disable();
		gpio_set(LED, LED_OFF);
	}
}

/* ----------------------------------------------------------- */
/* -----------------   ISR FUNCTIONS - END   ----------------- */
/* ----------------------------------------------------------- */


int main() {
	
	// Initialize the receive queue and UART
	queue_init(&rx_queue, 128);
	uart_init(115200);
	uart_set_rx_callback(uart_rx_isr); // Set the UART receive callback function
	uart_enable(); // Enable UART module
	
	// Initialize the temperature / humidity timer interrupt
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;  // Enable clock for TIM2
	// set the amount of ticks relative to the clock speed
	TIM2->PSC = 15999;  // Prescaler: divide 16 MHz by 16000 = 1 kHz
	TIM2->ARR = 1000 * reading_period - 1;   // Auto-reload: 1000 * period ticks at 1 kHz = period sec
	TIM2->DIER |= TIM_DIER_UIE;     // Enable update interrupt (overflow interrupt)
	NVIC_SetPriority(TIM2_IRQn, 3);  // set the priority
	
	// Initialize the temperature / humidity timer interrupt
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;  // Enable clock for TIM3
	// set the amount of ticks relative to the clock speed
	TIM3->PSC = 15999;  // Prescaler: divide 16 MHz by 16000 = 1 kHz
	TIM3->ARR = 2000;   // Auto-reload: 2000 ticks at 1 kHz = period sec
	TIM3->DIER |= TIM_DIER_UIE;     // Enable update interrupt (overflow interrupt)
	NVIC_SetPriority(TIM3_IRQn, 4);  // set the priority
	
	// Initialize the SysTick timer for the LED blinking
	timer_set_callback(led_blinking_isr);
	
	__enable_irq(); // Enable interrupts
	
	// clear visible page
	uart_print("\033[2J\033[H\n");
	
	// Initialize the Touch sensor
	gpio_set_mode(TOUCH, PullDown); // Set touch sensor out pin to PullDown (input)
	gpio_set_trigger(TOUCH, Rising);
	gpio_set_callback(TOUCH, touch_sensor_isr);
	
	// Initialize the lED
	gpio_set_mode(LED, Output);

	uart_print("Enter your password: ");
	
	// Variables to help with UART read / write
	uint8_t rx_char = 0;
	
	while(1) {
		
		buff_index = 0; // Reset buffer index
		
		do {
			// Wait until a digit or dash is received in the queue
			rx_char = 0;
			while (!queue_dequeue(&rx_queue, &rx_char) && print_mode && !update_values && !update_touch_sensor)
				__WFI(); // Wait for Interrupt
			
			if (update_values) {
				DHT11_read_data(DHT11);
				DHT11_data_handler();
				update_values = false;
			}
			
			if (update_touch_sensor && (touch_sensor_clicks % 3 == 0) && touch_sensor_clicks > 0) {
				reading_period = aem_sum;
				update_timer_frequency(reading_period);
				DHT11_data_handler();
				update_touch_sensor = false;
			}
			
			if (rx_char == 0x9 && MODE == MAIN && buff_index == 0) { // if buff_index > 0 then a command is being written
				selection = (selection + 1) % 4;
				uart_print("\033[?25l");
				uart_menu_handler(selection, 1);
				uart_print("\033[?25h");
				continue;
			}
		
			if (MODE == MAIN && rx_char == '\r' && buff_index == 0) {
				// no command was written
				// act based on selected option
				rx_char = 0; // We zero it out so it doesn't escape the while...
				switch(selection) {
					case 0:
						if (reading_period < 10) reading_period++;
						update_timer_frequency(reading_period);
						DHT11_data_handler();
						break;		
					case 1:
						if (reading_period > 2) reading_period--;
						update_timer_frequency(reading_period);
						DHT11_data_handler();
						break;
					case 2:
						display_cases = (display_cases + 1) % 3;
						DHT11_data_handler();
						break;
					case 3:
						__disable_irq();
						DHT11_read_data(DHT11);
						DHT11_data_handler();
						__enable_irq();
						break;
				}
				buff_index = 0;
			} else if (rx_char == 0x7F && buff_index > 0) { // Handle backspace character
					buff_index--; // Move buffer index back
					uart_tx(rx_char); // Send backspace character to erase on terminal
			} else if (rx_char >= 0x20 && rx_char <= 0x7E || rx_char == '\r') {
				// Store and echo the received character back
				buff[buff_index++] = (char)rx_char; // Store digit or dash in buffer
				uart_tx(rx_char); // Echo digit or dash back to terminal
				
				if (MODE == MAIN && rx_char != '\r'){ // a character has been typed and we are not on login phase, so it's a command 
					// update menu, hide highlight
					__disable_irq();
					uart_menu_handler(selection, 0);
					__enable_irq();
				}
			}
			
			if (MODE == MAIN && !print_mode) {
				sprintf(display_message, "\033[A\r                                                                 \r\n\033[9C");
				uart_print(display_message);
				NVIC_DisableIRQ(TIM3_IRQn);
				print_mode = true;
			}
			
		} while (rx_char != '\r' && buff_index < BUFF_SIZE); // Continue until Enter key or buffer full
		
		// Replace the last character with null terminator to make it a valid C string
		buff[buff_index - 1] = '\0';
		
		switch (MODE) {
			case MAIN:
				status_handler();
				break;
			case PASSWORD:
				password_handler();
				break;
			case AEM:
				aem_handler();
				break;
		}
	}
}
