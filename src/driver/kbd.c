#include "kernel.h"
#include "io.h"
#include "intr.h"
#include "driver_inout.h"

static drv_in_t *kbddrv;

static int read_kbd(void) { 
    uint32_t timeout; 
    uint8_t stat, data; 
    for(timeout = 500000L; timeout != 0; timeout--) { 
        stat = inportb(0x64); 
        // czekaj gdy bufor klawiatury jest pełny
        if(stat & 0x01) { 
            data = inportb(0x60); 
            // pętla, gdt błąd parzystości, lub koniec czasu oczekiwania 
            if((stat & 0xC0) == 0) return data; 
        } 
    } 
    return -1; 
} 

static void write_kbd(unsigned adr, unsigned data) { 
    unsigned long timeout; 
    unsigned stat; 
    for(timeout = 500000L; timeout != 0; timeout--) { 
        stat = inportb(0x64); 
        // czekaj gdy bufor klawiatury nie zrobi się pusty
        if((stat & 0x02) == 0) break; 
    } 
    if(timeout == 0) { 
        // print_str("write_kbd: timeout\n"); 
        return; 
    } 
    outportb(adr, data); 
} 

#define RAW1_LEFT_CTRL 0x1D 
#define RAW1_LEFT_SHIFT 0x2A 
#define RAW1_CAPS_LOCK 0x3A 
#define RAW1_LEFT_ALT 0x38 
#define RAW1_RIGHT_ALT 0x38 
#define RAW1_RIGHT_CTRL 0x1D 
#define RAW1_RIGHT_SHIFT 0x36 
#define RAW1_SCROLL_LOCK 0x46 
#define RAW1_NUM_LOCK 0x45 
#define RAW1_DEL 0x53 

#define KBD_META_CAPS 0x1000 
#define KBD_META_NUM 0x2000 
#define KBD_META_SCRL 0x4000 

