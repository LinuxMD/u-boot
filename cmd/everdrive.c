// SPDX-License-Identifier: GPL-2.0+
/*
 */
#include <command.h>
#include <string.h>
#include <vsprintf.h>

#include <everdrive-fifo.h>

static void *fifo = (void *) 0xa130d0;

static int everdrive_disk_init(int argc, char *const argv[]);
static int everdrive_disk_ls(int argc, char *const argv[]);
static int everdrive_disk_load(int argc, char *const argv[]);

struct everdrive_sub_cmd {
	const char *name;
	int min_args;
	int max_args;
	int (*handler)(int argc, char *const argv[]);
	const char *usage;
};

static const struct everdrive_sub_cmd disk_cmds[] = {
	{
		"init",
		0, 0,
		everdrive_disk_init,
		"init\n"
		"    - Initialise the EverDrive disk interface"
	},
	{
		"ls",
		0, 1,
		everdrive_disk_ls,
		"ls [<path>]\n"
		"    - List files on the EverDrive disk (default path: /)"
	},
	{
		"load",
		2, 2,
		everdrive_disk_load,
		"load <addr> <filename>\n"
		"    - Load <filename> from the EverDrive disk to memory address <addr>"
	},
	{ NULL }   /* sentinel */
};

static int everdrive_disk_init(int argc, char *const argv[])
{
	everdrive_fifo_sendcmd(fifo, &everdrive_fifo_cmd_disk_init);

	everdrive_fifo_read_status(fifo);

	return CMD_RET_SUCCESS;
}

struct everdrive_fileinfo {
	uint32_t size;
	uint16_t date;
	uint16_t time;
	uint8_t is_dir;
	unsigned char filename[65];
	int lenlen;
};

static struct everdrive_fileinfo fileinfofs[32];

static int everdrive_dir_get_one(struct everdrive_fileinfo *fileinfo)
{
	int filenamelen;
	/* why is this called resp? I have no idea */
	uint8_t resp;

	everdrive_fifo_read_u8(fifo, &resp);
	if (resp)
		return 0;

	everdrive_fifo_read_u32(fifo, &fileinfo->size);
	everdrive_fifo_read_u16(fifo, &fileinfo->date);
	everdrive_fifo_read_u16(fifo, &fileinfo->time);
	everdrive_fifo_read_u8(fifo, &fileinfo->is_dir);
	filenamelen = everdrive_fifo_read_str(fifo, fileinfo->filename, sizeof(fileinfo->filename) - 1);
	fileinfo->filename[filenamelen] = '\0';

	fileinfo->lenlen = filenamelen;

	return 1;
}

static int everdrive_dir_get(unsigned int start, unsigned int num, struct everdrive_fileinfo *result)
{
	int i;

	everdrive_fifo_sendcmd(fifo, &everdrive_fifo_cmd_disk_f_dir_get);
	everdrive_fifo_write_u16(fifo, start);
	everdrive_fifo_write_u16(fifo, num);
	everdrive_fifo_write_u16(fifo, sizeof(result[0].filename) - 1);

	for (i = 0; i < num; i++) {
		struct everdrive_fileinfo *fi = result++;

		memset(fi, 0, sizeof(fi));
		if (!everdrive_dir_get_one(fi))
			break;
	}

	return 0;
}

static int everdrive_dir_size(void)
{
	u16 dirsize;

	everdrive_fifo_sendcmd(fifo, &everdrive_fifo_cmd_disk_f_dir_size);
	everdrive_fifo_read_u16(fifo, &dirsize);
	everdrive_fifo_read_status(fifo);

	return dirsize;
}

static int everdrive_dir_ld(const char *path)
{
	const u16 pathlen = strlen(path);
	const u8 args = 0;

	everdrive_fifo_sendcmd(fifo, &everdrive_fifo_cmd_disk_f_dir_ld);
	everdrive_fifo_write_u8(fifo, args);
	everdrive_fifo_write_str(fifo, path, pathlen);
	everdrive_fifo_read_status(fifo);

	return 0;
}

static int everdrive_disk_ls(int argc, char *const argv[])
{
	const char *path = (argc > 0) ? argv[0] : "/";
	unsigned int dirsz;
	int i;

	everdrive_dir_ld(path);
	dirsz = everdrive_dir_size();
	if (dirsz)
		everdrive_dir_get(0, dirsz, fileinfofs);

	for (i = 0; i < dirsz; i++)
		printf("xx %s %d\n", fileinfofs[i].filename, fileinfofs[i].lenlen);

	return CMD_RET_SUCCESS;
}

static const uint32_t everdrive_file_load_blksz = 512;

