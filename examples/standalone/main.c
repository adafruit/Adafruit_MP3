#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mp3dec.h"

#define MAX_BUFFER_LEN (MAX_NSAMP * MAX_NGRAN * MAX_NCHAN)

int16_t audiodata[MAX_BUFFER_LEN];

typedef struct {
    char *ptr, *end;
} stream;
#define READ_PTR(stream) ((void*)((stream)->ptr))
#define BYTES_LEFT(stream) ((stream)->end - (stream)->ptr)
#define CONSUME(stream, n) ((stream)->ptr += n)

void skip_id3v2(stream* self) {
    if (BYTES_LEFT(self) < 10) {
        return;
    }
    uint8_t *data = READ_PTR(self);
    if (!(
            data[0] == 'I' &&
            data[1] == 'D' &&
            data[2] == '3' &&
            data[3] != 0xff &&
            data[4] != 0xff &&
            (data[5] & 0x1f) == 0 &&
            (data[6] & 0x80) == 0 &&
            (data[7] & 0x80) == 0 &&
            (data[8] & 0x80) == 0 &&
            (data[9] & 0x80) == 0)) {
        return;
    }
    uint32_t size = (data[6] << 21) | (data[7] << 14) | (data[8] << 7) | (data[9]);
    size += 10; // size excludes the "header" (but not the "extended header")
    CONSUME(self, size + 10);
}

bool mp3file_find_sync_word(stream* self) {
    int offset = MP3FindSyncWord(READ_PTR(self), BYTES_LEFT(self));
    if (offset >= 0) {
        CONSUME(self, offset);
        return true;
    }
    return false;
}

void fatal(const char *msg) { fprintf(stderr, "%s\n", msg); exit(1); }
void perror_fatal(const char *msg) { perror(msg); exit(1); }

bool probable_overflow(int16_t a, int16_t b) {
    if(a > 32700 && b < -32700) {
        return true;
    } else if(b > 32700 && a < -32700) {
        return true;
    } else {
        return false;
    }
}

void look_for_overflow(int16_t *ptr, size_t os, int frame) {
    for(size_t i=2; i<os; i+=2) {
        int16_t l_old = ptr[i-2];
        int16_t l_new = ptr[i];
        int16_t r_old = ptr[i-1];
        int16_t r_new = ptr[i+1];

        if(probable_overflow(l_old, l_new)) {
            printf("probable overflow, left  channel,  frame %5d sample %5d, %5d\n", frame, (i/2)-1, i/2);
            printf("Consecutive sample values: %5d %5d\n", l_old, l_new);
        }
        if(probable_overflow(r_old, r_new)) {
            printf("probable overflow, right channel,  frame %5d sample %5d, %5d\n", frame, (i/2)-1, i/2);
            printf("Consecutive sample values: %5d %5d\n", r_old, r_new);
        }
    }
}

int main(int argc, char **argv) {
    if(argc != 3) {
        fprintf(stderr, "Decode MP3 into headerless LE16 stereo\n");
        fprintf(stderr, "Usage: %s input.mp3 output.bin\n", argv[0]);
        exit(99);
    }

    HMP3Decoder decoder = MP3InitDecoder();

    FILE *fi = fopen(argv[1], "rb");
    if(!fi) perror_fatal("open");

    struct stat st;
    if(fstat(fileno(fi), &st) < 0) perror_fatal("fstat");

    stream s;
    s.ptr = malloc(st.st_size);
    if(!s.ptr) perror_fatal("malloc");
    s.end = s.ptr + st.st_size;

    if(fread(s.ptr, 1, st.st_size, fi) != st.st_size) perror_fatal("fread");

    FILE *fo = fopen(argv[2], "wb");
    if(!fo) perror_fatal("open");
    
    skip_id3v2(&s);

    int frame=0;
    while(mp3file_find_sync_word(&s)) {
        MP3FrameInfo fi;
        int err = MP3GetNextFrameInfo(decoder, &fi, READ_PTR(&s));
        if(err != ERR_MP3_NONE) fatal("MP3GetNextFrameInfo");
        int bytes_left = (int)BYTES_LEFT(&s);
        uint8_t *inbuf = READ_PTR(&s);
        err = MP3Decode(decoder, &inbuf, &bytes_left,
                audiodata, 0);
        if(err != ERR_MP3_NONE) fatal("MP3Decode");
        look_for_overflow(audiodata, fi.outputSamps, ++frame);
        if(fwrite(audiodata, 1, fi.outputSamps*sizeof(int16_t), fo)
                != fi.outputSamps*sizeof(int16_t))
            perror_fatal("fwrite");
        CONSUME(&s, BYTES_LEFT(&s) - bytes_left); 
    }

    return 0;
}
