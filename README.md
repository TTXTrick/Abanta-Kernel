# Abanta Kernel
Just an open-source PC kernel

First, there are a few install dependencies. Run ```sudo apt update
sudo apt install -y gcc binutils qemu-system-x86 mtools ovmf make```

# Run with QEMU locally
In order to run this in QEMU,
Clone this GitHub repository.
In your terminal, run ```cd Abanta-Kernel```.
Then, clone the edk2 headers into your Abanta folder: ```git clone https://github.com/tianocore/edk2```.
Now, to build abanta.efi, run ```make```. To clean, run ```make clean```.
Run the kernel in QEMU with ```make run```.

You are also free to create your own full OS distribution of Abanta.
