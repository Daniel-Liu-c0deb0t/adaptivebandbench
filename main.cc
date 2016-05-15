
/**
 * @file main.cc
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include "util.h"
#include "full.h"

#include "bench.h"


int blast_linear(
	void *work,
	char const *a,
	uint64_t alen,
	char const *b,
	uint64_t blen,
	int8_t *score_matrix, int8_t ge, int16_t xt);
	// int8_t m, int8_t x, int8_t gi, int8_t ge, int16_t xt);
int blast_affine(
	void *work,
	char const *a,
	uint64_t alen,
	char const *b,
	uint64_t blen,
	int8_t *score_matrix, int8_t gi, int8_t ge, int16_t xt);
	// int8_t m, int8_t x, int8_t gi, int8_t ge, int16_t xt);

int simdblast_linear(
	void *work,
	char const *a,
	uint64_t alen,
	char const *b,
	uint64_t blen,
	int8_t *score_matrix, int8_t ge, int16_t xt);
	// int8_t m, int8_t x, int8_t gi, int8_t ge, int16_t xt);
int simdblast_affine(
	void *work,
	char const *a,
	uint64_t alen,
	char const *b,
	uint64_t blen,
	int8_t *score_matrix, int8_t gi, int8_t ge, int16_t xt);
	// int8_t m, int8_t x, int8_t gi, int8_t ge, int16_t xt);

int ddiag_linear(
	void *work,
	char const *a,
	uint64_t alen,
	char const *b,
	uint64_t blen,
	int8_t *score_matrix, int8_t ge, int16_t xt);
	// int8_t m, int8_t x, int8_t gi, int8_t ge, int16_t xt);
int ddiag_affine(
	void *work,
	char const *a,
	uint64_t alen,
	char const *b,
	uint64_t blen,
	int8_t *score_matrix, int8_t gi, int8_t ge, int16_t xt);
	// int8_t m, int8_t x, int8_t gi, int8_t ge, int16_t xt);



/* wrapper of Myers' wavefront algorithm */
extern "C" {
	#include "wave/align.h"

	int enlarge_vector(Work_Data *work, int newmax);
	int enlarge_points(Work_Data *work, int newmax);
	int forward_wave(Work_Data *work, Align_Spec *spec, Alignment *align, Path *bpath,
	                        int *mind, int maxd, int mida, int minp, int maxp);
	int reverse_wave(Work_Data *work, Align_Spec *spec, Alignment *align, Path *bpath,
	                        int mind, int maxd, int mida, int minp, int maxp);
}

struct wavefront_work_s {
	Work_Data *w;
	Align_Spec *s;
	Path apath, bpath;
};

struct wavefront_work_s *wavefront_init_work(void)
{
	struct wavefront_work_s *work = (struct wavefront_work_s *)malloc(sizeof(struct wavefront_work_s));

	Work_Data *w = New_Work_Data();
	enlarge_vector(w, 10000 * (6 * sizeof(int) + sizeof(uint64_t)));
	enlarge_points(w, 4 * 4096 / 100 * sizeof(uint16_t) + sizeof(Path));


	float freq[4] = { 0.25, 0.25, 0.25, 0.25 };
	Align_Spec *s = New_Align_Spec(0.7, 100, freq);


	work->w = w;
	work->s = s;
	work->apath.trace = malloc(4096);
	work->bpath.trace = malloc(4096);

	return(work);
}


void wavefront_clean_work(struct wavefront_work_s *_work)
{
	struct wavefront_work_s *work = (struct wavefront_work_s *)_work;

	free(work->apath.trace);
	free(work->bpath.trace);

	Free_Align_Spec(work->s);
	Free_Work_Data(work->w);

	free(work);
	return;	
}

int 
wavefront(
	void *_work,
	char const *a,
	uint64_t alen,
	char const *b,
	uint64_t blen)
{
	struct wavefront_work_s *work = (struct wavefront_work_s *)_work;
	Work_Data *w = work->w;
	Align_Spec *s = work->s;

	Alignment aln;
	aln.path = &work->apath;
	aln.flags = 0;
	aln.aseq = (char *)a;
	aln.bseq = (char *)b;
	aln.alen = alen;
	aln.blen = blen;

	int low = 0;
	int high = 0;

	forward_wave(w, s, &aln, &work->bpath, &low, high, 0, -INT32_MAX, INT32_MAX);
	return(0);
}




/**
 * random sequence generator, modifier.
 * rseq generates random nucleotide sequence in ascii,
 * mseq takes ascii sequence, modifies the sequence in given rate.
 */
static char rbase(void)
{
	switch(rand() % 4) {
		case 0: return 'A';
		case 1: return 'C';
		case 2: return 'G';
		case 3: return 'T';
		default: return 'A';
	}
}

char *rseq(int len)
{
	int i;
	char *seq;
	seq = (char *)malloc(sizeof(char) * (len + 1));

	for(i = 0; i < len; i++) {
		seq[i] = rbase();
	}
	seq[len] = '\0';
	return(seq);
}

char *mseq(char const *seq, int x, int ins, int del)
{
	int i;
	int len = strlen(seq);
	char *mod, *ptr;
	mod = (char *)malloc(sizeof(char) * 2 * (len + 1));

	ptr = mod;
	for(i = 0; i < len; i++) {
		if(rand() % x == 0) { *ptr++ = rbase(); }
		else if(rand() % ins == 0) { *ptr++ = rbase(); i--; }
		else if(rand() % del == 0) { /* skip a base */ }
		else { *ptr++ = seq[i]; }
	}
	*ptr = '\0';
	return(mod);
}

