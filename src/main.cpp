// Project: BookNook Light Timer (v1-260210)
// Author: Alex Franke (CodeCreations), http://www.theFrankes.com
// Date: February 2026
// License: CC-BY-NC-SA 4.0 https://creativecommons.org/licenses/by-nc-sa/4.0/deed.en

#define F_CPU 1000000UL // clock speed 1MHz for delay.h

#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/atomic.h>
#include <util/delay.h>

// Description: 
// This project replaces a simple but energy inefficient touch-activated LED circuit in a 
// bookshelf book nook with a more sophisticated design that incorporates an ATTiny85 
// microcontroller. The new design allows for adjustable timer settings via DIP switches, 
// enabling the user to set how long the LED stays on after being activated by the touch 
// sensor. The microcontroller manages power consumption by entering a low-power sleep mode 
// when the LED is off, waking up only on touch events or timer interrupts. This approach 
// significantly extends battery life while providing greater functionality and user control 
// over the LED timer settings.
 
// Circuit Design: 
// Use the existing battery case, LED and latching touch sensor from the current design. The
// ATTiny85 is powered from the ~3V supply from the batteries. We'll use low-side switching 
// to control the LED, meaning the LED is connected to VCC through a current-limiting resistor 
// (300 ohms) and the ATTiny85 pin will connect to GND when the LED is ON. This allows us to 
// use the internal pull-up resistors for the touch sensor and DIP switches without needing 
// additional components. The touch sensor output connects to a pin configured for pin change 
// interrupts, allowing it to wake the microcontroller from sleep when touched. The DIP 
// switches are read only when the LED is turned on to set the timer duration, and they are 
// otherwise kept in a high-impedance state with pull-ups disabled to minimize power consumption.

// ATTiny85 Pins
//                   ----------     
//     (tie up)  PB5 | 1    8 | VCC  +3.3V
//  DIP_MSB_PIN  PB3 | 2    7 | PB2  DIP_PIN_PROD (active LOW for prod mode, HIGH for debug)
//  DIP_LSB_PIN  PB4 | 3    6 | PB1  TOUCH_PIN (TTP223 output)
//          GND  GND | 4    5 | PB0	 LED_PIN (LOW = ON) -> 100ohm resistor -> LED -> VCC
//                   ----------

// Power Consumption Notes:
// The ATTiny85 in this configuration draws approximately 0.3mA when the LED is off and the
// microcontroller is in sleep mode. When the LED is on, the current draw increases to around
// 10mA without a resistor. By adding a 300 ohm resistor in series with the LED, the current
// draw can be reduced to approximately 1.8mA with fresh batteries, which helps extend battery
// life and improve the longevity of both the LED and the microcontroller. I believe low-side
// switching with a series resistor is the best approach here to minimize power consumption 
// as well because the ATTimy85 is better at sinking current than sourcing it.

// LED Resistor Note: 
// The LED is currently running at around 10mA with no resistor, which is barely withig the 
// recommended current for an ATTiny85 pin (20mA absolute max, but generally safer to stay 
// below 5-10mA for longevity). To reduce the current and extend the life of the LED and the 
// microcontroller, I'll add a resistor in series with the LED, which would drop the current.  
// 300 ohms drops the current to around 1mA for fresh batteries, and it's a little dim, so 
// I'll try 100. This should provide enough light while significantly reducing power 
// consumption and increasing longevity. The LED draws 12mA both in and out of the  original 
// circuit at 2.8V (used to represent a depleated battery voltage). 

// PIN Notes: 
// I switched the pins around a little bit to make the PCB a little cleaner. We don't need the
// hardware interrupt pin for this project since the touch sensor can trigger a pin change 
// interrupt, so I'm using PB1 for the touch sensor and PB2 for the production mode DIP switch. 

// DIP switch testing:  
//   * 000 - 2min (5s delay rounded up to 8s due to WDT granularity)
//   * 001 - 4min (10s delay rounded up to 16s)
//   * 010 - 8min (20s delay rounded up to 24s)
//   * 011 - 16min (40s delay, which matches WDT ganularity so no rounding needed)
//   * 100 - 1hr
//   * 101 - 2hr
//   * 110 - 4hr
//   * 111 - 8hr

// DIP switch for configuration 
const uint8_t DIP_MSB_PIN   = 4;  // PB4, Pin 3
const uint8_t DIP_LSB_PIN   = 2;  // PB2, Pin 7
const uint8_t DIP_HOUR_PIN  = 3;  // PB3, Pin 2 (active LOW for hours mode, HIGH for minutes mode)

// Touch sensor and LED pins
const uint8_t TOUCH_PIN     = 1;  // PB1 - Touch signal input 
const uint8_t LED_PIN       = 0;  // PB0 - LED signal output (LOW = ON)

// Timing
volatile uint32_t sleepCounter = 0;
uint32_t maxSleepSeconds = 0;        // total sleep time in seconds (based on DIP)
volatile bool touchEvent  = false;

bool ledOn = false;

// Number of seconds per sleep cycle (adjusted for low power)
const uint8_t SLEEP_SECONDS = 8; // normal 8s

// Watchdog ISR
ISR(WDT_vect) {
  sleepCounter += SLEEP_SECONDS;
}

