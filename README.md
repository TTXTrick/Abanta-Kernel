# Abanta Kernel
Just an open-source PC kernel

First, there are a few install dependencies. Run ```sudo apt update
sudo apt install gnu-efi gnu-efi-dev```

# Run with QEMU locally
In order to run this in QEMU,
Clone this GitHub repository.
In your terminal, run ```cd Abanta-Kernel```.
Now, to build abanta.efi, run ```make build```, them ```make```. To clean, run ```make clean```.
Run the kernel in QEMU with ```make run```.

You are also free to create your own full OS distribution of Abanta.