uint16_t set1_scancode_to_ascii(drv_in_t *drv, uint16_t code) { 
    drv_kbd_data_t *data = drv->drv_data;
    static const unsigned char map[] = { 
        0, 0x1B, '1', '2', '3', '4', '5', '6', // 00
        '7', '8', '9', '0', '-', '=', '\b', '\t', // 08
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', // 10
            // 1Dh to lewy Ctrl 
        'o', 'p', '[', ']', '\n', 0, 'a', 's', // 18
        'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', // 20
            // 2Ah to lewy Shift 
        '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', // 28
            // 36h to prawy Shift 
        'b', 'n', 'm', ',', '.', '/', 0, 0, // 30
            // 38h to lewy Alt, 3Ah to Caps Lock
        0, ' ', 0, IN_KEY_F1, IN_KEY_F2, IN_KEY_F3, IN_KEY_F4, IN_KEY_F5, // 38 
            // 45h to Num Lock, 46h to Scroll Lock
        IN_KEY_F6, IN_KEY_F7, IN_KEY_F8, IN_KEY_F9, IN_KEY_F10,0, 0,IN_KEY_HOME, // 40
        IN_KEY_UP, IN_KEY_PGUP,'-', IN_KEY_LFT,'5', IN_KEY_RT, '+', IN_KEY_END, // 48
        IN_KEY_DN, IN_KEY_PGDN,IN_KEY_INS,IN_KEY_DEL,0, 0, 0, IN_KEY_F11, // 50
        IN_KEY_F12 }; // 58
    static const unsigned char shift_map[] = { 
        0, 0x1B, '!', '@', '#', '$', '%', '^', // 00
        '&', '*', '(', ')', '_', '+', '\b', '\t', // 08
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', // 10
        'O', 'P', '{', '}', '\n', 0, 'A', 'S', // 18
        'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', // 20
        '"', '~', 0, '|', 'Z', 'X', 'C', 'V', // 28
        'B', 'N', 'M', '<', '>', '?', 0, 0, // 30
        0, ' ', 0, IN_KEY_F1, IN_KEY_F2, IN_KEY_F3, IN_KEY_F4, IN_KEY_F5, // 38 
        IN_KEY_F6, IN_KEY_F7, IN_KEY_F8, IN_KEY_F9, IN_KEY_F10,0, 0, IN_KEY_HOME, // 40
        IN_KEY_UP, IN_KEY_PGUP,'-', IN_KEY_LFT,'5', IN_KEY_RT, '+', IN_KEY_END, // 48
        IN_KEY_DN, IN_KEY_PGDN,IN_KEY_INS,IN_KEY_DEL,0, 0, 0, IN_KEY_F11, // 50
        IN_KEY_F12 }; // 58
    uint16_t temp; 
    // sprawdź czy kod break (np. gdy klawisz został zwolniony) 
    if(code >= 0x80) { 
        data->saw_break_code = 1; 
        code &= 0x7F; 
    } 
    // kod break, które nas na razie interesują to Ctrl, Shift, Alt
    if(data->saw_break_code) { 
        if(code == RAW1_LEFT_ALT || code == RAW1_RIGHT_ALT) 
            data->kbd_status &= ~IN_SPECIAL_ALT; 
        else if(code == RAW1_LEFT_CTRL || code == RAW1_RIGHT_CTRL) 
            data->kbd_status &= ~IN_SPECIAL_CTRL; 
        else if(code == RAW1_LEFT_SHIFT || code == RAW1_RIGHT_SHIFT) 
            data->kbd_status &= ~IN_SPECIAL_SHIFT; 
        data->saw_break_code = 0; 
        return -1; 
    } 
    // jeśli to kod make: sprawdź klawisze "meta" podobnie jak powyżej
    if(code == RAW1_LEFT_ALT || code == RAW1_RIGHT_ALT) { 
        data->kbd_status |= IN_SPECIAL_ALT; 
        return -1; 
    } 
    if(code == RAW1_LEFT_CTRL || code == RAW1_RIGHT_CTRL) { 
        data->kbd_status |= IN_SPECIAL_CTRL; 
        return -1; 
    } 
    if(code == RAW1_LEFT_SHIFT || code == RAW1_RIGHT_SHIFT) { 
        data->kbd_status |= IN_SPECIAL_SHIFT; 
        return -1; 
    } 
    // Scroll Lock, Num Lock, i Caps Lock ustawiają diody LED. 
    if(code == RAW1_SCROLL_LOCK) { 
        data->kbd_status ^= KBD_META_SCRL; 
        goto LEDS; 
    } 
    if(code == RAW1_NUM_LOCK) { 
        data->kbd_status ^= KBD_META_NUM; 
        goto LEDS; 
    } 
    if(code == RAW1_CAPS_LOCK) { 
        data->kbd_status ^= KBD_META_CAPS; 
LEDS:             
        write_kbd(0x60, 0xED);      // komenda "set LEDS" 
        temp = 0; 
        if(data->kbd_status & KBD_META_SCRL) 
            temp |= 1; 
        if(data->kbd_status & KBD_META_NUM) 
            temp |= 2; 
        if(data->kbd_status & KBD_META_CAPS) 
            temp |= 4; 
        write_kbd(0x60, temp); 
        return -1; 
    } 
    // brak konwersji, gdy Alt jest naciśnięty
    // if(kbd_status & KBD_META_ALT) 
    //     return code; 
    // konwertuj A-Z[\]^_ na kody sterowania 
    // if((data->kbd_status & IN_SPECIAL_ANY) == IN_SPECIAL_CTRL) { 
    //     if(code >= sizeof(map) / sizeof(map[0])) 
    //         return -1; 
    //     temp = map[code]; 
    //     if(temp >= 'a' && temp <= 'z') 
    //         return temp - 'a'; 
    //     if(temp >= '[' && temp <= '_') 
    //         return temp - '[' + 0x1B; 
    // } 
    // konwertuj kod skanowania na kod ASCII 
    if(data->kbd_status & IN_SPECIAL_SHIFT) { 
        // ignoruj niepoprawne kody 
        if(code >= sizeof(shift_map) / sizeof(shift_map[0])) return -1; 
        temp = shift_map[code]; 
        if(temp == 0) return -1; 
        // caps lock? 
        if((data->kbd_status & KBD_META_CAPS) || ((data->kbd_status & IN_SPECIAL_ANY) != IN_SPECIAL_SHIFT)) { 
            if(temp >= 'A' && temp <= 'Z') 
                temp = map[code]; 
        } 
    } else { 
        if(code >= sizeof(map) / sizeof(map[0])) 
            return -1; 
        temp = map[code]; 
        if(temp == 0) 
            return -1; 
        if((data->kbd_status & KBD_META_CAPS) && ((data->kbd_status & IN_SPECIAL_ANY) == 0)) { 
            if(temp >= 'a' && temp <= 'z') 
                temp = shift_map[code]; 
        } 
    } 
    return temp | (data->kbd_status & IN_SPECIAL_ANY); 
} 

void do_irq1(void) { 
    int16_t key = set1_scancode_to_ascii(kbddrv, inportb(0x60)); 
    if(key >= 0) { 
        // if((key & (IN_SPECIAL_ALT | IN_SPECIAL_CTRL)) == 0 && (key & 0xFF) >= ' ' && (key & 0xFF) < IN_KEY_F1) keybord_in(kbd_data, key);
        // else keybord_ctrl(kbd_data, key & 0xFF, key & 0xFF00);
        kbddrv->in_clb(kbddrv, key & 0xFF, key & 0xFF00);
    } 
}

void drv_kbd_init(drv_in_t *drv) { 
    drv_kbd_data_t *data = drv->drv_data;
    data->kbd_status = 0;
    data->saw_break_code = 0;
    kbddrv = drv;
    set_intr_gate(0x21, &irq1); 
    enable_irq(1); 
}