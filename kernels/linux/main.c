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

static void after_task_tick(struct task_struct *curr);

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

/* SECTION Tense single-core implementation */

static u64 tense_time;

// // per-cpu timeline
// DEFINE_PER_CPU(u64, timelines);

// // cpus that have runnable tense tasks on
// struct cpumask __cpu_tense_mask __read_mostly;
// #define cpu_tense_mask ((const struct cpumask *)&__cpu_tense_mask);

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

static LIST_HEAD(tense_tasks);
DEFINE_SPINLOCK(tense_tasks_lock);

static void init (void)
{
	tense_time = 0;
	tense->update_curr = &update_curr;
	tense->after_task_tick = &after_task_tick;
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


// static inline void
// set_cpu_tense(unsigned int cpu, bool is_tense)
// {
// 	if (is_tense)
// 		cpumask_set_cpu(cpu, &__cpu_tense_mask);
// 	else
// 		cpumask_clear_cpu(cpu, &__cpu_tense_mask);
// }

static void add_current_task(void)
{
	struct tense_task *task;
	// int cpuid = smp_processor_id();

	// if (!cpu_tense(cpuid)) {
	// 	this_cpu_write(timelines, tense_time);
	// 	set_cpu_tense(cpuid, true);
	// }

	task = kmalloc(sizeof(*task), GFP_KERNEL);

	task->task_struct = current;

	task->wakeup_time = U64_MAX;
	hrtimer_init(&task->wakeup_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	task->wakeup_timer.function = tense_wakeup_timer;
	
	task->faster = 1;
	task->slower = 1;

	task->next_io_duration = 0;

	spin_lock(&tense_tasks_lock);
	list_add(&task->list, &tense_tasks);
	spin_unlock(&tense_tasks_lock);

	current->tense_task = task;
	
	tense_log_add_current_task(); 
}

static void remove_current_task(void)
{
	struct tense_task *task = current->tense_task;

	if (!task)
		return;

	current->tense_task = NULL;

	tense_log_current_schedstats();
	tense_log_remove_current_task();
	
	spin_lock(&tense_tasks_lock);

	list_del(&task->list);
	if (list_empty(&tense_tasks))
		tense_time = 0;
	
	spin_unlock(&tense_tasks_lock);

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

	tense_time += delta_exec;

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

/*
 * This is a chance to do anything that needs to be atomic against schedule such
 * as waking up tasks. Interrupts are disabled.
 */
static void after_task_tick(struct task_struct *curr)
{
	if (!curr->tense_task)
		return;

	wake_up_sleepers();
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

#define latency 100000
#define tick 4000000
static void wake_up_sleepers(void)
{
	struct tense_task *task;
	ktime_t scaled_wakeup;

	spin_lock(&tense_tasks_lock);
	list_for_each_entry(task, &tense_tasks, list) {
		/*
		 * If timer is not active (0), process was already woken up.
		 * If timer is currently executing callback (-1), it will wake up.
		 * Otherwise it's our job to call wake_up_process here.
		 */
		if (task->wakeup_time < tense_time
			&& hrtimer_try_to_cancel(&task->wakeup_timer) == 1) {

			tense_log(2, "[%llu] forced wake up %s(%d) %li expected %llu",
				tense_time, task->task_struct->comm,
				task->task_struct->pid, task->task_struct->state,
				task->wakeup_time);

			WARN_ON(tense_time - task->wakeup_time > 10 * latency);

			task->wakeup_time = U64_MAX;
			wake_up_process(task->task_struct);
		}
		else if (task->wakeup_time != U64_MAX
			&& task->wakeup_time < tense_time + tick) {

			/* We need to prepare for the worst case here. That is,
			 * the current process runs for a whole tick without
			 * interruption. However, it's simple because we only
			 * need to account for constant time dilation (there
			 * will be an update on change) and we can cancel the
			 * timer.
			 */

			scaled_wakeup = task->wakeup_time - tense_time; 
			if (current->tense_task)
				scaled_wakeup = scale_inv(scaled_wakeup, current->tense_task);
			
			tense_log(2, "[%llu] set wake up %lli", tense_time, scaled_wakeup);

			hrtimer_start(&task->wakeup_timer, scaled_wakeup, HRTIMER_MODE_REL);
		}
	}
	spin_unlock(&tense_tasks_lock);
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
		current_sleep(offset);
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
