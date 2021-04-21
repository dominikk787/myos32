OUTPUT_ARCH("i386")
ENTRY(low_start) 
SECTIONS { 
    . = 1M;
    _code = .;
    .multiboot : {
        KEEP(*(.multiboot.data)) 
        *(.multiboot.text)
    }
    . += 0xC0000000;
    .text : AT (ADDR (.text) - 0xC0000000) { 
        *(.text) 
        *(.text.*) 
    } 
    . = ALIGN (4K);
    .data : AT (ADDR (.data) - 0xC0000000) { 
        *(.rodata)
        *(.rodata.*)
        *(.data) 
        *(.data.*) 
    } 
    _edata = . - 0xC0000000;
    .bss : /*AT (ADDR (.bss) - 0xC0000000)*/ { 
        *(.bss) 
        *(.bss.*) 
        *(.COMMON)
    } 
    . = ALIGN (4K);
    _end = . - 0xC0000000; 
}