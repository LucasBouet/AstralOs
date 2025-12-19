#include "keyboard_map.h"

/* VGA text mode constants */
#define LINES 25
#define COLUMNS_IN_LINE 80
#define BYTES_FOR_EACH_ELEMENT 2
#define SCREENSIZE (BYTES_FOR_EACH_ELEMENT * COLUMNS_IN_LINE * LINES)

/* Keyboard */
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define ENTER_KEY_CODE 0x1C

/* IDT */
#define IDT_SIZE 256
#define INTERRUPT_GATE 0x8E
#define KERNEL_CODE_SEGMENT_OFFSET 0x08

#define MAX_COMMAND_LEN 100

extern void keyboard_handler(void);
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);

unsigned int current_loc = 0;
unsigned int prompt_loc = 0;
char *vidptr = (char*)0xB8000;

char command_buffer[MAX_COMMAND_LEN];
unsigned int command_len = 0;

/* ================= IDT ================= */

struct IDT_entry {
    unsigned short offset_lowerbits;
    unsigned short selector;
    unsigned char zero;
    unsigned char type_attr;
    unsigned short offset_higherbits;
};

struct IDT_entry IDT[IDT_SIZE];

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void idt_init(void)
{
    unsigned long keyboard_address = (unsigned long)keyboard_handler;
    unsigned long idt_ptr[2];

    IDT[0x21].offset_lowerbits = keyboard_address & 0xFFFF;
    IDT[0x21].selector = KERNEL_CODE_SEGMENT_OFFSET;
    IDT[0x21].zero = 0;
    IDT[0x21].type_attr = INTERRUPT_GATE;
    IDT[0x21].offset_higherbits = (keyboard_address >> 16) & 0xFFFF;

    /* PIC remap */
    write_port(0x20, 0x11);
    write_port(0xA0, 0x11);
    write_port(0x21, 0x20);
    write_port(0xA1, 0x28);
    write_port(0x21, 0x00);
    write_port(0xA1, 0x00);
    write_port(0x21, 0x01);
    write_port(0xA1, 0x01);

    /* Mask all except keyboard */
    write_port(0x21, 0xFD);
    write_port(0xA1, 0xFF);

    unsigned long idt_address = (unsigned long)IDT;
    idt_ptr[0] = (sizeof(struct IDT_entry) * IDT_SIZE) | ((idt_address & 0xFFFF) << 16);
    idt_ptr[1] = idt_address >> 16;

    load_idt(idt_ptr);
}

/* ================= VGA ================= */

void scroll_screen(void)
{
    unsigned int line_size = COLUMNS_IN_LINE * BYTES_FOR_EACH_ELEMENT;

    for (unsigned int i = 0; i < (LINES - 1) * line_size; i++)
        vidptr[i] = vidptr[i + line_size];

    for (unsigned int i = (LINES - 1) * line_size; i < SCREENSIZE; i += 2) {
        vidptr[i] = ' ';
        vidptr[i + 1] = 0x07;
    }

    current_loc = (LINES - 1) * line_size;
}

void clear_screen(void)
{
    for (unsigned int i = 0; i < SCREENSIZE; i += 2) {
        vidptr[i] = ' ';
        vidptr[i + 1] = 0x07;
    }
    current_loc = 0;
}

void kprint(const char *str)
{
    while (*str) {
        if (current_loc >= SCREENSIZE)
            scroll_screen();

        vidptr[current_loc++] = *str++;
        vidptr[current_loc++] = 0x07;
    }
}

void kprint_newline(void)
{
    unsigned int line_size = COLUMNS_IN_LINE * BYTES_FOR_EACH_ELEMENT;
    current_loc += (line_size - (current_loc % line_size));

    if (current_loc >= SCREENSIZE)
        scroll_screen();
}

/* ================= SHELL ================= */

void print_prompt(void)
{
    kprint("> ");
    prompt_loc = current_loc;
}

void execute_command(void)
{
    command_buffer[command_len] = '\0';

    if (strcmp(command_buffer, "clear") == 0 || strcmp(command_buffer, "cls") == 0) {
        clear_screen();
    }

    else if (strcmp(command_buffer, "ping") == 0) {
        kprint_newline();
        kprint("pong!");
    }

	else if (strcmp(command_buffer, "help") == 0) {
		kprint_newline();
		kprint("clear | cls | help | osinfo | ping");
	}

	else if (strcmp(command_buffer, "osinfo") == 0) {
		kprint_newline();
		kprint("AstralOs V0.0.1 HomeMade Kernel, booted succefully, Credits : Lucas Bouet");
	}


    else if (command_len != 0) {
        kprint_newline();
        kprint("Unknown command: ");
        kprint(command_buffer);
    }

    command_len = 0;
    kprint_newline();
    print_prompt();
}

/* ================= KEYBOARD ================= */

void keyboard_handler_main(void)
{
    write_port(0x20, 0x20); // EOI

    if (!(read_port(KEYBOARD_STATUS_PORT) & 1))
        return;

    unsigned char keycode = read_port(KEYBOARD_DATA_PORT);

    /* Ignore key releases */
    if (keycode & 0x80)
        return;

    if (keycode == ENTER_KEY_CODE) {
        execute_command();
        return;
    }

    char c = keyboard_map[keycode];
    if (c == 0)
        return;

    /* BACKSPACE */
    if (c == '\b') {
        if (command_len > 0 && current_loc > prompt_loc) {
            command_len--;
            current_loc -= 2;
            vidptr[current_loc] = ' ';
            vidptr[current_loc + 1] = 0x07;
        }
        return;
    }

    /* NORMAL CHARACTER */
    if (command_len < MAX_COMMAND_LEN - 1) {
        command_buffer[command_len++] = c;

        if (current_loc >= SCREENSIZE)
            scroll_screen();

        vidptr[current_loc++] = c;
        vidptr[current_loc++] = 0x07;
    }
}

/* ================= KERNEL ================= */

void kmain(void)
{
    clear_screen();
    kprint("Kernel Booted.");
    kprint_newline();
    print_prompt();

    idt_init();

    while (1);
}
