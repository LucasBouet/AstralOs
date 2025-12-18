
#### Build commands ####
```
nasm -f elf32 kernel.asm -o kasm.o
```
```
gcc -m32 -c kernel.c -o kc.o
```
```
ld -m elf_i386 -T link.ld -o kernel kasm.o kc.o
```

If you get the following error message:
```
kc.o: In function `idt_init':
kernel.c:(.text+0x129): undefined reference to `__stack_chk_fail'
```

compile with the `-fno-stack-protector` option:
```
gcc -fno-stack-protector -m32 -c kernel.c -o bin/kc.o
```

#### Test on emulator ####
```
qemu-system-i386 -kernel kernel
```
