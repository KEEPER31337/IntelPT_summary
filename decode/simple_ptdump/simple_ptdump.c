#include "pt_cpu.h"
#include "pt_last_ip.h"

#include "intel-pt.h"

#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#if defined(_MSC_VER) && (_MSC_VER < 1900)
#define snprintf _snprintf_c
#endif

struct ptdump_options {
	/* Show the current offset in the trace stream. */
	uint32_t show_offset : 1;

	/* Show raw packet bytes. */
	uint32_t show_raw_bytes : 1;

	/* Show last IP for packets with IP payloads. */
	uint32_t show_last_ip : 1;
};

struct ptdump_buffer {
	/* The trace offset. */
	char offset[17];

	/* The raw packet bytes. */
	char raw[33];

	/* The packet opcode. */
	char opcode[10];

	union {
		/* The standard packet payload. */
		char standard[25];

		/* An extended packet payload. */
		char extended[48];
	} payload;

	/* The tracking information. */
	struct {
		/* The tracking identifier. */
		char id[5];

		/* The tracking information. */
		char payload[17];
	} tracking;

	/* A flag telling whether an extended payload is used. */
	uint32_t use_ext_payload : 1;

	/* A flag telling whether to skip printing this buffer. */
	uint32_t skip : 1;
};

struct ptdump_tracking {
	/* Track last-ip. */
	struct pt_last_ip last_ip;

	/* The last estimated TSC. */
	uint64_t tsc;

	/* The last calibration value. */
	uint64_t fcr;

	/* Header vs. normal decode.  Set if decoding PSB+. */
	uint32_t in_header : 1;
};

static int usage(const char *name) {
	fprintf(stderr, "%s: [<options>] <ptfile>.  Use --help or -h for help.\n", name);
	return -1;
}

static int no_file_error(const char *name) {
	fprintf(stderr, "%s: No processor trace file specified.\n", name);
	return -1;
}

static int unknown_option_error(const char *arg, const char *name) {
	fprintf(stderr, "%s: unknown option: %s.\n", name, arg);
	return -1;
}

static int help(const char *name) {
	printf("usage: %s [<options>] <ptfile>[:<from>[-<to>]\n\n", name);
	printf("options:\n");
	printf("  --help|-h                 this text.\n");
	printf("  --raw                     show raw packet bytes.\n");
	printf("  <ptfile>[:<from>[-<to>]]  load the processor trace data from <ptfile>;\n");

	return 1;
}

/* Parse range options
 *
 * Change string type to int type using 'strtoull'
 */
static int parse_range(const char *arg, uint64_t *begin, uint64_t *end) {
	char *rest;

	if (!arg || !*arg)
		return 0;

	errno = 0;
	*begin = strtoull(arg, &rest, 0);
	if (errno)
		return -1;

	if (!*rest)
		return 1;

	if (*rest != '-')
		return -1;

	*end = strtoull(rest + 1, &rest, 0);
	if (errno || *rest)
		return -1;

	return 2;
}

/* Preprocessing filename
 *
 * If filename contains offset/range options, split argument into filename, offset, range
 * If no offset / range, set 0.
 */
static int preprocess_filename(char *filename, uint64_t *offset, uint64_t *size) {
	uint64_t begin, end;
	char *range;
	int parts;

	if (!filename || !offset || !size)
		return -pte_internal;

	/* Check if filenmae has ':' from end of filename */
	range = strrchr(filename, ':');
	if (!range)	{
		*offset = 0ull;
		*size = 0ull;

		return 0;
	}

	/* Parse range options(':from', '-to') without suffix(':', '-')
	 *
	 * if parts <= 0 : No range options
	 * if parts == 1 : Only 'from' options -> begin(offset)
	 * if parts == 2 : Both 'from', 'to' options -> begin(offset), end
	 */
	parts = parse_range(range + 1, &begin, &end);
	if (parts <= 0)	{
		*offset = 0ull;
		*size = 0ull;

		return 0;
	}

	if (parts == 1)	{
		*offset = begin;
		*size = 0ull;

		*range = 0;

		return 0;
	}

	if (parts == 2)	{
		if (end <= begin)
			return -pte_invalid;

		*offset = begin;
		*size = end - begin;

		*range = 0;

		return 0;
	}

	return -pte_internal;
}

