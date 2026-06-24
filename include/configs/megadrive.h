#define CFG_SYS_INIT_RAM_ADDR 0

/*
 * We want to map the save SRAM into the last 512KB
 * slot so the most RAM we can map into the cart address
 * space is 3.5MB - note, I disabled this for now as we need the RAM..
 */
#define CFG_SYS_INIT_RAM_SIZE 0x380000

#define CFG_EXTRA_ENV_SETTINGS \
		"bootcmd=everdrive disk init;" \
		"everdrive disk load 0x1000 vmlinux.lz4;" \
		"unlz4 0x1000 0x180000 0x180000;" \
		"bootelf -p 0x180000\0" \
		"stdout=serial,vidconsole\0" \
		"autostart=yes\0" \
		"bootargs=console=ttyVDP0 console=ttyED0 root=/dev/edblk -- smolinit.getty=/dev/ttyED0 smolinit.getty=/dev/ttyVDP0 smolinit.hostname=md\0"
