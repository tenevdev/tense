#include <linux/cpu.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/tense.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

// Name for file in debugfs with the tense interface
#define TENSE_NAME "tense"
static struct dentry *debugfs_file;

/* SECTION Parameters that can be set with command-line args to insmod */

static u8 sync_type = 0;
module_param(sync_type, byte, 0);
MODULE_PARM_DESC(sync_type, "what synchronisation algorithm to use");

static unsigned long sync_bound = 1000000;
module_param(sync_bound, ulong, 0);
MODULE_PARM_DESC(sync_bound, "don't allow processes which are ahead by more \
	than sync_bound from the current min_time process to run any further; \
	value is in nanoseconds and only applies if sync is enabled");

static unsigned long nops_per_ms = 500000;
module_param(nops_per_ms, ulong, 0);
MODULE_PARM_DESC(nops_per_ms, "how many iterations of the nop loop take \
	approx. 1 ms; this value is acquired through calibration");

static u8 log_level = 1;
module_param(log_level, byte, 0);
MODULE_PARM_DESC(log_level, "how verbose the logging should be");

/* SECTION Tense core */

/*
 * This struct holds function pointers to hooks that exist in the
 * scheduler of a modified Linux kernel that supports tense. It's used for
 * ease of development - recompile the module only if no actual changes to
 * the logic of the hooks are needed.
 */
extern struct tense_operations *tense;

static void init (void);

static void add_current_task (void);

static void remove_current_task(void);

static u64 update_curr (u64 delta_exec);

static void current_sleep(u64 duration);

static bool after_task_tick(struct task_struct *curr, struct list_head **tasks);

static void wake_up_sleepers(void);

static void set_current_tdf (u32 faster, u32 slower);

static u64 tense_current_time (void);

/* SECTION Macros to log information in hooks */

#define tense_log(level, format, ...) if (log_level >= level) \
	printk(KERN_DEBUG "tense: " format "\n", ## __VA_ARGS__)

#define tense_log_add_current_task() \
	tense_log(3,"add comm=%s pid=%d", current->comm, current->pid)

#define tense_log_set_current_tdf(f, s) \
	tense_log(3,"set tdf faster=%d slower=%d", f, s)

#define tense_log_current_time(mono_v, exec_v, exec_r) \
	tense_log(3, "mono_v=%llu exec_v=%llu exec_r=%llu", mono_v, exec_v, exec_r)

#define tense_log_remove_current_task() \
	tense_log(3,"remove comm=%s pid=%d", current->comm, current->pid)

#define tense_log_update_curr(se, delta) \
	tense_log(2, "update_curr sum_exec_runtime=%llu vruntime=%llu delta_exec=%llu", \
		se->sum_exec_runtime, se->vruntime, *delta)

#define tense_log_init_sleeper(se, expires) \
	tense_log(3,"init_sleeper vruntime=%llu expires=%llu", \
		se->vruntime, expires)

#define tense_log_current_schedstats() tense_log(2, \
	"[%llu] s:%llu e:%llu v:%llu S:%llu W:%llu I:%llu", \
	tense_time, \
	current->start_time, \
	current->se.sum_exec_runtime, \
	current->se.vruntime, \
	current->se.statistics.sum_sleep_runtime, \
	current->se.statistics.wait_sum, \
	current->se.statistics.iowait_sum)

/* SECTION Tense implementation */

// static u64 tense_time;

// per-cpu timeline
DEFINE_PER_CPU_SHARED_ALIGNED(u64, timelines);
#define tense_time this_cpu_read(timelines)

/*
 * Used for activation after sync
 *
 * per_cpu(cpu, cpu_wait) == cpu <==> the cpu is not waiting on any other cpu
 *
 * during sync checks if per(cpu, cpu_wait) == this_cpu then this cpu must check
 * if cpu still needs to wait and if not, set the value back to cpu
 */
DEFINE_PER_CPU_SHARED_ALIGNED(int, cpu_wait);
DEFINE_PER_CPU(bool, waiting);

/*
 * Used for determining cpu_tense_mask membership
 * inc/dec on:
 *  add/remove _current_task
 *  activate/deactivate _task (todo)
 */
DEFINE_PER_CPU(int, num_tense_tasks);

