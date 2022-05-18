#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <va/va.h>
#include <va/va_x11.h>
#include <assert.h>
#include<stdbool.h>

FILE *fd;
unsigned char ch;
long unsigned int pos;
long unsigned int prev;
long unsigned int size;

struct marker {
	unsigned char mr;
	const char *desc;
};

VAHuffmanTableBufferJPEGBaseline hft;
VAIQMatrixBufferJPEGBaseline iqm;
VAPictureParameterBufferJPEGBaseline pic_param;
VASliceParameterBufferJPEGBaseline slice_param;

struct marker markers[] = {
	{ 0xd8, "Start of Image" },
	{ 0xd9, "End of Image" },
	{ 0xda, "Start Of Scan" },
	{ 0xdb, "Quantization table" },
	{ 0xE0, "App specific" },
	{ 0xc4, "Huffman table" },
	{ 0xc0, "Start of Frame"},
	{ 0xfe, "Text Comment"},
};

int get_marker_string(char *s, unsigned char mr)
{
	int i;

	for (i = 0; i < sizeof(markers)/sizeof(markers[0]); i++) {
		if (markers[i].mr == mr)
			strcpy(s, markers[i].desc);
	}

	return 0;
}


int read_byte(unsigned char *ch)
{
	int r = 0;
	r = fread(ch, 1, 1, fd);
	if (r != 1)
		printf("failed to read a byte\n");
	pos = ftell(fd) - 1;
	return r;
}

int read_bytes(unsigned char *ch, unsigned int n)
{
	int r = 0;
	r = fread(ch, 1, n, fd);
	if (r != n)
		printf("failed to read %d bytes read %d\n", n, r);

	return r;
}

void reach_next_marker(void)
{
	do {
		read_byte(&ch);
		pos = ftell(fd) - 1;
	} while (ch != 0xff);
}

unsigned int width, height, num_components, format, sos_nr_components;

struct format {
	unsigned char sampler;
	char *name;
};

struct format formats[] = {
	{0x22, "YCbCr420"},
	{0x21, "YCbCr422"},
};

struct comp {
	char comp;
	unsigned char q_s;
	unsigned char xyf;
	unsigned char h_s;
};

struct comp components[3];

int get_format_string(char *s, unsigned char sampler)
{
	int i;

	for (i = 0; i < sizeof(formats)/sizeof(formats[0]); i++) {
		if (formats[i].sampler == sampler)
			strcpy(s, formats[i].name);
	}

	return 0;
}

struct table {
	unsigned char size;
	unsigned char coeff[128];
};

struct table quant_table[4];
struct table huffman_table[4];
unsigned num_quant;
unsigned num_huffman;
unsigned char *compressed_data;
long unsigned int file_size = 0;
long unsigned int data_size = 0;

