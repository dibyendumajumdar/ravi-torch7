#ifndef TH_GENERATOR
#define TH_GENERATOR

#include <mutex>
#include "THRandom.h"

/* A THGenerator contains all the state required for a single random number stream */
struct THGenerator {
	THGeneratorState gen_state;
	/* The mutex is put at the end (unlike Aten) so that layout is backward
	   compatible */
	std::mutex mutex; /* mutex for using this generator */ 
};

#endif