/*
 * Runable tense tasks per cpu
 *
 * this list needs to be managed by adding/removing a task on:
 *	add/remove _current_task
 *	activate/deactivate_task that is not due to tense sync
 *
 * there are subtle cases:
 *   (1) when a new task is added but the core is waiting the task is added to
 *       the list but we need to also deactivate it and schedule()
 *   (2) when a deactivated task is activated but the core is waiting the task
 *       is added to the list but we need to deactivate it; reasoning about this
 *       is very complicated because a task activation could happen in many
 *       contexts, so we don't try to deactivate for now; if it had to be done
 *       (i.e. there is a failing test case), then it would be best to deactivate
 *       from inside schedule() with a newly added hook to tense struct.
 *
 * When a sync needs to happen all tasks for a cpu are deactivated in
 * after_task_tick() by returning a handle to the list; the actual calls to
 * deactivate_task must happen in kernel code because the function is not
 * exported. Then, again in kernel code, resched_curr is called.
 *
 * Also in after_task_tick(), the cpu_wait value is set to the cpuid that is far
 * behind this cpu. A flag is_waiting is set so that this cpu knows that it
 * needs to poll for changes to cpu_wait.
 *
 * The next step of the sync algorithm happens when the other cpu detects it is
 * no longer far behind (in update_curr) and resets the cpu_wait value.
 *
 * Finally, in the original cpu in after_task_tick() we detect the change and
 * activate all tasks in the list. Again, activate_task is done in kernel code.
 * I'm not really sure after_task_tick() is called on an idle cpu, though.
 */
DEFINE_PER_CPU(struct list_head, tense_tasks);
DEFINE_PER_CPU(spinlock_t, tense_tasks_lock);

static LIST_HEAD(no_tasks);

/*
 * The time of a cpu with no runnable task should not be used for sync check
 *
 * holds cpus that have runnable tense tasks
 */
struct cpumask __cpu_tense_mask __read_mostly;
#define cpu_tense_mask ((const struct cpumask *)&__cpu_tense_mask)

#define cpu_tense(cpu) cpumask_test_cpu((cpu), cpu_tense_mask)
#define set_cpu_tense(cpu) cpumask_set_cpu(cpu, &__cpu_tense_mask)
#define clear_cpu_tense(cpu) cpumask_clear_cpu(cpu, &__cpu_tense_mask)

/*
 * Average speed of time
 *
 * t_(i+1) = (1 - weight) * t_i + weight * (faster / slower)
 *
 * In practice time_speed is shifted left by SHIFT to maintain precision, so
 *
 * st_(i+1) = ((1 - weight) << SHIFT) * (st_i >> SHIFT)
	      + (weight << SHIFT) * (faster / slower)
 *
 * where weight is defined to be delta_exec / 10 ms
 */
#define PRECISION 10
#define INT_SHIFT 23 // 8 388 608 ns as a shift
#define INTERVAL (1 << INT_SHIFT)

static u64 time_speed = 1 << PRECISION;

/*
 * Making tense_task->slower a power of 2 is an optimisation opportunity both
 * for this function and delta_exec scaling in other places. However, this is
 * probably best done at the library level (or the file_ops handlers).
 */
static inline void time_speed_update(u64 delta_exec, u64 faster, u64 slower)
{
	u64 update, decay;

	if (unlikely(delta_exec > INTERVAL)) {
		time_speed = (faster << PRECISION) / slower;
		return;
	}

	update = ((delta_exec * faster) << PRECISION) / (slower << INT_SHIFT);
	decay = ((INTERVAL - delta_exec) << PRECISION) >> INT_SHIFT;

	time_speed = ((decay * time_speed) >> PRECISION) + update;
}

static void init(void)
{
	int cpu;

	tense->update_curr = &update_curr;
	tense->after_task_tick = &after_task_tick;

	for_each_cpu(cpu, cpu_online_mask) {
		per_cpu(cpu_wait, cpu) = cpu;
		per_cpu(num_tense_tasks, cpu) = 0;
		spin_lock_init(per_cpu_ptr(&tense_tasks_lock, cpu));
		INIT_LIST_HEAD(per_cpu_ptr(&tense_tasks, cpu));
	}
}

static enum hrtimer_restart tense_wakeup_timer(struct hrtimer *timer)
{
	struct tense_task *task =
		container_of(timer, struct tense_task, wakeup_timer);

	// todo: check for early wakeup rel. to tense_time + delta from local
	// and restart if needed

	task->wakeup_time = U64_MAX;
	wake_up_process(task->task_struct);

	return HRTIMER_NORESTART;
}

