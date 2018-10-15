#include "../util/macros.h"

routine(child,
	lock()
	unlock()
)

main(
	lock()
	spawn(child)
	jump(100)
	unlock()
	join(child)
	time_range(100, 105)
	// real_time_lt(5)
)
