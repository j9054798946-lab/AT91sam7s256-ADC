/*
 * Version: 2012
 * AT91SAM7S256 - LED blink + UART
 * Настройка правильной скорости UART
 */

#include "AT91SAM7S256.h"
#include <intrinsics.h>

#define LED_PIN (1 << 7)

volatile unsigned int led_state = 0;
volatile unsigned int pit_counter = 0;

// Определяем реальный MCK из измерений PIT
// PIT Clock = 86727 / 0.050 = 1734540 Hz
// MCK = PIT Clock * 16 = 27752640 Hz
#define MCK 27752640UL

// ---------------- USART0 ----------------
void usart0_init(unsigned int baud)
{
    unsigned int cd;

    // Включаем клок USART0
    AT91C_BASE_PMC->PMC_PCER = (1 << AT91C_ID_US0);

    // Назначаем пины PA5=TXD0 и PA6=RXD0 как Peripheral A
    AT91C_BASE_PIOA->PIO_PDR = (1<<5) | (1<<6);
    AT91C_BASE_PIOA->PIO_ASR = (1<<5) | (1<<6);

    // Reset and disable TX/RX
    AT91C_BASE_US0->US_CR = AT91C_US_RSTRX | AT91C_US_RSTTX
                          | AT91C_US_RXDIS | AT91C_US_TXDIS;

    // Mode: асинхрон, CLKS=MCK, 8N1
    AT91C_BASE_US0->US_MR = AT91C_US_USMODE_NORMAL
                          | AT91C_US_CLKS_CLOCK
                          | AT91C_US_CHRL_8_BITS
                          | AT91C_US_PAR_NONE
                          | AT91C_US_NBSTOP_1_BIT;

    // Правильный расчет BRGR с реальным MCK
    cd = (MCK + (baud * 8)) / (16 * baud);
    AT91C_BASE_US0->US_BRGR = cd;

    // Enable TX/RX
    AT91C_BASE_US0->US_CR = AT91C_US_TXEN | AT91C_US_RXEN;
}

// отправка одного символа
void usart0_putc(char c)
{
    while (!(AT91C_BASE_US0->US_CSR & AT91C_US_TXRDY));
    AT91C_BASE_US0->US_THR = c;
}

// ---------------- PIT ----------------
void PIT_Handler(void)
{
    volatile unsigned int dummy = AT91C_BASE_PITC->PITC_PIVR; // clear int flag
    (void)dummy;

    pit_counter++;
    
    // Переключаем LED каждые 10 прерываний (500ms)
    if (pit_counter >= 10) {
        pit_counter = 0;
        
        if (led_state) {
            AT91C_BASE_PIOA->PIO_SODR = LED_PIN; // LED OFF
            led_state = 0;
            usart0_putc('0');   // отправляем ASCII '0' = 0x30
        } else {
            AT91C_BASE_PIOA->PIO_CODR = LED_PIN; // LED ON
            led_state = 1;
            usart0_putc('1');   // отправляем ASCII '1' = 0x31
        }
    }
}

__irq void IRQ_Handler(void)
{
    unsigned sr = AT91C_BASE_PITC->PITC_PISR;
    if (sr & AT91C_PITC_PITS) {
        PIT_Handler();
    }
    AT91C_BASE_AIC->AIC_EOICR = 0;
}

// ---------------- MAIN ----------------
int main(void)
{
    // отключаем watchdog
    AT91C_BASE_WDTC->WDTC_WDMR = AT91C_WDTC_WDDIS;

    // Инициализация LED
    AT91C_BASE_PMC->PMC_PCER = (1 << AT91C_ID_PIOA);
    AT91C_BASE_PIOA->PIO_PER = LED_PIN;
    AT91C_BASE_PIOA->PIO_OER = LED_PIN;
    AT91C_BASE_PIOA->PIO_SODR = LED_PIN; // OFF по умолчанию

    // AIC: разрешаем SYS-прерывания (PIT)
    AT91C_BASE_AIC->AIC_IDCR = (1 << AT91C_ID_SYS);
    AT91C_BASE_AIC->AIC_ICCR = (1 << AT91C_ID_SYS);
    AT91C_BASE_AIC->AIC_IECR = (1 << AT91C_ID_SYS);

    // Точное значение PIT для 50ms
    AT91C_BASE_PITC->PITC_PIMR = AT91C_PITC_PITEN
                               | AT91C_PITC_PITIEN
                               | 86727;

    // Инициализация USART0 с правильным MCK
    usart0_init(115200);
    
    // Тестовая отправка при старте
    usart0_putc('T');
    usart0_putc('E');
    usart0_putc('S');
    usart0_putc('T');
    usart0_putc('\r');
    usart0_putc('\n');
    
    __enable_interrupt();

    while (1) {
        // всё в прерываниях (LED+UART)
    }
}