static int load_file(uint8_t **buffer, size_t *psize, const char *filename,
					 uint64_t offset, uint64_t size, const char *prog) {
	uint8_t *content;
	size_t read;
	FILE *file;
	long fsize, begin, end;
	int errcode;

	if (!buffer || !psize || !filename || !prog) {
		fprintf(stderr, "%s: internal error.\n", prog ? prog : "");
		return -1;
	}

	errno = 0;
	file = fopen(filename, "rb");
	if (!file) {
		fprintf(stderr, "%s: failed to open %s: %d.\n",	prog, filename, errno);
		return -1;
	}

	errcode = fseek(file, 0, SEEK_END);
	if (errcode) {
		fprintf(stderr, "%s: failed to determine size of %s: %d.\n", prog, filename, errno);
		goto err_file;
	}

	fsize = ftell(file);
	if (fsize < 0) {
		fprintf(stderr, "%s: failed to determine size of %s: %d.\n", prog, filename, errno);
		goto err_file;
	}

	begin = (long)offset;
	if (((uint64_t)begin != offset) || (fsize <= begin)) {
		fprintf(stderr, "%s: bad offset 0x%" PRIx64 " into %s.\n", prog, offset, filename);
		goto err_file;
	}

	end = fsize;
	if (size) {
		uint64_t range_end;

		range_end = offset + size;
		if ((uint64_t)end < range_end) {
			fprintf(stderr, "%s: bad range 0x%" PRIx64 " in %s.\n",	prog, range_end, filename);
			goto err_file;
		}

		end = (long)range_end;
	}

	fsize = end - begin;

	content = malloc((size_t)fsize);
	if (!content) {
		fprintf(stderr, "%s: failed to allocated memory %s.\n",	prog, filename);
		goto err_file;
	}

	errcode = fseek(file, begin, SEEK_SET);
	if (errcode) {
		fprintf(stderr, "%s: failed to load %s: %d.\n",	prog, filename, errno);
		goto err_content;
	}

	read = fread(content, (size_t)fsize, 1u, file);
	if (read != 1) {
		fprintf(stderr, "%s: failed to load %s: %d.\n",	prog, filename, errno);
		goto err_content;
	}

	fclose(file);

	*buffer = content;
	*psize = (size_t)fsize;

	return 0;

err_content:
	free(content);

err_file:
	fclose(file);
	return -1;
}

static int load_pt(struct pt_config *config, const char *filename,
				   uint64_t foffset, uint64_t fsize, const char *prog) {
	uint8_t *buffer;
	size_t size;
	int errcode;

	errcode = load_file(&buffer, &size, filename, foffset, fsize, prog);
	if (errcode < 0)
		return errcode;

	config->begin = buffer;
	config->end = buffer + size;

	return 0;
}

static int diag(const char *errstr, uint64_t offset, int errcode) {
	if (errcode)
		printf("[%" PRIx64 ": %s: %s]\n", offset, errstr, pt_errstr(pt_errcode(errcode)));
	else
		printf("[%" PRIx64 ": %s]\n", offset, errstr);

	return errcode;
}

static void ptdump_tracking_init(struct ptdump_tracking *tracking) {
	if (!tracking)
		return;

	pt_last_ip_init(&tracking->last_ip);

	tracking->tsc = 0ull;
	tracking->fcr = 0ull;
	tracking->in_header = 0;
}

static void ptdump_tracking_reset(struct ptdump_tracking *tracking)
{
	if (!tracking)
		return;

	pt_last_ip_init(&tracking->last_ip);

	tracking->tsc = 0ull;
	tracking->fcr = 0ull;
	tracking->in_header = 0;
}

static void ptdump_tracking_fini(struct ptdump_tracking *tracking) {
	if (!tracking)
		return;
}

