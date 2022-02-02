#include <8051.h>

// PWM setup
unsigned char k = 30; // default
unsigned char k_copy = 30; // default

unsigned char k_changed = 0;

unsigned int L = 553; // default
unsigned int H = 17879; // default

__bit __at(0x01) pwm_flag;
__bit __at(0x97) test_diod;

unsigned char pwm_value[4] = {'\0'};

unsigned char software_timer = 0;

unsigned int TL0_STATE0 = (47104 + 553) % 256; 
unsigned int TH0_STATE0 = (47104 + 553) / 256;

unsigned int TL0_STATE1 = (47104 + 17879) % 256; 
unsigned int TH0_STATE1 = (47104 + 17879) / 256;
// PWM setup end

__bit __at(0x96) led_off; // 1 - off, 0 - on
unsigned char led_index = 0;

__bit __at(0x0B5) kbd_on; // 1 - on, 0 - off
__bit __at(0x00) key_down; // 1 - down, 0 - no
unsigned char keys[] = {0b00001000, 0b00010000, 0b00100000, 0b00000100}; // up , down, left, right

__xdata unsigned char * CSDS = (__xdata unsigned char *) 0xFF30;  
__xdata unsigned char * CSDB = (__xdata unsigned char *) 0xFF38;

__xdata unsigned char * LCDWC = (__xdata unsigned char *) 0xFF80;  
__xdata unsigned char * LCDWD = (__xdata unsigned char *) 0xFF81;  
__xdata unsigned char * LCDRC = (__xdata unsigned char *) 0xFF82; 

__xdata unsigned char * CS55B = (__xdata unsigned char *) 0xFF29; 
__xdata unsigned char * CS55D = (__xdata unsigned char *) 0xFF2B; 

__xdata unsigned char * CSKB1 = (__xdata unsigned char *) 0xFF22;

__code unsigned char change_state[] = {'>', 'C', 'H', 'A', 'N', 'G', 'E', ' ', 'S', 'T', 'A', 'T', 'E', '\0'};
__code unsigned char settings[] = {'>', 'S', 'E', 'T', 'T', 'I', 'N', 'G', 'S', '\0'};
__code unsigned char start[] = {'>', 'S', 'T', 'A', 'R', 'T', '\0'};
__code unsigned char stop[] = {'>', 'S', 'T', 'O', 'P', '\0'};
__code unsigned char pwm[] = {'>', 'P', 'W', 'M', '\0'};
__code unsigned char reset[] = {'>', 'R', 'E', 'S', 'E', 'T', '\0'};

__code unsigned char numbers[] = {0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110, 0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111}; 

void pwm_init();
void lcd_init();

void lcd_wait_while_busy();
void lcd_cmd(unsigned char);
void lcd_data(unsigned char);

void main_page_change_state(); // current_line == 0
void main_page_settings(); // current_line == 1

void change_state_start(); // current_line == 2
void change_state_stop(); // current_line == 3

void settings_pwm(); // current_line == 4
void settings_reset(); // current_line == 5

void pwm_edit(); // current_line == 6

void refresh_display();

void calculate();

void transmission_init();
void parse_string_to_num();

unsigned char receive_buf[4] = {'\0'};
unsigned char receive_buf_index;
__bit __at(0x19) receive_flag;

unsigned char left_digit;
unsigned char middle_digit;
unsigned char right_digit;

unsigned char temp_number = 0;

void t0_int() __interrupt(1){
	TF0 = 0;
	if (pwm_flag) {
		pwm_flag = 0;
		test_diod = 1;
		
		*(CS55B) = 0x00;
		
		TL0 = TL0_STATE0;
		TH0 = TH0_STATE0;
	} else {
		pwm_flag = 1;
		test_diod = 0;
		
		*(CS55B) = 0xFF;
		
		software_timer++; 
		
		TL0 = TL0_STATE1;
		TH0 = TH0_STATE1;
	}
}

void serial_int() __interrupt(4){
	if (RI) {
		receive_buf[receive_buf_index] = SBUF;
		++receive_buf_index;
		
		RI = 0;
		receive_flag = 1;
	}
}