int print_msg(int flag, char const *fmt, ...)
{
	int r = 0;
	if(flag == 0) {
		va_list l;
		va_start(l, fmt);
		r = vfprintf(stdout, fmt, l);
		va_end(l);
	}
	return(r);
}

int print_bench(int flag, char const *name, int64_t l, int64_t a, int64_t sl, int64_t sa)
{
	if(flag == 0) {
		printf("%s\t%lld\t%lld\t%lld\t%lld\n", name, l / 1000, a / 1000, sl, sa);
	} else if(flag == 1) {
		printf("%lld\t%lld\t", l / 1000, a / 1000);
	} else if(flag == 2) {
		printf("%lld\t", a / 1000);
	}
	return(0);
}

int main(int argc, char *argv[])
{
	int flag = 0;
	if(argc > 1 && strcmp(argv[1], "-s") == 0) {
		flag = 1;
		argc--; argv++;
	} else if(argc > 1 && strcmp(argv[1], "-a") == 0) {
		flag = 2;
		argc--; argv++;
	}

	int i;
	int const m = 1, x = -1, gi = -1, ge = -1;
	int const xt = 30;
	char *a, *b, *at, *bt;
	int len = (argc > 1) ? atoi(argv[1]) : 1000;
	int cnt = (argc > 2) ? atoi(argv[2]) : 1000;
	bench_t bl, ba, sl, sa, ddl, dda, wl;
	volatile int64_t sbl = 0, sba = 0, ssl = 0, ssa = 0, sddl = 0, sdda = 0;
	struct timeval tv;

	int8_t score_matrix[16] __attribute__(( aligned(16) ));
	build_score_matrix(score_matrix, m, x);

	gettimeofday(&tv, NULL);
	unsigned long s = (argc > 3) ? atoi(argv[3]) : tv.tv_usec;
	srand(s);
	print_msg(flag, "%lu\n", s);

	a = rseq(len * 9 / 10);
	b = mseq(a, 10, 40, 40);
	at = rseq(len / 10);
	bt = rseq(len / 10);
	a = (char *)realloc(a, 2*len); strcat(a, at); free(at);
	b = (char *)realloc(b, 2*len); strcat(b, bt); free(bt);
	print_msg(flag, "%p, %p\n", a, b);

	// print_msg(flag, "%s\n%s\n", a, b);

	print_msg(flag, "len:\t%d\ncnt:\t%d\n", len, cnt);
	print_msg(flag, "m: %d\tx: %d\tgi: %d\tge: %d\n", m, x, gi, ge);
	print_msg(flag, "alg\tlinear\taffine\tsc(l)\tsc(a)\n");


	/* malloc work */
	void *work = aligned_malloc(1024 * 1024 * 1024, sizeof(__m128i));


	/* blast */
	bench_init(bl);
	bench_init(ba);
	bench_start(bl);
	for(i = 0; i < cnt; i++) {
		sbl += blast_linear(work, a, strlen(a), b, strlen(b), score_matrix, ge, xt);
	}
	bench_end(bl);
	bench_start(ba);
	for(i = 0; i < cnt; i++) {
		sba += blast_affine(work, a, strlen(a), b, strlen(b), score_matrix, gi, ge, xt);
	}
	bench_end(ba);
	print_bench(flag, "blast", bench_get(bl), bench_get(ba), sbl, sba);

	/* simdblast */
	bench_init(sl);
	bench_init(sa);
	bench_start(sl);
	for(i = 0; i < cnt; i++) {
		ssl += simdblast_linear(work, a, strlen(a), b, strlen(b), score_matrix, ge, xt);
	}
	bench_end(sl);
	bench_start(sa);
	for(i = 0; i < cnt; i++) {
		ssa += simdblast_affine(work, a, strlen(a), b, strlen(b), score_matrix, gi, ge, xt);
	}
	bench_end(sa);
	print_bench(flag, "simdblast", bench_get(sl), bench_get(sa), ssl, ssa);


	/* adaptive banded */
	bench_init(ddl);
	bench_init(dda);
	bench_start(ddl);
	for(i = 0; i < cnt; i++) {
		sddl += ddiag_linear(work, a, strlen(a), b, strlen(b), score_matrix, ge, xt);
	}
	bench_end(ddl);
	bench_start(dda);
	for(i = 0; i < cnt; i++) {
		sdda += ddiag_affine(work, a, strlen(a), b, strlen(b), score_matrix, gi, ge, xt);
	}
	bench_end(dda);
	print_bench(flag, "aband", bench_get(ddl), bench_get(dda), sddl, sdda);


	free(work);


	/* wavefront */
	bench_init(wl);
	if(len >= 1000) {
		/* wavefront algorithm fails to align sequences shorter than 1000 bases */

		struct wavefront_work_s *wwork = wavefront_init_work();

		bench_start(wl);
		for(i = 0; i < cnt; i++) {
			wavefront(wwork, a, strlen(a), b, strlen(b));
		}
		bench_end(wl);
	}
	print_bench(flag, "wavefront", bench_get(wl), bench_get(wl), 0, 0);


	if(flag != 0) {
		printf("\n");
	}
	free(a);
	free(b);
	return 0;
}

/**
 * end of main.cc
 */