#define print_field(field, ...)                      \
	do {                                             \
		/* Avoid partial overwrites. */              \
		memset(field, 0, sizeof(field));             \
		snprintf(field, sizeof(field), __VA_ARGS__); \
	} while (0)

static int print_buffer(struct ptdump_buffer *buffer, uint64_t offset,
						const struct ptdump_options *options) {
	const char *sep;

	if (!buffer)
		return diag("error printing buffer", offset, -pte_internal);

	if (buffer->skip)
		return 0;

	/* Make sure the first column starts at the beginning of the line - no
	 * matter what column is first.
	 */
	sep = "";

	if (options->show_offset) {
		printf("%-*s", (int)sizeof(buffer->offset), buffer->offset);
		sep = " ";
	}

	if (buffer->raw[0])	{
		printf("%s%-*s", sep, (int)sizeof(buffer->raw), buffer->raw);
		sep = " ";
	}

	if (buffer->payload.standard[0])
		printf("%s%-*s", sep, (int)sizeof(buffer->opcode), buffer->opcode);
	else
		printf("%s%s", sep, buffer->opcode);

	/* We printed at least one column.  From this point on, we don't need
	 * the separator any longer.
	 */

	if (buffer->use_ext_payload)
		printf(" %s", buffer->payload.extended);
	else if (buffer->tracking.id[0]) {
		printf(" %-*s", (int)sizeof(buffer->payload.standard), buffer->payload.standard);

		printf(" %-*s", (int)sizeof(buffer->tracking.id), buffer->tracking.id);
		printf("%s", buffer->tracking.payload);
	}
	else if (buffer->payload.standard[0])
		printf(" %s", buffer->payload.standard);

	printf("\n");
	return 0;
}

static int print_raw(struct ptdump_buffer *buffer, uint64_t offset,
					 const struct pt_packet *packet,
					 const struct pt_config *config) {
	const uint8_t *begin, *end;
	char *bbegin, *bend;
	int len;

	if (!buffer || !packet)
		return diag("error printing packet", offset, -pte_internal);

	begin = config->begin + offset;
	end = begin + packet->size;

	if (config->end < end)
		return diag("bad packet size", offset, -pte_bad_packet);

	bbegin = buffer->raw;
	bend = bbegin + sizeof(buffer->raw);

	for (; begin < end; ++begin, bbegin += len)	{
		size_t size = (size_t)((uintptr_t)bend - (uintptr_t)bbegin);

		len = snprintf(bbegin, size, "%02x", *begin);
		if (len != 2)
			return diag("truncating raw packet", offset, 0);
	}

	return 0;
}

static uint64_t sext(uint64_t val, uint8_t sign) {
	uint64_t signbit, mask;

	signbit = 1ull << (sign - 1);
	mask = ~0ull << sign;

	return val & signbit ? val | mask : val & ~mask;
}

static int print_ip_payload(struct ptdump_buffer *buffer, uint64_t offset,
							const struct pt_packet_ip *packet) {
	if (!buffer || !packet)
		return diag("error printing payload", offset, -pte_internal);

	switch (packet->ipc) {
	case pt_ipc_suppressed:
		print_field(buffer->payload.standard, "%x: ????????????????", pt_ipc_suppressed);
		return 0;

	case pt_ipc_update_16:
		print_field(buffer->payload.standard, "%x: ????????????%04" PRIx64, pt_ipc_update_16, packet->ip);
		return 0;

	case pt_ipc_update_32:
		print_field(buffer->payload.standard, "%x: ????????%08" PRIx64, pt_ipc_update_32, packet->ip);
		return 0;

	case pt_ipc_update_48:
		print_field(buffer->payload.standard, "%x: ????%012" PRIx64, pt_ipc_update_48, packet->ip);
		return 0;

	case pt_ipc_sext_48:
		print_field(buffer->payload.standard, "%x: %016" PRIx64, pt_ipc_sext_48, sext(packet->ip, 48));
		return 0;

	case pt_ipc_full:
		print_field(buffer->payload.standard, "%x: %016" PRIx64, pt_ipc_full, packet->ip);
		return 0;
	}

	print_field(buffer->payload.standard, "%x: %016" PRIx64, packet->ipc, packet->ip);
	return diag("bad ipc", offset, -pte_bad_packet);
}