void main(){
	unsigned char key_mat_1, key_mult, i;
	
	unsigned char current_line = 0;
	
	lcd_init();
	
	pwm_init();
	
	transmission_init();
	
	main_page_change_state();
	
	while(1){
		if (receive_flag) {
			if (receive_buf_index == 3) {
				parse_string_to_num();
				
				receive_buf_index = 0;
			}
			receive_flag = 0;
		}
		
		if(software_timer >= 2){
			software_timer = 0;
			
			if(k_changed == 1)
				calculate();
		}
		
		refresh_display();
		
		key_mult = 0b00000000;
		key_mat_1 = *CSKB1;
		
		led_off = 1;
		
		for(i = 0; i < 4; ++i){
			*CSDS = keys[i];
			if(kbd_on){
				key_mult = keys[i];
				break;
			}
		}
		
		if(led_index == 0){
			*CSDS = 0x01<<led_index;
			*CSDB = numbers[k % 10];
		} else if (led_index == 1){
			*CSDS = 0x01<<led_index;
			*CSDB = numbers[(k / 10) % 10];
		} else if (led_index == 2){
			*CSDS = 0x01<<led_index;
			*CSDB = numbers[(k / 100) % 10];
		}
		
		led_off = 0;
		
		if(key_mat_1 == 0b11111111 && key_mult == 0b00000000 && key_down == 1){
			key_down = 0;
		}
		
		if(key_down == 0){
			if(key_mat_1 == 0b11011111 && current_line == 0){ // down key change state
				lcd_cmd(0b00000001);
		
				current_line++;
				
				main_page_settings(); // show main page setting selected
				
				key_down = 1;
			} else if(key_mat_1 == 0b11101111 && current_line == 1){ // up key settings
				lcd_cmd(0b00000001);
			
				current_line--;
								
				main_page_change_state(); // show main page change state selected
				
				key_down = 1;
			} else if(key_mat_1 == 0b01111111 && current_line == 0){ // enter change state
				lcd_cmd(0b00000001);
				
				current_line += 2;
				
				change_state_start(); // show change_state->start selected
				
				key_down = 1;
			} else if(key_mat_1 == 0b11011111 && current_line == 2){ // down key start 
				lcd_cmd(0b00000001);
				
				current_line++;
				
				change_state_stop(); // show change_state->stop selected
				
				key_down = 1;
			} else if(key_mat_1 == 0b11101111 && current_line == 3){ // up key stop
				lcd_cmd(0b00000001);
				
				current_line--;
				
				change_state_start(); // show change_state->start selected
				
				key_down = 1;
			} else if(key_mat_1 == 0b01111111 && current_line == 1){ // enter settings
				lcd_cmd(0b00000001);
				
				current_line += 3;
				
				settings_pwm(); // show settings->pwm
				
				key_down = 1;
			} else if(key_mat_1 == 0b11011111 && current_line == 4){ // down key pwm
				lcd_cmd(0b00000001);
				
				current_line++;
				
				settings_reset(); // show settings->reset
				
				key_down = 1;
			} else if(key_mat_1 == 0b11101111 && current_line == 5){
				lcd_cmd(0b00000001);
				
				current_line--;
				
				settings_pwm(); // show settings->reset
				
				key_down = 1;
			} else if(key_mat_1 == 0b10111111 && (current_line == 2 || current_line == 3)){ // esc from change state submenu
				lcd_cmd(0b00000001);
				
				current_line = 0;
				
				main_page_change_state(); // show main page change_state selected
				
			    key_down = 1;
			} else if(key_mat_1 == 0b10111111 && (current_line == 4 || current_line == 5)){ // esc from change state submenu
				lcd_cmd(0b00000001);
				
				current_line = 1;
				
				main_page_settings(); // show main page settings selected
				
				key_down = 1;
			} else if(key_mat_1 == 0b01111111 && current_line == 2){ // timer run
				TR0 = 1; 
				
				key_down = 1;
			} else if(key_mat_1 == 0b01111111 && current_line == 3){ // timer stop
				TR0 = 0;
				
				key_down = 1;
			} else if(key_mat_1 == 0b01111111 && current_line == 5){ // reset
				k = 30;
				k_copy = 30;
				
				k_changed = 1;
				
				key_down = 1;
			} else if(key_mat_1 == 0b01111111 && current_line == 4){ // pwm edit
				lcd_cmd(0b00000001);
				
				current_line += 2;
				
				pwm_value[0] = ((k / 100) % 10) + 48;
				pwm_value[1] = ((k / 10) % 10) + 48;
				pwm_value[2] = (k % 10) + 48;
				
				pwm_edit();
				
				key_down = 1;
			} else if(key_mat_1 == 0b11101111 && current_line == 6){ // pwm += 1
				if(k_copy < 120){
					lcd_cmd(0b00000001);
					
					k_copy++;
				
					pwm_value[0] = ((k_copy / 100) % 10) + 48;
					pwm_value[1] = ((k_copy / 10) % 10) + 48;
					pwm_value[2] = (k_copy % 10) + 48;
				
					pwm_edit();
				}
				key_down = 1;
			} else if(key_mat_1 == 0b11011111 && current_line == 6){ // pwm -= 1
				if(k_copy > 30){
					lcd_cmd(0b00000001);
				
					k_copy--;
				
					pwm_value[0] = ((k_copy / 100) % 10) + 48;
					pwm_value[1] = ((k_copy / 10) % 10) + 48;
					pwm_value[2] = (k_copy % 10) + 48;
				
					pwm_edit();
				}
				key_down = 1;
			} else if(key_mat_1 == 0b11110111 && current_line == 6){ // pwm += 10
				if(k_copy <= 110){
					lcd_cmd(0b00000001);
				
					k_copy += 10;
				
					pwm_value[0] = ((k_copy / 100) % 10) + 48;
					pwm_value[1] = ((k_copy / 10) % 10) + 48;
					pwm_value[2] = (k_copy % 10) + 48;
				
					pwm_edit();
				}
				key_down = 1;
			} else if(key_mat_1 == 0b11111011 && current_line == 6){ // pwm -= 10
				if(k_copy >= 40){
					lcd_cmd(0b00000001);
				
					k_copy -= 10;
				
					pwm_value[0] = ((k_copy / 100) % 10) + 48;
					pwm_value[1] = ((k_copy / 10) % 10) + 48;
					pwm_value[2] = (k_copy % 10) + 48;
				
					pwm_edit();
				}
				key_down = 1;
			} else if(key_mat_1 == 0b01111111 && current_line == 6){ // save changes
				lcd_cmd(0b00000001);
			
				k = k_copy;
				k_changed = 1;
				
				current_line -= 2;
				
				settings_pwm();

				key_down = 1;
			} else if(key_mat_1 == 0b10111111 && current_line == 6){ // discard changes
				lcd_cmd(0b00000001);
				
				pwm_value[0] = ((k / 100) % 10) + 48;
				pwm_value[1] = ((k / 10) % 10) + 48;
				pwm_value[2] = (k % 10) + 48;
				
				current_line -= 2;
				
				settings_pwm();
				
				key_down = 1;
			} else if(key_mult == 0b00001000){ // up key mult
				if(k < 120){
					k++;
					k_copy = k;
					k_changed = 1;
				}
				key_down = 1;
			} else if(key_mult == 0b00010000){ // down key mult
				if(k > 30){
					k--;
					k_copy = k;
					k_changed = 1;
				}
				key_down = 1;
			} else if(key_mult == 0b00100000){ // left key mult
				if(k >= 40){
					k -= 10;
					k_copy = k;
					k_changed = 1;
				}
				key_down = 1;
			} else if(key_mult == 0b00000100){ // right key mult
				if(k <= 110){
					k += 10;
					k_copy = k;
					k_changed = 1;
				}
				key_down = 1;
			}
		}
	}
}

