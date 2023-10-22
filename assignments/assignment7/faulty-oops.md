# Kernel OOPS Analysis

This section analyzes the faulty kernel module present in the buildroot image of 
[Assignment 5 repository](https://github.com/cu-ecen-aeld/assignment-5-aasu8675). 

The buildroot repository uses the [Assignment 7 repository](https://github.com/cu-ecen-aeld/assignment-7-aasu8675) to pull source code for linux device drivers.

Given below is the command used to test the faulty module:

``` echo "hello_world" > /dev/faulty ```

The output of this command is shown below:

```
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=00000000420b9000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 158 Comm: sh Tainted: G           O      5.15.18 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xa8/0x2b0
sp : ffffffc008d23d80
x29: ffffffc008d23d80 x28: ffffff80020d0000 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040001000 x22: 000000000000000c x21: 000000556e952770
x20: 000000556e952770 x19: ffffff8002086c00 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006f7000 x3 : ffffffc008d23df0
x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x14/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x40/0xa0
 el0_svc+0x20/0x60
 el0t_64_sync_handler+0xe8/0xf0
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 12ffeb2ca24d0303 ]---
```

* The first line 'Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000' shows that there is an error in the kernel due to a NULL pointer dereference.

* The line 'pc : faulty_write+0x14/0x20 [faulty]' shows the program counter status when the kernel error occurs. The module faulty was being executed at the time of the crash, which consists of a function called "faulty_write". 

## faulty_write Code Analysis

Given below is the code snippet of the faulty_write function present in the [Assignment 7 repository](https://github.com/cu-ecen-aeld/assignment-7-aasu8675/blob/master/misc-modules/faulty.c)

```
ssize_t faulty_write (struct file *filp, const char __user *buf, size_t count,
		loff_t *pos)
{
	/* make a simple fault by dereferencing a NULL pointer */
	*(int *)0 = 0;
	return 0;
}
```

* The faulty_write function dereferences a NULL pointer which causes the kernel error.

## Cross-referencing with cross compiled objdump

By using the following command from the path "assignment-5-aasu8675/buildroot/output/host/bin" we can observe the disaessembly:

```
objdump -S /home/aamir/AESD/assignment-7-aasu8675/misc-modules/faulty.ko
```

The full disassembly of the faulty module can be found in the [faulty-oops-disassembly.txt](./faulty-oops-disassembly.txt)



* The error occurs in the faulty_write section which is of interest while debugging the issue: the disassembly of the faulty write section is shown below:

```
Disassembly of section .text:

0000000000000000 <faulty_write>:
	return ret;
}

ssize_t faulty_write (struct file *filp, const char __user *buf, size_t count,
		loff_t *pos)
{
   0:	e8 00 00 00 00       	callq  5 <faulty_write+0x5>
   5:	55                   	push   %rbp
	/* make a simple fault by dereferencing a NULL pointer */
	*(int *)0 = 0;
	return 0;
}
   6:	31 c0                	xor    %eax,%eax
	*(int *)0 = 0;
   8:	c7 04 25 00 00 00 00 	movl   $0x0,0x0
   f:	00 00 00 00 
{
  13:	48 89 e5             	mov    %rsp,%rbp
}
  16:	5d                   	pop    %rbp
  17:	e9 00 00 00 00       	jmpq   1c <faulty_write+0x1c>
  1c:	0f 1f 40 00          	nopl   0x0(%rax)
```

1. The function gets called and moves to #0x5 which pushes the base pointer
2. At address 0x6, eax register is cleared by using the xor operation
3. Address 0x8 tries to move the value 0x0 to an address of 0x0 which causes the null dereferencing
4. The base pointer is then moved to the stack pointer at address 0x13
5. At 0x16 the base pointer is popped
6. At 0x17, a jmpq exists which makes a jump to 0x1c
7. The nop at 0x1c serves alignment purpose in the pipeline 

Conclusion: The movl instruction performs the dereferencing of null pointer by writing at the address. But the program crashes later at 0x14. As movl takes more cycles to execute, there is no dependency between the next instruction "mov %rsp,%rbp" which might be executing in the pipeline. Hence, the fault probably occurs at a later stage.