int parse_marker_chunk(long unsigned int start, long unsigned int size)
{
	int i = 0, r;
	unsigned char *buf = (unsigned char *)malloc(size+16);
	char s[128];

	if (size < 1) {
		printf("Error unknown\n");
		exit(1);
	}

	fseek(fd, start, SEEK_SET);
	read_bytes(buf, size);

	switch(buf[1]) {
		case 0xd8:
			get_marker_string(s, 0xd8);
			printf("Parsing %s start=%lu size=%lu\n", s, start, size);
			break;
		case 0xd9:
			get_marker_string(s, 0xd9);
			printf("Parsing %s start=%lu size=%lu\n", s, start, size);
			break;
		case 0xda:
			get_marker_string(s, 0xda);
			data_size = file_size - start - 16;

			compressed_data = (unsigned char *)malloc(data_size+4096);
			if (!compressed_data) {
				printf("Failed to allocate memory for compressed data\n");
				free(buf);
				return 1;
			} else {
				fseek(fd, start+0xE, SEEK_SET);
				read_bytes(compressed_data, data_size);
			}

			printf("Parsing %s start=%lu size=%lu\n", s, start, data_size);
			for (i = 0; i < 32; i++)
				printf("%x ", buf[i]);
			printf("\n\n");
			for (i = 0; i < 16; i++)
				printf("%x ", compressed_data[i]);
			printf("-- %x %x %x\n",
					compressed_data[data_size-3],
					compressed_data[data_size-2],
					compressed_data[data_size-1]);
			printf("\n\n");

			sos_nr_components = buf[4];
			if (components[0].comp == buf[5])
				components[0].h_s = buf[0x6];
			if (components[1].comp == buf[7])
				components[1].h_s = buf[0x8];
			if (components[2].comp == buf[9])
				components[2].h_s = buf[0xa];

			free(buf);
			return 1;
			break;
		case 0xdb:
			get_marker_string(s, 0xdb);
			for (i = 0; i < size; i++)
				quant_table[num_quant].coeff[i] = buf[i];
			quant_table[num_quant].size = size;
			num_quant++;
			printf("Parsing %s start=%lu size=%lu num_quant:%d\n", s, start, size, num_quant);
			break;
		case 0xE0:
			get_marker_string(s, 0xe0);
			printf("Parsing %s start=%lu size=%lu\n", s, start, size);
			break;
		case 0xc0:
			get_marker_string(s, 0xc0);
			printf("Parsing %s start=%lu size=%lu\n", s, start, size);
			height = (buf[5] << 8 | buf[6]);
			width = (buf[7] << 8 | buf[8]);
			num_components = buf[9];
			format = buf[11];
			get_format_string(s, buf[11]);
			components[0].comp = buf[0xa];
			components[0].xyf = buf[0xb];
			components[0].q_s = buf[0xc];

			components[1].comp = buf[0xd];
			components[1].xyf = buf[0xE];
			components[1].q_s = buf[0xF];

			components[2].comp = buf[0x10];
			components[2].xyf = buf[0x11];
			components[2].q_s = buf[0x12];

			printf("%ux%u components=%u %s\n", width, height, num_components, s);
			for (i = 0; i < 3; i++)
				printf("componen[%d] sampling:%dx%d quantization_table:%d\n",
						components[i].comp, components[i].xyf >> 4, components[i].xyf & 0xf, components[i].q_s );
			break;
		case 0xc4:
			get_marker_string(s, 0xc4);
			for (i = 0; i < size; i++)
				huffman_table[num_huffman].coeff[i] = buf[i];
			huffman_table[num_huffman].size = size;
			num_huffman++;
			printf("Parsing %s start=%lu size=%lu num_huffman:%d\n", s, start, size, num_huffman);
			break;
		case 0xfe:
			get_marker_string(s, 0xfe);
			printf("Parsing %s start=%lu size=%lu\n", s, start, size);
			break;
		defaut:
			printf("Unknown marker\n");
	}

#define DEBUG
#if defined DEBUG
	for (i = 0; i < size; i++)
		printf("%x ", buf[i]);
	printf("\n\n");
#endif

	free(buf);
	return 0;
}

static void process_huffman_tabels()
{
   int i, k;
   unsigned dcac = 0;
   unsigned yc = 0;
	unsigned size = 0;
   printf("Process Huffman tables %d", num_huffman);
   for (i = 0; i < num_huffman; i++) {
      yc = (huffman_table[i].coeff[4] & 0xf);
      dcac = ((huffman_table[i].coeff[4] & 0xf0) >> 4);
      printf("\n%s %s hft[%d]  ", yc ? "C" : "Y", dcac ? "AC" : "DC", yc);
		hft.load_huffman_table[yc] = 1;
		if (!dcac) {
			size = sizeof(hft.huffman_table[yc].num_dc_codes);
			memcpy(hft.huffman_table[yc].num_dc_codes, huffman_table[i].coeff + 5, size);
			for (k = 0; k < size; k++)
				printf("%x ", hft.huffman_table[yc].num_dc_codes[k]);
			size = sizeof(hft.huffman_table[yc].dc_values);
			memcpy(hft.huffman_table[yc].dc_values, huffman_table[i].coeff + 5 + 16, size);
			for (k = 0; k < size; k++)
				printf("%x ", hft.huffman_table[yc].dc_values[k]);
		} else {
			size = sizeof(hft.huffman_table[yc].num_ac_codes);
			memcpy(hft.huffman_table[yc].num_ac_codes, huffman_table[i].coeff + 5, size);
			for (k = 0; k < size; k++)
				printf("%x ", hft.huffman_table[yc].num_ac_codes[k]);
			size = sizeof(hft.huffman_table[yc].ac_values);
			memcpy(hft.huffman_table[yc].ac_values, huffman_table[i].coeff + 5 + 16, size);
			for (k = 0; k < size; k++)
				printf("%x ", hft.huffman_table[yc].ac_values[k]);

		}
   }
	printf("\n");
}

static void process_quantization_tabels()
{
	int i, k;
	printf("\nProcess Quantization table %d\n", num_quant);
   for (i = 0; i < num_quant; i++) {
		iqm.load_quantiser_table[i] = 1;
		memcpy(&iqm.quantiser_table[i][0], &quant_table[i].coeff[5], 64);
		for (k = 0; k < 64; k++)
			printf("%2x ", iqm.quantiser_table[i][k]);
		printf("\n");
	}
	printf("\n");
}