void lcd_wait_while_busy(){
	while(*LCDRC & 0b10000000);
}

void lcd_cmd(unsigned char c){
	lcd_wait_while_busy();
	*LCDWC = c;
}

void lcd_data(unsigned char d){
	lcd_wait_while_busy();
	*LCDWD = d;
}

void lcd_init(){
	lcd_cmd(0b00111000);
	lcd_cmd(0b00001100);
	lcd_cmd(0b00000110);
	lcd_cmd(0b00000001);
}

void pwm_init(){
	TMOD &= 0b11110001; // timer mode
	TMOD |= 0b00000001; // timer 0, tryb 1
	
	TL0 = (47104 + 17879) % 256;
	TH0 = (47104 + 17879) / 256;
	
	*(CS55D) = 0x80; 
	
	EA = 1; // zezwolenie obsługi przerwań
	ET0 = 1; // zezwolenie obsługi przerwan od T0
	
	*(CS55B) = 0xFF;
	pwm_flag = 1;
	test_diod = 0;
	
	TF0 = 0; // wyczyszenie znacznika przepelnienia
	
	TR0 = 1; // aktywność licznika T0
}

void transmission_init(){
	SCON = 0b01010000; // SM0 = 0 SM1 = 1 REN = 1
	
	TMOD &= 0b00101111; // ustawienie t1
	TMOD |= 0b00100000; // w tryb 2
	
	TL1 = 0xFD;
	TH1 = 0xFD;
	
	PCON &= 0b01111111; // tryb 0
	
	ES = 1;
	EA = 1;
	
	receive_flag = 0;
	receive_buf_index = 0;
	
	TF1 = 0;
	TR1 = 1;
}

