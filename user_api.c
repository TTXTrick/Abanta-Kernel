/* user_api.c - example user program that uses host ABI
   Build: gcc -c -fPIC -ffreestanding -o user_api.o user_api.c
          ld -shared -o user_api.elf user_api.o
*/

extern struct abanta_host_api *abanta_host_api;

/* an example function exported */
void user_func(void) {
    if (abanta_host_api && abanta_host_api->print_utf16) {
        abanta_host_api->print_utf16(L"Hello from user_func via host_api->print_utf16()\n");
    }
}

/* entry called by loader: receives host_api pointer */
void _start(struct abanta_host_api *api) {
    /* print using host_api (arg) */
    if (api && api->print_utf16) api->print_utf16(L"Hello from user (entry with api arg)\n");

    /* also call user_func which uses global abanta_host_api symbol */
    user_func();

    /* return to loader */
    return;
}