static int print_tnt_payload(struct ptdump_buffer *buffer, uint64_t offset,
							 const struct pt_packet_tnt *packet) {
	uint64_t tnt;
	uint8_t bits;
	char *begin, *end;

	if (!buffer || !packet)
		return diag("error printing payload", offset, -pte_internal);

	bits = packet->bit_size;
	tnt = packet->payload;

	begin = buffer->payload.extended;
	end = begin + bits;

	if (sizeof(buffer->payload.extended) < bits) {
		diag("truncating tnt payload", offset, 0);

		end = begin + sizeof(buffer->payload.extended);
	}

	for (; begin < end; ++begin, --bits)
		*begin = tnt & (1ull << (bits - 1)) ? '!' : '.';

	return 0;
}

static const char *print_pwrx_wr(const struct pt_packet_pwrx *packet) {
	const char *wr;

	if (!packet)
		return "err";

	wr = NULL;
	if (packet->interrupt)
		wr = "int";

	if (packet->store) {
		if (wr)
			return NULL;
		wr = " st";
	}

	if (packet->autonomous) {
		if (wr)
			return NULL;
		wr = " hw";
	}

	if (!wr)
		wr = "bad";

	return wr;
}

static int print_packet(struct ptdump_buffer *buffer, uint64_t offset,
						const struct pt_packet *packet,
						struct ptdump_tracking *tracking,
						const struct ptdump_options *options,
						const struct pt_config *config) {
	if (!buffer || !packet || !tracking || !options)
		return diag("error printing packet", offset, -pte_internal);

	switch (packet->type) {
	case ppt_unknown:
		print_field(buffer->opcode, "<unknown>");
		return 0;

	case ppt_invalid:
		print_field(buffer->opcode, "<invalid>");
		return 0;

	case ppt_psb:
		print_field(buffer->opcode, "psb");
		tracking->in_header = 1;
		return 0;

	case ppt_psbend:
		print_field(buffer->opcode, "psbend");
		tracking->in_header = 0;
		return 0;

	case ppt_pad:
		print_field(buffer->opcode, "pad");
		return 0;

	case ppt_ovf:
		print_field(buffer->opcode, "ovf");
		return 0;

	case ppt_stop:
		print_field(buffer->opcode, "stop");
		return 0;

	case ppt_fup:
		print_field(buffer->opcode, "fup");
		print_ip_payload(buffer, offset, &packet->payload.ip);
		return 0;

	case ppt_tip:
		print_field(buffer->opcode, "tip");
		print_ip_payload(buffer, offset, &packet->payload.ip);
		return 0;

	case ppt_tip_pge:
		print_field(buffer->opcode, "tip.pge");
		print_ip_payload(buffer, offset, &packet->payload.ip);
		return 0;

	case ppt_tip_pgd:
		print_field(buffer->opcode, "tip.pgd");
		print_ip_payload(buffer, offset, &packet->payload.ip);
		return 0;

	case ppt_pip:
		print_field(buffer->opcode, "pip");
		print_field(buffer->payload.standard, "%" PRIx64 "%s", packet->payload.pip.cr3,	packet->payload.pip.nr ? ", nr" : "");
		print_field(buffer->tracking.id, "cr3");
		print_field(buffer->tracking.payload, "%016" PRIx64, packet->payload.pip.cr3);
		return 0;

	case ppt_vmcs:
		print_field(buffer->opcode, "vmcs");
		print_field(buffer->payload.standard, "%" PRIx64, packet->payload.vmcs.base);
		print_field(buffer->tracking.id, "vmcs");
		print_field(buffer->tracking.payload, "%016" PRIx64, packet->payload.vmcs.base);
		return 0;

	case ppt_tnt_8:
		print_field(buffer->opcode, "tnt.8");
		return print_tnt_payload(buffer, offset, &packet->payload.tnt);

	case ppt_tnt_64:
		print_field(buffer->opcode, "tnt.64");
		return print_tnt_payload(buffer, offset, &packet->payload.tnt);

	case ppt_mode:
	{
		const struct pt_packet_mode *mode;

		mode = &packet->payload.mode;
		switch (mode->leaf)	{
		case pt_mol_exec:
		{
			const char *csd, *csl, *sep;

			csd = mode->bits.exec.csd ? "cs.d" : "";
			csl = mode->bits.exec.csl ? "cs.l" : "";

			sep = csd[0] && csl[0] ? ", " : "";

			print_field(buffer->opcode, "mode.exec");
#if (LIBIPT_VERSION < 0x201)
			print_field(buffer->payload.standard, "%s%s%s",
						csd, sep, csl);
#else
			print_field(buffer->payload.standard, "%s%s%s%s%s",
						csd, sep, csl,
						mode->bits.exec.iflag &&
								(csd[0] || csl[0])
							? ", "
							: "",
						mode->bits.exec.iflag ? "if" : "");
#endif
		}
			return 0;

		case pt_mol_tsx:
		{
			const char *intx, *abrt, *sep;

			intx = mode->bits.tsx.intx ? "intx" : "";
			abrt = mode->bits.tsx.abrt ? "abrt" : "";

			sep = intx[0] && abrt[0] ? ", " : "";

			print_field(buffer->opcode, "mode.tsx");
			print_field(buffer->payload.standard, "%s%s%s", intx, sep, abrt);
		}
			return 0;
		}

		print_field(buffer->opcode, "mode");
		print_field(buffer->payload.standard, "leaf: %x", mode->leaf);

		return diag("unknown mode leaf", offset, 0);
	}

	case ppt_tsc:
		print_field(buffer->opcode, "tsc");
		print_field(buffer->payload.standard, "%" PRIx64, packet->payload.tsc.tsc);
		return 0;

	case ppt_cbr:
		print_field(buffer->opcode, "cbr");
		print_field(buffer->payload.standard, "%x",	packet->payload.cbr.ratio);
		return 0;

	case ppt_tma:
		print_field(buffer->opcode, "tma");
		print_field(buffer->payload.standard, "%x, %x",	packet->payload.tma.ctc, packet->payload.tma.fc);
		return 0;

	case ppt_mtc:
		print_field(buffer->opcode, "mtc");
		print_field(buffer->payload.standard, "%x",	packet->payload.mtc.ctc);
		return 0;

	case ppt_cyc:
		print_field(buffer->opcode, "cyc");
		print_field(buffer->payload.standard, "%" PRIx64, packet->payload.cyc.value);
		return 0;

	case ppt_mnt:
		print_field(buffer->opcode, "mnt");
		print_field(buffer->payload.standard, "%" PRIx64, packet->payload.mnt.payload);
		return 0;

	case ppt_exstop:
		print_field(buffer->opcode, "exstop");
		print_field(buffer->payload.standard, "%s",	packet->payload.exstop.ip ? "ip" : "");
		return 0;

	case ppt_mwait:
		print_field(buffer->opcode, "mwait");
		print_field(buffer->payload.standard, "%08x, %08x",	packet->payload.mwait.hints, packet->payload.mwait.ext);
		return 0;

	case ppt_pwre:
		print_field(buffer->opcode, "pwre");
		print_field(buffer->payload.standard, "c%u.%u%s",
					(packet->payload.pwre.state + 1) & 0xf,
					(packet->payload.pwre.sub_state + 1) & 0xf,
					packet->payload.pwre.hw ? ", hw" : "");
		return 0;

	case ppt_pwrx:
	{
		const char *wr;

		wr = print_pwrx_wr(&packet->payload.pwrx);
		if (!wr)
			wr = "bad";

		print_field(buffer->opcode, "pwrx");
		print_field(buffer->payload.standard, "%s: c%u, c%u", wr,
					(packet->payload.pwrx.last + 1) & 0xf,
					(packet->payload.pwrx.deepest + 1) & 0xf);
		return 0;
	}

	case ppt_ptw:
		print_field(buffer->opcode, "ptw");
		print_field(buffer->payload.standard, "%x: %" PRIx64 "%s",
					packet->payload.ptw.plc,
					packet->payload.ptw.payload,
					packet->payload.ptw.ip ? ", ip" : "");

		return 0;

#if (LIBIPT_VERSION >= 0x201)
	case ppt_cfe:
		print_field(buffer->opcode, "cfe");

		switch (packet->payload.cfe.type)
		{
		case pt_cfe_intr:
		case pt_cfe_vmexit_intr:
		case pt_cfe_uintr:
			print_field(buffer->payload.standard, "%u: %u%s",
						packet->payload.cfe.type,
						packet->payload.cfe.vector,
						packet->payload.cfe.ip ? ", ip" : "");
			return 0;

		case pt_cfe_sipi:
			print_field(buffer->payload.standard, "%u: %x%s",
						packet->payload.cfe.type,
						packet->payload.cfe.vector,
						packet->payload.cfe.ip ? ", ip" : "");
			return 0;

		case pt_cfe_iret:
		case pt_cfe_smi:
		case pt_cfe_rsm:
		case pt_cfe_init:
		case pt_cfe_vmentry:
		case pt_cfe_vmexit:
		case pt_cfe_shutdown:
		case pt_cfe_uiret:
			break;
		}

		print_field(buffer->payload.standard, "%u%s",
					packet->payload.cfe.type,
					packet->payload.cfe.ip ? ", ip" : "");
		return 0;

	case ppt_evd:
		print_field(buffer->opcode, "evd");
		print_field(buffer->payload.standard, "%u: %" PRIx64,
					packet->payload.evd.type,
					packet->payload.evd.payload);

		switch (packet->payload.evd.type) {
		case pt_evd_cr2:
		case pt_evd_vmxq:
		case pt_evd_vmxr:
			break;
		}

		return 0;

#endif /* (LIBIPT_VERSION >= 0x201) */
	}

	return diag("unknown packet", offset, -pte_bad_opc);
}

