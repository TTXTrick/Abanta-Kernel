# Abanta Kernel
Just an open-source PC kernel

Before doing anything, there are a few prerequisites. Run ```sudo apt update && sudo apt install build-essential nasm grub-mkrescue xorriso qemu-system-x86```

# Run with QEMU locally
In order to run this in QEMU,
Clone this GitHub repository.
In your terminal, run ```cd Abanta-Kernel```.
Now, to build abanta.efi, run ```make```, then ```make iso```. To clean, run ```make clean```.
Run the kernel in QEMU with ```make run```.

You are also free to create your own full OS distribution of Abanta.

<img width="723" height="400" alt="image" src="https://github.com/user-attachments/assets/b182718e-206a-42f8-b58c-47a8134bfa47" />