// Touch ISR
ISR(PCINT0_vect) {

  bool pinHigh = PINB & (1 << TOUCH_PIN);

  // A touchEvent is only triggered on the transition from LOW to HIGH -- this prevents 
  // multiple triggers from a single touch and allows us to ignore noise when the pin is 
  // idle (HIGH) and only respond to actual touches (LOW -> HIGH)
  static bool touchActive = false;
  if (pinHigh && !touchActive) {
    // IDLE -> ACTIVE transition
    touchActive = true;
    touchEvent = true;
  }
  else if (!pinHigh && touchActive) {
    // ACTIVE -> IDLE transition
    touchActive = false;
  }
}

void setup() {
  // Set up PIN modes 
  pinMode(LED_PIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);

  // Set LED off initially
  digitalWrite(LED_PIN, HIGH); // HIGH to turn off since we're doing low-side switching

  // Disable unused modules to save power
  ADCSRA &= ~(1 << ADEN);   // manually kill ADC
  power_adc_disable();      // disable ADC clock
  ACSR |= (1 << ACD);       // misable analog comparator
  power_timer0_disable();   // Cut power to timer0
  power_timer1_disable();   // Cut power to timer1

  // Configure touch pin change interrupt
  GIMSK |= (1 << PCIE);    // Enable pin change interrupts
  PCMSK |= (1 << TOUCH_PIN);  // PB2, the touch pin, is allowed to wake us

  sei(); // enable interrupts
}

void sleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // Deepest sleep mode
  sleep_enable(); // Arm sleep
  cli();  // Disable interrupts
  sleep_bod_disable(); // disabled in fuses, but just in case
  sei();  // Re-enable interrupts
  sleep_cpu();  // Go to sleep

  //...sleeping...

  sleep_disable(); // Just woke up 
}

// Read DIP switches for sleep time
void readDIP() {

  // Read DIP switches -- only need to do this when turning ON 
  DIDR0 &= ~((1 << ADC3D) | (1 << ADC2D)); // activate input buffers
  pinMode(DIP_MSB_PIN, INPUT_PULLUP);
  pinMode(DIP_LSB_PIN, INPUT_PULLUP);
  pinMode(DIP_HOUR_PIN, INPUT_PULLUP); // production mode by default
  _delay_us(50); // stabilize pins
  
  // Invert active low switches and combine into a single value
  uint8_t bit0 = !digitalRead(DIP_LSB_PIN);
  uint8_t bit1 = !digitalRead(DIP_MSB_PIN);
  uint8_t value = (bit1 << 1) | bit0;

  // LOW (switch ON) = hours mode, HIGH/OFF = minutes mode
  bool hoursMode = !digitalRead(DIP_HOUR_PIN); 
  
  // Shut them back down immediately - disable pullups, input buffers, and set to High-Z 
  // to save power since we only need to read them once when the LED turns on.
  PORTB &= ~((1 << DIP_LSB_PIN) | (1 << DIP_MSB_PIN) | (1 << DIP_HOUR_PIN) );  // disable pull-ups
  DIDR0 |= (1 << ADC3D) | (1 << ADC2D); // disable input buffers
  pinMode(DIP_LSB_PIN, INPUT);          // back to High-Z
  pinMode(DIP_MSB_PIN, INPUT);
  pinMode(DIP_HOUR_PIN, INPUT);

  // Convert DIP value to cycles: 0b00=1, 0b01=2, 0b10=4, 0b11=8
  /// ...and then to seconds based on DEBUG mode
  uint8_t units = 1 << value;

  if (hoursMode) {
    maxSleepSeconds = units * 3600UL; // 1 hour per unit (1, 2, 4, 8 hours)
  }
  else {
    // Directly use minutes instead of hours. These will be rounded up to the 
    // nearest 8s in the main loop due to the WDT granularity.
    maxSleepSeconds = units * 120UL;  // 2 min per unit (2, 4, 8, 16 mins)
  }
}

void loop() {
  // Check for touch to toggle LED
  if (touchEvent) {
    touchEvent = false; 
    ledOn = !ledOn;

    if (ledOn) {

      // Canculate LED ON time based on DIP switches
      readDIP(); 

      // Start the Watchdog timer -- this only needs to be running when the LED is ON
      // ALso reset sleep counter and turn on LED
      sleepCounter = 0;
      MCUSR &= ~(1 << WDRF);
      WDTCR |= (1 << WDCE) | (1 << WDE);
      WDTCR = (1 << WDIE) | (1 << WDP3) | (1 << WDP0);

      // actually turn on LED
      digitalWrite(LED_PIN, LOW); // LOW to turn on since we're doing low-side switching

    } else {
      // User manually toggled OFF, so shut down WDT and turn off LED
      digitalWrite(LED_PIN, HIGH); // HIGH to turn off since we're doing low-side switching
      wdt_disable();
    }
  }

  sleep();  // go to sleep

  if ( ledOn ) {
    uint32_t counterCopy;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      counterCopy = sleepCounter;
    }

    if (counterCopy >= maxSleepSeconds) {
      ledOn = false;
      digitalWrite(LED_PIN, HIGH); // HIGH to turn off since we're doing low-side switching
      wdt_disable(); // fully disable WDT to save power while LEDis off 
    }
    else {
      // Re-enable WDT interrupt for the next 8s cycle -- necessary because the WDT 
      // interrupt bit often clears itself
      WDTCR |= (1 << WDCE) | (1 << WDIE); 
    }
  }
}