static int dump_one_packet(uint64_t offset, const struct pt_packet *packet,
						   struct ptdump_tracking *tracking,
						   const struct ptdump_options *options,
						   const struct pt_config *config) {
	struct ptdump_buffer buffer;
	int errcode;

	memset(&buffer, 0, sizeof(buffer));

	print_field(buffer.offset, "%016" PRIx64, offset);

	if (options->show_raw_bytes) {
		errcode = print_raw(&buffer, offset, packet, config);
		if (errcode < 0)
			return errcode;
	}

	errcode = print_packet(&buffer, offset, packet, tracking, options, config);
	if (errcode < 0)
		return errcode;

	return print_buffer(&buffer, offset, options);
}

static int dump_packets(struct pt_packet_decoder *decoder,
						struct ptdump_tracking *tracking,
						const struct ptdump_options *options,
						const struct pt_config *config) {
	uint64_t offset;
	int errcode;

	offset = 0ull;
	for (;;) {
		struct pt_packet packet;

		errcode = pt_pkt_get_offset(decoder, &offset);
		if (errcode < 0)
			return diag("error getting offset", offset, errcode);

		errcode = pt_pkt_next(decoder, &packet, sizeof(packet));
		if (errcode < 0) {
			if (errcode == -pte_eos)
				return 0;

			return diag("error decoding packet", offset, errcode);
		}

		errcode = dump_one_packet(offset, &packet, tracking, options, config);
		if (errcode < 0)
			return errcode;
	}
}

