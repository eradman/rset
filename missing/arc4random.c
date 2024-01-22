#include <inttypes.h>
#include <sys/random.h>

uint32_t
arc4random(void)
{
	uint32_t val;
	getrandom(&val, sizeof val, GRND_RANDOM);

	return val;
}