static void add_current_task(void)
{
	struct tense_task *task;
	int cpu;

	// this makes sure cpu stays the same while we add the task
	spinlock_t *tasks_lock = &get_cpu_var(tense_tasks_lock);

	cpu = smp_processor_id();

	WARN_ON(cpu_tense(cpu) != (this_cpu_read(num_tense_tasks) == 0));

	this_cpu_inc(num_tense_tasks);

	if (!cpu_tense(cpu)) {
		// todo: 0 is not always accurate, it's hard to be accurate here
		this_cpu_write(timelines, 0);
		set_cpu_tense(cpu);
	}

	task = kmalloc(sizeof(*task), GFP_KERNEL);

	task->task_struct = current;

	task->wakeup_time = U64_MAX;
	hrtimer_init(&task->wakeup_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	task->wakeup_timer.function = tense_wakeup_timer;
	
	task->faster = 1;
	task->slower = 1;

	task->next_io_duration = 0;

	spin_lock(tasks_lock);
	list_add(&task->list, this_cpu_ptr(&tense_tasks));
	spin_unlock(tasks_lock);
	put_cpu_var(tense_tasks_lock);

	current->tense_task = task;

	tense_log_add_current_task(); 

	if (this_cpu_read(waiting)) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
}

static void remove_current_task(void)
{
	int cpu;
	struct tense_task *task = current->tense_task;
	spinlock_t *tasks_lock;

	if (!task)
		return;

	current->tense_task = NULL;

	tense_log_current_schedstats();
	tense_log_remove_current_task();

	tasks_lock = &get_cpu_var(tense_tasks_lock);
	spin_lock(tasks_lock);

	list_del(&task->list);

	spin_unlock(tasks_lock);
	put_cpu_var(tense_tasks_lock);

	if(!this_cpu_dec_return(num_tense_tasks)) {
		WARN_ON(!list_empty(this_cpu_ptr(&tense_tasks)));
		clear_cpu_tense(smp_processor_id());
	}

	//todo: if cpumask_empty the experiment is over.

	kfree(task);
}

#define scale(x, task) (((x) * (task->slower)) / (task->faster));
#define scale_inv(x, task) (((x) * (task->faster)) / (task->slower));

static u64 update_curr(u64 delta_exec)
{
	struct tense_task *task = current->tense_task;
	
	if (!task)
		return delta_exec;

	// Avg speed of time calculation (currently unused)
	// time_speed_update(delta_exec, task->faster, task->slower);

	delta_exec = scale(delta_exec, task);
	
	tense_log(2, "[%llu] (%d) update by %llu | ts %llu %u %u", tense_time,
		smp_processor_id(), delta_exec, time_speed, task->faster, task->slower);

	// tense_time += delta_exec;
	this_cpu_add(timelines, delta_exec);

	/*
	 * This is a call to update_curr from deactivate_task when the task is
	 * about to sleep. Add the last delta_exec to get accurate wakeup_time.
	 */
	if (task->wakeup_time != U64_MAX) {
		task->wakeup_time += delta_exec;
		tense_log(2, "[%llu] sleep + %llu to %llu", tense_time, delta_exec, task->wakeup_time);
	}

	return delta_exec;
}

static bool time_is_over_sync_bound(void)
{
	int cpu;
	s64 delta;

	tense_log(2, "this: %llu", this_cpu_read(timelines));
	for_each_cpu(cpu, cpu_tense_mask) {
		tense_log(2, "	cpu (%d): %llu", cpu, per_cpu(timelines, cpu));

		delta = this_cpu_read(timelines) - per_cpu(timelines, cpu);
		if (delta > sync_bound) {
			this_cpu_write(cpu_wait, cpu);
			return true;
		}
	}
	return false;
}

/*
 * This is a chance to do anything that needs to be atomic against schedule such
 * as waking up tasks. Interrupts are disabled.
 */
static bool after_task_tick(struct task_struct *curr, struct list_head **tasks)
{
	*tasks = &no_tasks;

	if (this_cpu_read(waiting) && this_cpu_read(cpu_wait) == smp_processor_id()) {
		WARN_ON(curr->tense_task);

		this_cpu_write(waiting, false);
		*tasks = this_cpu_ptr(&tense_tasks);
		return true;
	}

	if (!curr->tense_task)
		return false;

	tense_log(2, "after task tick called");

	// wake_up_sleepers();

	if (time_is_over_sync_bound()) {
		this_cpu_write(waiting, true);
		*tasks = this_cpu_ptr(&tense_tasks);
	}

	return false;
}

static u64 tense_current_time (void)
{
	return tense_time;
}

/*
 * Assumes current is a tense_task
 */
static void set_current_tdf (u32 faster, u32 slower) {
	tense_log_set_current_tdf(faster, slower);

	/* 
	 * Call schedule first so that update_curr uses the old scale.
	 * This will also make sure process times are updated.
	 */
	schedule();

	current->tense_task->faster = faster;
	current->tense_task->slower = slower;       
}

static void
current_sleep(u64 duration)
{
	struct tense_task *task = current->tense_task;

	task->wakeup_time = tense_time + duration;

	do {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	} while (task->wakeup_time != U64_MAX && !signal_pending(current));

	__set_current_state(TASK_RUNNING);

	// todo: what if woken up by a signal
}

static void wake_up_sleepers(void)
{
	tense_log(0, "wake_up_sleepers not implemented for multicore scenario");
}

/* SECTION File operations interface for tense. See libtense for user space */

/*
 * A process reads the file to get its current virtual time.
 */
ssize_t
read_tense(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	struct timespec64 kernel_tp;
	struct timespec *tp = (struct timespec *) buff;

	kernel_tp = ns_to_timespec64(tense_current_time());
	
	if(put_timespec64(&kernel_tp, tp))
		return -EFAULT;

	return 0;
}

/*
 * A process opens the file to start a tense experiment or join an existing one.
 * There can only be one experiment executing at a time.
 */
static int
open_tense(struct inode *inode, struct file *filp)
{
	add_current_task();

	// todo: error handling if current task cannot be added
	return 0;
}

/*
 * A process writes to the file to set its time dilation factor. Because there
 * are no floats in the kernel we use two integers - (faster, slower).
 */
static ssize_t
write_tense(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
	u32 faster, slower;
	faster = *(u32 *)buf;
	slower = *(((u32 *)buf) + 1);

	set_current_tdf(faster, slower);

	return count;
}

/*
 * Seeking is a bit weird. The file offset of the tense file is interpreted as
 * the time offset for the seeking process, or alternatively, the position of
 * this process on the virtual timeline. The returned value is tense_time after
 * the requested operation completes.
 *
 *  - SEEK_SET  - set vruntime to @offset
 *  - SEEK_CUR  - add @offset to vruntime; the current time-dilation applies
 *  - SEEK_DATA - use @offset as the predicted I/O time for the next blocking
 *                I/O operation; the operation itself doesn't run within this
 *                call
 *  - SEEK_HOLE - use @offset as the duration to sleep in virtual time starting
 *                right now; the current process is put to sleep immediately
 */
static loff_t
llseek_tense(struct file *filp, loff_t offset, int whence)
{
	switch(whence) {
	case SEEK_SET:
		current->se.vruntime = offset;
		schedule();
		break;
	case SEEK_CUR:
		current->se.vruntime += offset;
		update_curr(offset);
		schedule();
		break;
	case SEEK_END:
		return -EINVAL;
	case SEEK_DATA:
		current->tense_task->next_io_duration = offset;
		break;
	case SEEK_HOLE:
		tense_log(0, "sleep not implemented for multicore scenario");
		// current_sleep(offset);
		break;
	default:
		return -EINVAL;
	}

	return tense_time;
}

/*
 * A process closes the file to exit from the experiment. The last process to
 * exit an experiment causes the timing state to be reset so that a new
 * experiment can start as soon as another process joins in.
 */
static int
release_tense (struct inode *inode, struct file *filp)
{
	remove_current_task();
	return 0;
}

static const struct file_operations tense_fops = {
	.owner          = THIS_MODULE,
	.open           = open_tense,
	.read           = read_tense,
	.write          = write_tense,
	.llseek         = llseek_tense,
	.release        = release_tense,
};

static int __init
tense_init(void)
{
	init();
	
	debugfs_file = debugfs_create_file_unsafe(TENSE_NAME, 0666,
		NULL, /* place it in root of debugfs */
		NULL, /* private data is setup on open */
		&tense_fops);
	return 0;
}

static void __exit
tense_exit(void)
{
	// Set tense to do nothing
	tense_nop();

	debugfs_remove(debugfs_file);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tencho Tenev");
MODULE_DESCRIPTION("Nothing yet");

module_init(tense_init);
module_exit(tense_exit);