static int dump_sync(struct pt_packet_decoder *decoder,
					 struct ptdump_tracking *tracking,
					 const struct ptdump_options *options,
					 const struct pt_config *config) {
	int errcode;

	if (!options)
		return diag("setup error", 0ull, -pte_internal);

	errcode = pt_pkt_sync_forward(decoder);
	if (errcode < 0) {
		if (errcode == -pte_eos)
			return 0;

		return diag("sync error", 0ull, errcode);
	}

	for (;;) {
		errcode = dump_packets(decoder, tracking, options, config);
		if (!errcode)
			break;

		errcode = pt_pkt_sync_forward(decoder);
		if (errcode < 0) {
			if (errcode == -pte_eos)
				return 0;

			return diag("sync error", 0ull, errcode);
		}

		ptdump_tracking_reset(tracking);
	}

	return errcode;
}

static int dump(struct ptdump_tracking *tracking,
				const struct pt_config *config,
				const struct ptdump_options *options) {
	struct pt_packet_decoder *decoder;
	int errcode;

	decoder = pt_pkt_alloc_decoder(config);
	if (!decoder)
		return diag("failed to allocate decoder", 0ull, 0);

	errcode = dump_sync(decoder, tracking, options, config);

	pt_pkt_free_decoder(decoder);

	if (errcode < 0)
		return errcode;

	return 0;
}