void parse_string_to_num(){
	right_digit = receive_buf[2] - 48;
	middle_digit = receive_buf[1] - 48;
	left_digit = receive_buf[0] - 48;
	
	temp_number = left_digit * 100 + middle_digit * 10 + right_digit * 1;
	
	if (temp_number <= 120 && temp_number >= 30) {
		k = temp_number;
		k_copy = k;
		k_changed = 1;
	}
}

void calculate(){
	L = (18432 / 1000) * k;
	H = 18432 - L;
	
	TL0_STATE0 = (47104 + L) % 256; 
	TH0_STATE0 = (47104 + L) / 256;

	TL0_STATE1 = (47104 + H) % 256; 
	TH0_STATE1 = (47104 + H) / 256;
	
	k_changed = 0;
}

void refresh_display(){
	++led_index;
	if(led_index > 2)
		led_index = 0;
	
	led_off = 1;
			
	if(led_index == 0){
		*CSDS = 0x01<<led_index;
		*CSDB = numbers[k % 10];
	} else if (led_index == 1){
		*CSDS = 0x01<<led_index;
		*CSDB = numbers[(k / 10) % 10];
	} else if (led_index == 2){
		*CSDS = 0x01<<led_index;
		*CSDB = numbers[(k / 100) % 10];
	}
	
	led_off = 0;
}

void main_page_change_state(){
	unsigned char i = 0;
	
	for(i = 0; change_state[i] != '\0'; ++i){ // selected
		lcd_data(change_state[i]);
	}
	
	lcd_cmd(0b11000000);
	
	for(i = 1; settings[i] != '\0'; ++i){ // not selected
		lcd_data(settings[i]);
	}
}

void main_page_settings(){
	unsigned char i = 0;
							
	for(i = 1; change_state[i] != '\0'; ++i){ // not selected
		lcd_data(change_state[i]);
	}
				
	lcd_cmd(0b11000000);
				
	for(i = 0; settings[i] != '\0'; ++i){ // selected
		lcd_data(settings[i]);
	}
}

void change_state_start(){
	unsigned char i = 0;
							
	for(i = 0; start[i] != '\0'; ++i){ // selected
		lcd_data(start[i]);
	}
				
	lcd_cmd(0b11000000);
				
	for(i = 1; stop[i] != '\0'; ++i){ // not selected
		lcd_data(stop[i]);
	}
}

void change_state_stop(){
	unsigned char i = 0;
							
	for(i = 1; start[i] != '\0'; ++i){ // not selected
		lcd_data(start[i]);
	}
				
	lcd_cmd(0b11000000);
				
	for(i = 0; stop[i] != '\0'; ++i){ // selected
		lcd_data(stop[i]);
	}
}

void settings_pwm(){
	unsigned char i = 0;
							
	for(i = 0; pwm[i] != '\0'; ++i){ // selected
		lcd_data(pwm[i]);
	}
				
	lcd_cmd(0b11000000);
				
	for(i = 1; reset[i] != '\0'; ++i){ // not selected
		lcd_data(reset[i]);
	}
}

void settings_reset(){
	unsigned char i = 0;
							
	for(i = 1; pwm[i] != '\0'; ++i){ // not selected
		lcd_data(pwm[i]);
	}
				
	lcd_cmd(0b11000000);
				
	for(i = 0; reset[i] != '\0'; ++i){ // selected
		lcd_data(reset[i]);
	}
}

void pwm_edit(){
	unsigned char i = 0;
	
	for(i = 0; pwm_value[i] != '\0'; ++i){ // selected
		lcd_data(pwm_value[i]);
	}
}
