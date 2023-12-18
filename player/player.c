#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <mad.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <id3.h>

pa_simple *device = NULL;

int ret = 1;
int error;

#define FIELD_BUFFER_SIZE 16

void output(struct mad_header const *header, struct mad_pcm *pcm);
void play(char* filename);
int print_meta(char* filename);

int main(int argc, char **argv) {
    
	if (argc < 2) {
        fprintf(stderr, "Usage: %s [filename.mp3] [filename.mp3] ...", argv[0]);
        return 255;
    }

    // Set up PulseAudio 16-bit 44.100kHz stereo output
    static const pa_sample_spec ss = { .format = PA_SAMPLE_S16LE, .rate = 44100, .channels = 2 };
    if (!(device = pa_simple_new(NULL, "player", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
        printf("pa_simple_new() failed\n");
        return 255;
    }

    for (int i = 1; i < argc; i++){
        print_meta(argv[i]);
        play(argv[i]);
    }

    if (device)
        pa_simple_free(device);

    return EXIT_SUCCESS;

}

void print_frame(char* name, ID3Frame* frame){
	char buffer[FIELD_BUFFER_SIZE];

    if(frame != NULL){		
		ID3Field *field = ID3Frame_GetField(frame, ID3FN_TEXT);
		int size = ID3Field_Size(field);
		ID3Field_GetASCII(field, buffer, size);
	   	printf("%s: ", name);
		printf(buffer);
		printf("\n");
	}

}

int print_meta(char* filename) {
	ID3Tag *tag = ID3Tag_New();

	printf("File Information\n");
	printf(" Name: ");
	printf(filename);
	ID3Tag_Link(tag, filename);	
	ID3Frame* frame = ID3Tag_FindFrameWithID(tag, ID3FID_TITLE);
	print_frame("\n Title", frame);
	frame = ID3Tag_FindFrameWithID(tag, ID3FID_ALBUM);
	print_frame(" Album", frame);
	frame = ID3Tag_FindFrameWithID(tag, ID3FID_PERFORMERSORTORDER);
	print_frame(" Performer", frame);
	frame = ID3Tag_FindFrameWithID(tag, ID3FID_COMMENT);
	print_frame(" Comments", frame);
}	

void play(char* filename) {

    // File pointer
    FILE *fp = fopen(filename, "r");
    int fd = fileno(fp);

    // Fetch file size, etc
    struct stat metadata;
    if (fstat(fd, &metadata) >= 0) {
        printf(" Size: %d Kbytes\n", (int)metadata.st_size / 1024);
    } else {
        printf("Failed to stat %s\n", filename);
        fclose(fp);
    }
	printf("Playing\n");
    
	char *input_stream = mmap(0, metadata.st_size, PROT_READ, MAP_SHARED, fd, 0);

    // Decode frame and synthesize loop
	struct mad_stream mad_stream;
	struct mad_frame mad_frame;
	struct mad_synth mad_synth;
    
	mad_stream_init(&mad_stream);
	mad_stream_buffer(&mad_stream, input_stream, metadata.st_size);
	
	// Initialize MAD library
	mad_synth_init(&mad_synth);
	mad_frame_init(&mad_frame);

    while (1) {
    
		// Decode frame from the stream
        if (mad_frame_decode(&mad_frame, &mad_stream)) {
            if (MAD_RECOVERABLE(mad_stream.error)) {
                continue;
            } else if (mad_stream.error == MAD_ERROR_BUFLEN) {
                continue;
            } else {
                break;
            }
        }

        // Synthesize PCM data of frame
        mad_synth_frame(&mad_synth, &mad_frame);

	    output(&mad_frame.header, &mad_synth.pcm);
    }
    
    fclose(fp);
	mad_stream_finish(&mad_stream);
	mad_synth_finish(&mad_synth);
	mad_frame_finish(&mad_frame);
}

int scale(mad_fixed_t sample) {
     sample += (1L << (MAD_F_FRACBITS - 16));
     if (sample >= MAD_F_ONE)
         sample = MAD_F_ONE - 1;
     else if (sample < -MAD_F_ONE)
         sample = -MAD_F_ONE;
     return sample >> (MAD_F_FRACBITS + 1 - 16);
}


void output(struct mad_header const *header, struct mad_pcm *pcm) {
    register int nsamples = pcm->length;
    mad_fixed_t const *left_ch = pcm->samples[0], *right_ch = pcm->samples[1];
    static char stream[1152 * 4];
    while (nsamples--) {
        signed int sample;
        sample = scale(*left_ch++);
        stream[(pcm->length-nsamples) * 4 ] = ((sample >> 0) & 0xff);
        stream[(pcm->length-nsamples) * 4 + 1] = ((sample >> 8) & 0xff);
        sample = scale(*right_ch++);
        stream[(pcm->length-nsamples) * 4 + 2 ] = ((sample >> 0) & 0xff);
        stream[(pcm->length-nsamples) * 4 + 3] = ((sample >> 8) & 0xff);
    }
    if (pa_simple_write(device, stream, (size_t)1152 * 4, &error) < 0) {
        fprintf(stderr, "pa_simple_write() failed: %s\n", pa_strerror(error));
        return;
    }
}