static int process_args(int argc, char *argv[],
						struct ptdump_tracking *tracking,
						struct ptdump_options *options,
						struct pt_config *config, char **ptfile) {
	int idx, errcode;

	if (!argv || !tracking || !options || !config || !ptfile) {
		fprintf(stderr, "%s: internal error.\n", argv ? argv[0] : "");
		return -1;
	}

	for (idx = 1; idx < argc; ++idx) {
		if (strncmp(argv[idx], "-", 1) != 0) {
			*ptfile = argv[idx];
			if (idx < (argc - 1))
				return usage(argv[0]);
			break;
		}

		if (strcmp(argv[idx], "-h") == 0)
			return help(argv[0]);
		else if (strcmp(argv[idx], "--help") == 0)
			return help(argv[0]);
		else if (strcmp(argv[idx], "--raw") == 0)
			options->show_raw_bytes = 1;
		else
			return unknown_option_error(argv[idx], argv[0]);
	}

	return 0;
}

int main(int argc, char *argv[]) {
	struct ptdump_tracking tracking;
	struct ptdump_options options;
	struct pt_config config;
	int errcode;
	char *ptfile;
	uint64_t pt_offset, pt_size;

	/* 1. Intialize all variable */
	ptfile = NULL;

	memset(&options, 0, sizeof(options));
	options.show_offset = 1;

	memset(&config, 0, sizeof(config));
	pt_config_init(&config);

	ptdump_tracking_init(&tracking);

	/* 2. Extract arguments(ptfile, options) from command(argv) */
	errcode = process_args(argc, argv, &tracking, &options, &config, &ptfile);
	if (errcode != 0) {
		if (errcode > 0)
			errcode = 0;
		goto out;
	}

	if (!ptfile) {
		errcode = no_file_error(argv[0]);
		goto out;
	}

	/* 3. Extract range options(from, to) from commmand(argv) */
	errcode = preprocess_filename(ptfile, &pt_offset, &pt_size);
	if (errcode < 0) {
		fprintf(stderr, "%s: bad file %s: %s.\n", argv[0], ptfile,
				pt_errstr(pt_errcode(errcode)));
		goto out;
	}

	if (config.cpu.vendor) {
		errcode = pt_cpu_errata(&config.errata, &config.cpu);
		if (errcode < 0)
			diag("failed to determine errata", 0ull, errcode);
	}

	/* 4. Check ptfile is valid
	 *
	 * If valid, open ptfile and modify config
	 * If not, printf error
	 */
	errcode = load_pt(&config, ptfile, pt_offset, pt_size, argv[0]);
	if (errcode < 0)
		goto out;

	/* 5. Dump packet
	 *
	 * Dump packet and print them with defined format
	 */
	errcode = dump(&tracking, &config, &options);

out:
	free(config.begin);
	ptdump_tracking_fini(&tracking);

	return -errcode;
}