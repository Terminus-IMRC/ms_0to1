#include "libms.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static char *progname = NULL;

static void usage()
{
	printf("Usage: %s [OPTION]...\n", progname);
	printf("  -x, -X X      the number of the rows and of the columns\n");
	printf("  -0            convert 0-origin to 1-origin\n");
	printf("  -1            convert 1-origin to 0-origin\n");
	printf("  -i  FILE      input file\n");
	printf("  -o  FILE      output file\n");
	printf("  -n            input and output normal-formed squares (default)\n");
	printf("  -b            input and output binary squares\n");
	printf("  -h            input and output host-binary squares\n");
	printf("  -s  BUFSIZE   buffer size in a unit of a square (default:1000)\n");
	printf("  -?            show this usage\n");
}

int main(int argc, char *argv[])
{
	int i;
	int opt;

	int X_cmd = 0, add = 0;
	char *ifn = NULL, *ofn = NULL;
	enum stype {
		SQUARE_NORMAL, SQUARE_BINARY, SQUARE_HOST_BINARY
	};
	enum stype stype = SQUARE_NORMAL;

	ms_state_t st;
	int *ms;
	size_t bufsize = 1000;

	_Bool is_X_cmd_set = 0, is_add_set = 0, is_ifn_set = 0, is_ofn_set = 0, is_stype_set = 0;

	progname = argv[0];

	while ((opt = getopt(argc, argv, "x:X:01i:o:nbhs:?")) != -1) {
		switch (opt) {
			case 'x':
			case 'X':
				if (is_X_cmd_set) {
					fprintf(stderr, "error: X options are specified more than once\n");
					usage();
					exit(EXIT_FAILURE);
				}
				X_cmd = atoi(optarg);
				is_X_cmd_set = !0;
				break;
			case '0':
			case '1':
				if (is_add_set) {
					fprintf(stderr, "error: origin options are specified more than once\n");
					usage();
					exit(EXIT_FAILURE);
				}
				switch (opt) {
					case '0':
						add = 1;
						break;
					case '1':
						add = -1;
						break;
				}
				is_add_set = !0;
				break;
			case 'i':
				if (is_ifn_set) {
					fprintf(stderr, "error: input file option is specified more than once\n");
					usage();
					exit(EXIT_FAILURE);
				}
				ifn = strdup(optarg);
				is_ifn_set = !0;
				break;
			case 'o':
				if (is_ofn_set) {
					fprintf(stderr, "error: output file option is specified more than once\n");
					usage();
					exit(EXIT_FAILURE);
				}
				ofn = strdup(optarg);
				is_ofn_set = !0;
				break;
			case 'n':
			case 'b':
			case 'h':
				if (is_stype_set) {
					fprintf(stderr, "error: square type options are specified more than once\n");
					usage();
					exit(EXIT_FAILURE);
				}
				switch (opt) {
					case 'n':
						stype = SQUARE_NORMAL;
						break;
					case 'b':
						stype = SQUARE_BINARY;
						break;
					case 'h':
						stype = SQUARE_HOST_BINARY;
						break;
				}
				is_stype_set = !0;
				break;
			case 's':
				bufsize = atoi(optarg);
				printf("bufsize: %d\n", bufsize);
				break;
			case '?':
				usage();
				exit(EXIT_FAILURE);
			default:
				fprintf(stderr, "error: internal: option exception: -%c\n", opt);
				exit(EXIT_FAILURE);
		}
	}

	if (!is_X_cmd_set) {
		fprintf(stderr, "error: X options are not specified\n");
		usage();
		exit(EXIT_FAILURE);
	}
	if (!is_add_set) {
		fprintf(stderr, "error: origin options are not specified\n");
		usage();
		exit(EXIT_FAILURE);
	}
	if (!is_ifn_set) {
		fprintf(stderr, "error: input file option is not specified\n");
		usage();
		exit(EXIT_FAILURE);
	}
	if (!is_ofn_set) {
		fprintf(stderr, "error: output file option is not specified\n");
		usage();
		exit(EXIT_FAILURE);
	}

	ms_init(X_cmd, MS_ORIGIN_ONE, &st); /* MS_ORIGIN_ONE also supports zero value. I wish... */
	ms = ms_alloc(&st);

	switch (stype) {
		case SQUARE_NORMAL:
			{
				int ret;
				FILE *ifp, *ofp;
				char *str;
				const int str_size = 0x1000;

				ifp = fopen(ifn, "r");
				if (ifp == NULL) {
					fprintf(stderr, "error: fopen: %s: %s\n", ifn, strerror(errno));
					exit(EXIT_FAILURE);
				}
				ofp = fopen(ofn, "w");
				if (ofp == NULL) {
					fprintf(stderr, "error: fopen: %s: %s\n", ofn, strerror(errno));
					exit(EXIT_FAILURE);
				}
				str = malloc(str_size * sizeof(char));
				if (str == NULL) {
					fprintf(stderr, "error: failed to allocate str\n");
					exit(EXIT_FAILURE);
				}

				while (fgets(str, str_size, ifp) != NULL) {
					str_to_ms(ms, str, &st);
					for (i = 0; i < ms_Ceilings(&st); i ++)
						ms[i] += add;
					ms_to_str(str, ms, &st);
					ret = fputs(str, ofp);
					if (ret == EOF) {
						fprintf(stderr, "error: fputs returned EOF\n");
						exit(EXIT_FAILURE);
					}
					ret = putc('\n', ofp);
					if (ret == EOF) {
						fprintf(stderr, "error: putc returned EOF\n");
						exit(EXIT_FAILURE);
					}
				}
			}
			break;
		case SQUARE_BINARY:
		case SQUARE_HOST_BINARY:
			{
				ms_bin_seq_read_t mbsr;
				ms_bin_seq_read_flag_t flagr = MS_BIN_SEQ_READ_FLAG_NONE;
				ms_bin_seq_write_t mbsw;
				ms_bin_seq_write_flag_t flagw = MS_BIN_SEQ_WRITE_FLAG_CREAT | MS_BIN_SEQ_WRITE_FLAG_TRUNC;
				ms_bin_ret_t ret;

				if (stype == SQUARE_HOST_BINARY) {
					flagr |= MS_BIN_SEQ_READ_FLAG_HOST_WIDTH;
					flagw |= MS_BIN_SEQ_WRITE_FLAG_HOST_WIDTH;
				}
				ms_bin_seq_read_open(ifn, flagr, &mbsr, &st);
				ms_bin_seq_read_set_buffer(bufsize, &mbsr, &st);
				ms_bin_seq_write_open(ofn, flagw, &mbsw, &st);

				do {
					ret = ms_bin_seq_read_next(ms, &mbsr, &st);
					for (i = 0; i < ms_Ceilings(&st); i ++)
						ms[i] += add;
					ms_bin_seq_write_next(ms, &mbsw, &st);
				} while (ret != MS_BIN_RET_EOF);

				ms_bin_seq_read_close(&mbsr, &st);
				ms_bin_seq_write_close(&mbsw, &st);
			}
			break;
	}
	ms_free(ms, &st);
	ms_finalize(&st);
	free(ofn);
	free(ifn);

	return 0;
}