static void process_picture_param()
{
	int i;

	printf("Process Picture params\n");
	pic_param.picture_width = width; 
	pic_param.picture_height = height;

	printf("Image %ux%u\n", pic_param.picture_width, pic_param.picture_height);
	for (i = 0; i < num_components; i++) {
		pic_param.components[i].component_id = components[i].comp;
		pic_param.components[i].h_sampling_factor = (components[i].xyf & 0xf0) >> 4;
		pic_param.components[i].v_sampling_factor = components[i].xyf & 0xf;
		pic_param.components[i].quantiser_table_selector = components[i].q_s; 
		printf("Component %d h:%d v:%d Quantization table:%d\n", pic_param.components[i].component_id,
				 pic_param.components[i].h_sampling_factor,
				 pic_param.components[i].v_sampling_factor,
				 pic_param.components[i].quantiser_table_selector);
	}
	printf("\n");
}

static void process_slice_param()
{
	int i;
	printf("Process Slice params\n");
	for (i = 0; i < sos_nr_components; i++) {
		slice_param.components[i].component_selector = components[i].comp;
		slice_param.components[i].dc_table_selector = (components[i].h_s & 0xf0) >> 4;
		slice_param.components[i].ac_table_selector = (components[i].h_s & 0xf);
		printf("Component %d Huffmantable DC:%d AC:%d\n", slice_param.components[i].component_selector,
				 slice_param.components[i].dc_table_selector,
				 slice_param.components[i].ac_table_selector);
	}
	printf("\n");
}

#define NUM_SURFACES 16
VABufferID hft_buf, iqm_buf, picparam_buf, sliceparam_buf, slicedata_buf;
VASurfaceID surfaces[NUM_SURFACES];
VAConfigID config;
VAContextID vacontext;
VAStatus vastatus;
VAEntrypoint entrypoints[5];
int major, minor;
VADisplay   vadpy;

/* Used from tinyjpeg libva-utils */
static Display *x11_display;

static VADisplay va_open_display(void)
{
    x11_display = XOpenDisplay(NULL);
    if (!x11_display) {
        printf("error: can't connect to X server!\n");
        return NULL;
    }
    return vaGetDisplay(x11_display);
}

static void va_close_display(VADisplay va_dpy)
{
    if (!x11_display)
        return;
    XCloseDisplay(x11_display);
    x11_display = NULL;
}

static int create_vaapi_codec_ctx()
{

	vadpy = va_open_display();
	/* Initialize the library */
	vastatus = vaInitialize(vadpy, &major, &minor);
	assert(vastatus == VA_STATUS_SUCCESS);

	return 0;
}

static int close_vaapi_codec_ctx()
{

	va_close_display(vadpy);
	/* all library internal resources will be cleaned up */
	vaTerminate(vadpy);
}

static char optstr[] = "pdf:";
bool decode = false;
bool parse = false;
char *file = NULL;
char *obj = NULL;

void usage()
{
	printf("%s -p [-d] -f sample_1920x1280.jpeg\n", obj);
	printf("\t-p parse\n");
	printf("\t-d decode\n");
	printf("\t-f filename\n");
	exit(1);
}


int main(int argc, char *argv[])
{
	unsigned int app_off, app_size;
	int k, num_chunks = 20, r = 0;
	int c;

	obj = argv[0];
	while ((c = getopt(argc, argv, optstr)) != -1) {
		switch (c) {
			case 'p':
				parse = true;
				break;
			case 'd':
				decode = true;
				break;
			case 'f':
				file = optarg;
				break;
			case '?':
			default:
				usage();
				break;
		}
	}

	if (file == NULL || !parse)
		usage();

	fd = fopen(file, "rb");
	if (!fd) {
		printf("failed to open input file %s\n", argv[1]);
		exit(1);
	}

	fseek(fd, 0, SEEK_END);
	file_size = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	for (k = 0; k < num_chunks; k++) {
		reach_next_marker();
		prev = pos;
		reach_next_marker();
		size = pos - prev;
		if (parse_marker_chunk(prev, size))
			break;
	}

   process_huffman_tabels();
   process_quantization_tabels();
	process_picture_param();
	process_slice_param();

	if (decode) {
		create_vaapi_codec_ctx();
		close_vaapi_codec_ctx();
	}

	if (compressed_data)
		free(compressed_data);
	fclose(fd);
}
