#include "../bridge-led-policy.h"
#include <assert.h>
int main(void)
{
	assert(bridge_color(0, 0, 0, 0, 0) == 0x000002);
	assert(bridge_color(1, 0, 0, 0, 100) != 0);
	assert(bridge_color(1, 1, 0, 0, 100) == 0x002000);
	assert(bridge_color(1, 0, 1, 0, 100) == 0x200010);
	assert(bridge_color(1, 1, 1, 0, 100) == 0x202010);
	assert(bridge_color(1, 1, 1, 1, 100) == 0x200000);
}
