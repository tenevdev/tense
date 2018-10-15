#include "../util/macros.h"

routine(virtual_work,
	jump(100)
	lock()
	unlock()
)

routine(real_work,
	run(100)
	lock()
	unlock()
)

main(
	spawn(virtual_work)
	spawn(real_work)

	join(virtual_work)
	join(real_work)
	
	time_range(200, 210)
	// real_time_lt(5)
)