static int everdrive_file_read(void *dst, uint32_t amount)
{
	uint8_t resp;

	if (amount > everdrive_file_load_blksz)
		return -EINVAL;

	everdrive_fifo_sendcmd(fifo, &everdrive_fifo_cmd_disk_f_frd);
	everdrive_fifo_write_u32(fifo, amount);
	everdrive_fifo_read_u8(fifo, &resp);
	everdrive_fifo_read(fifo, dst, amount);

	return 0;
}

static int everdrive_file_available(uint64_t *result)
{
	everdrive_fifo_sendcmd(fifo, &everdrive_fifo_cmd_disk_f_avb);
	everdrive_fifo_read_u64(fifo, result);

	return 0;
}

static int everdrive_file_open(const unsigned char* path, uint8_t mode)
{
	size_t pathlen = strlen(path);

	everdrive_fifo_sendcmd(fifo, &everdrive_fifo_cmd_disk_f_fopen);
	everdrive_fifo_write_u8(fifo, mode);
	everdrive_fifo_write_str(fifo, path, pathlen);

	everdrive_fifo_read_status(fifo);

	return 0;
}

static int everdrive_file_close(void)
{
	everdrive_fifo_sendcmd(fifo, &everdrive_fifo_cmd_disk_f_fclose);
	everdrive_fifo_read_status(fifo);

	return 0;
}

static int everdrive_disk_load(int argc, char *const argv[])
{
	const char *addr_str = argv[0];
	const char *filename  = argv[1];
	unsigned long addr;
	uint64_t size;
	char *endp;

	addr = simple_strtoul(addr_str, &endp, 16);
	if (*endp != '\0') {
		printf("everdrive disk load: invalid address '%s'\n", addr_str);
		return CMD_RET_FAILURE;
	}

	everdrive_file_open(filename, EVERDRIVE_FILE_MODE_READ);
	everdrive_file_available(&size);

	printf("file size %llu\n", size);
	/* TODO: check we can actually load everything, assuming we don't care about +2GB issues for now */

	unsigned int loadpos;
	unsigned int loadsz = size;
	void *dst = (void *) addr;
	for (loadpos = 0; loadpos < loadsz; loadpos += everdrive_file_load_blksz) {
		unsigned int sz = min(loadsz - loadpos, everdrive_file_load_blksz);

		everdrive_file_read(dst, sz);
		dst += sz;

		printf("file read %u\n", sz);
	}

	everdrive_file_close();

	return CMD_RET_SUCCESS;
}

static int dispatch_sub_cmd(const struct everdrive_sub_cmd *table,
			    int argc, char *const argv[])
{
	const struct everdrive_sub_cmd *c;
	int extra_argc;

	if (argc < 1) {
		printf("Available sub-commands:\n");
		for (c = table; c->name; c++)
			printf("  %s\n", c->usage);
		return CMD_RET_USAGE;
	}

	for (c = table; c->name; c++) {
		if (strcmp(argv[0], c->name) != 0)
			continue;

		/* argv[0] is the matched sub-command name; extra args follow */
		extra_argc = argc - 1;

		if (extra_argc < c->min_args) {
			printf("everdrive: '%s' requires at least %d argument(s)\n",
			       c->name, c->min_args);
			printf("  Usage: everdrive disk %s\n", c->usage);
			return CMD_RET_USAGE;
		}
		if (c->max_args >= 0 && extra_argc > c->max_args) {
			printf("everdrive: '%s' accepts at most %d argument(s)\n",
			       c->name, c->max_args);
			printf("  Usage: everdrive disk %s\n", c->usage);
			return CMD_RET_USAGE;
		}

		return c->handler(extra_argc, argv + 1);
	}

	printf("everdrive: unknown sub-command '%s'\n", argv[0]);
	return CMD_RET_USAGE;
}

static int do_everdrive(struct cmd_tbl *cmdtp, int flag,
			int argc, char *const argv[])
{
	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	/* argv[0] = "everdrive", argv[1] = target */
	const char *target = argv[1];

	/* Remaining args passed down: argv[2..] */
	int  sub_argc = argc - 2;
	char *const *sub_argv = argv + 2;

	if (strcmp(target, "disk") == 0) {
		return dispatch_sub_cmd(disk_cmds, sub_argc, sub_argv);
	}

	printf("everdrive: unknown target '%s'\n", target);
	return CMD_RET_USAGE;
}

U_BOOT_CMD(
	everdrive, CONFIG_SYS_MAXARGS, 0, do_everdrive,
	"EverDrive cartridge commands",
	"disk init\n"
	"    - Initialise the EverDrive SD/disk interface\n"
	"everdrive disk ls [<path>]\n"
	"    - List directory contents on the EverDrive disk\n"
	"everdrive disk load <addr> <filename>\n"
	"    - Load a file from the EverDrive disk into RAM\n"
);
